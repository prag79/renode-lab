//
// SimpleTimer.cs - a hand-written Renode peripheral ("IP model").
//
// A minimal 32-bit memory-mapped periodic timer with an interrupt
// output. It is the kind of block you'd find on any real SoC: load a
// period, enable it, and it raises an IRQ every period until the CPU
// acknowledges it.
//
// Register map (offsets from the peripheral base):
//   0x00 CONTROL   bit0 ENABLE, bit1 IRQ_ENABLE
//   0x04 RELOAD    32-bit period, in timer ticks
//   0x08 COUNTER   32-bit current count (read-only)
//   0x0C STATUS    bit0 PENDING (read; write 1 to clear)
//
// Renode compiles this file on the fly when the .resc does
// `i @peripherals/SimpleTimer.cs`. After that, the platform .repl can
// instantiate it as `Timers.SimpleTimer`.
//

/// @file SimpleTimer.cs
/// @brief Minimal 32-bit periodic timer peripheral model for Renode.
///
/// This file demonstrates the full lifecycle of a Renode peripheral:
/// bus interface, register declaration via the Register Framework,
/// use of the built-in `LimitTimer` primitive for time-keeping, and
/// GPIO-backed interrupt output that the CPU platform binds into the
/// interrupt controller in the .repl file.

// ---------------------------------------------------------------------
// Renode core primitives used throughout the class.
// ---------------------------------------------------------------------

/// @brief Core types: `IMachine`, `GPIO`, base peripheral interfaces.
using Antmicro.Renode.Core;

/// @brief Register Framework: `DoubleWordRegisterCollection`,
///        `IFlagRegisterField`, `IValueRegisterField`, `FieldMode`,
///        etc. Declarative register modelling avoids hand-written
///        switch/case decoders.
using Antmicro.Renode.Core.Structure.Registers;

/// @brief Structured logging (`this.Log(LogLevel.Noisy, ...)`).
using Antmicro.Renode.Logging;

/// @brief Base peripheral marker interfaces (`IPeripheral`,
///        `IKnownSize`, etc.).
using Antmicro.Renode.Peripherals;

/// @brief System-bus abstractions: `IDoubleWordPeripheral` for 32-bit
///        word-granular memory-mapped I/O.
using Antmicro.Renode.Peripherals.Bus;

/// @brief `LimitTimer` — the reusable Renode primitive that owns the
///        virtual clock and fires a callback when the count reaches
///        its limit.
using Antmicro.Renode.Peripherals.Timers;

/// @brief `WorkMode` (`Periodic`, `OneShot`, ...) lives here, not in
///        `Peripherals.Timers`. Omitting this `using` yields CS0103
///        when Renode compiles the file via `i @peripherals/...`.
using Antmicro.Renode.Time;

/// @namespace Antmicro.Renode.Peripherals.Timers
/// @brief Home namespace for timer-family peripherals. Placing the
///        class here keeps discovery consistent with Renode's built-in
///        timers (e.g. the STM32 basic timer, HiFive CLINT, ...).
namespace Antmicro.Renode.Peripherals.Timers
{
    // IDoubleWordPeripheral  -> 32-bit bus accesses
    // IProvidesRegisterCollection -> lets us use the Register Framework
    // IKnownSize             -> we advertise our footprint on the bus

    /// @class SimpleTimer
    /// @brief 32-bit periodic timer with maskable interrupt.
    ///
    /// The class implements three Renode contracts:
    /// - `IDoubleWordPeripheral`               — routes 32-bit word bus
    ///   accesses (`ReadDoubleWord`/`WriteDoubleWord`) to the Register
    ///   Framework.
    /// - `IProvidesRegisterCollection<DoubleWordRegisterCollection>` —
    ///   lets the framework auto-generate register decode/dispatch and
    ///   handles reset semantics.
    /// - `IKnownSize`                          — publishes the size of
    ///   the peripheral's aperture on the system bus so the .repl can
    ///   pin it to an address range at instantiation time.
    public class SimpleTimer : IDoubleWordPeripheral, IProvidesRegisterCollection<DoubleWordRegisterCollection>, IKnownSize
    {
        /// @brief Construct the peripheral and wire up its time base.
        ///
        /// Called by Renode when the .repl instantiates the block.
        /// The .repl passes the owning `IMachine`; the optional
        /// `frequency` (Hz) sets how fast the internal counter ticks.
        ///
        /// @param machine    The machine this peripheral belongs to.
        ///                   Injected automatically by Renode from the
        ///                   .repl declaration; also gives us access
        ///                   to the shared `ClockSource`.
        /// @param frequency  Timer tick rate in Hz. Defaults to 1 MHz
        ///                   (`DefaultFrequency`) if the .repl doesn't
        ///                   override it.
        public SimpleTimer(IMachine machine, long frequency = DefaultFrequency)
        {
            /// @brief Interrupt output line.
            ///
            /// A `GPIO` in Renode is a single-bit signal used both for
            /// GPIO pins and for interrupt lines. The .repl file
            /// connects `IRQ` to a slot on the interrupt controller
            /// (e.g. `simpletimer -> nvic@10`).
            IRQ = new GPIO();

            // The actual time-keeping is delegated to a LimitTimer driven by
            // the machine clock. WorkMode.Periodic makes it auto-reload, so it
            // fires LimitReached once per period.

            /// @brief Underlying time base.
            ///
            /// `LimitTimer` counts up on the machine's virtual clock at
            /// `frequency` ticks per second. When the count hits
            /// `limit`, it raises the `LimitReached` event and (thanks
            /// to `WorkMode.Periodic`) auto-reloads from zero.
            /// `uint.MaxValue` is a placeholder — the guest firmware
            /// will overwrite it via the RELOAD register.
            /// `eventEnabled: true` allows the callback to fire.
            innerTimer = new LimitTimer(machine.ClockSource, frequency, this, "simpletimer",
                limit: uint.MaxValue, workMode: WorkMode.Periodic, eventEnabled: true);

            /// @brief Subscribe to end-of-period notifications from the
            ///        time base. `OnLimitReached` sets PENDING and
            ///        drives the IRQ line if enabled.
            innerTimer.LimitReached += OnLimitReached;

            /// @brief Owning register collection for this peripheral.
            ///        The framework will route bus accesses to whichever
            ///        register was defined at the incoming offset.
            RegistersCollection = new DoubleWordRegisterCollection(this);

            /// @brief Declare each register (offset, fields, callbacks)
            ///        via the Register Framework DSL.
            DefineRegisters();

            /// @brief Bring all fields, the time base, and the IRQ line
            ///        to their post-reset state before the CPU starts.
            Reset();
        }

        /// @brief 32-bit bus read entry point.
        ///
        /// Called by the system bus whenever the CPU issues a 32-bit
        /// load in this peripheral's aperture. We simply forward to
        /// the Register Framework, which:
        ///   1) looks up the register defined at `offset`,
        ///   2) invokes any registered `valueProviderCallback`s,
        ///   3) returns the assembled 32-bit value.
        ///
        /// @param offset  Byte offset within the peripheral's window.
        /// @return        The 32-bit value assembled from that register.
        public uint ReadDoubleWord(long offset)
        {
            return RegistersCollection.Read(offset);
        }

        /// @brief 32-bit bus write entry point.
        ///
        /// Mirrors `ReadDoubleWord`. The framework decodes the offset,
        /// updates the affected fields, and fires any per-field
        /// `writeCallback`s that we registered in `DefineRegisters`.
        ///
        /// @param offset  Byte offset within the peripheral's window.
        /// @param value   The 32-bit value the CPU wrote.
        public void WriteDoubleWord(long offset, uint value)
        {
            RegistersCollection.Write(offset, value);
        }

        /// @brief Cold-reset the whole peripheral.
        ///
        /// Called on machine reset, on `machine Reset` from the
        /// Monitor, and once during construction. Must be idempotent
        /// and leave the peripheral in the exact state a real chip
        /// would present after power-on.
        public void Reset()
        {
            /// @brief Reset every register field to its declared
            ///        default (all zero unless a `WithFlag(..., true)`
            ///        default was passed).
            RegistersCollection.Reset();

            /// @brief Stop the timebase and zero the counter.
            innerTimer.Reset();

            /// @brief Drive IRQ inactive. Necessary because `pendingFlag`
            ///        is cleared above but the physical line has to be
            ///        pulled low explicitly so the interrupt controller
            ///        sees the falling edge.
            IRQ.Unset();
        }

        /// @brief Size of the peripheral's window on the system bus.
        ///
        /// 0x100 bytes leaves ample room for future registers without
        /// requiring a .repl edit. The .repl typically pins the base
        /// (`Timers.SimpleTimer @ sysbus 0x50000000`); the framework
        /// combines base + this size to establish the aperture.
        public long Size => 0x100;

        /// @brief Interrupt output signal.
        ///
        /// Wired into the platform's interrupt controller from the .repl
        /// (e.g. `simpletimer -> plic@10`). Renode routes edge/level
        /// semantics on the receiving side; the sender just sets/unsets.
        public GPIO IRQ { get; }

        /// @brief Register collection exposed to the Register Framework.
        ///
        /// The property is public so infrastructure code (Monitor
        /// commands like `sysbus.mydev ReadDoubleWord 0x00`) can walk
        /// the collection for introspection.
        public DoubleWordRegisterCollection RegistersCollection { get; }

        /// @brief Declare all four registers using the fluent DSL.
        ///
        /// Each `DefineRegister` call:
        ///   - Anchors a register at the given byte offset.
        ///   - Chains `.WithFlag` / `.WithValueField` calls to describe
        ///     bit fields (position, width, access mode, name).
        ///   - Optionally captures a field reference (`out someFlag`)
        ///     so the C# code can read/write it directly later.
        ///   - Optionally registers callbacks that fire whenever the
        ///     CPU writes / reads the field.
        private void DefineRegisters()
        {
            /// @brief CONTROL register (offset 0x00).
            ///        bit0 ENABLE     — starts/stops `innerTimer`.
            ///        bit1 IRQ_ENABLE — masks the IRQ output.
            ///        bits 2..31      — reserved, read as zero, ignore writes.
            RegistersCollection.DefineRegister((long)Registers.Control)
                .WithFlag(0, out enableFlag, name: "ENABLE",
                    /// @brief Propagate CONTROL.ENABLE into the time base.
                    ///        The callback fires on every write, so writing
                    ///        1 starts counting and writing 0 stops it.
                    writeCallback: (_, value) => { innerTimer.Enabled = value; })
                .WithFlag(1, out irqEnableFlag, name: "IRQ_ENABLE",
                    /// @brief Re-evaluate the IRQ line whenever the mask
                    ///        bit changes so that clearing IRQ_ENABLE
                    ///        immediately drops an already-active line.
                    writeCallback: (_, __) => UpdateInterrupt())
                .WithReservedBits(2, 30);

            /// @brief RELOAD register (offset 0x04).
            ///        32-bit period (in timer ticks). Writing 0 would
            ///        stall `LimitTimer`, so we clamp to 1.
            RegistersCollection.DefineRegister((long)Registers.Reload)
                .WithValueField(0, 32, out reloadField, name: "RELOAD",
                    /// @brief Push the new period into the time base.
                    ///        Because `WorkMode.Periodic` is set, the
                    ///        next auto-reload uses this new limit.
                    writeCallback: (_, value) => { innerTimer.Limit = (value == 0 ? 1UL : value); });

            /// @brief COUNTER register (offset 0x08, read-only).
            ///        Returns the live counter from the time base.
            RegistersCollection.DefineRegister((long)Registers.Counter)
                .WithValueField(0, 32, FieldMode.Read, name: "COUNTER",
                    /// @brief On every read, sample the time base's
                    ///        current value. Writes are silently
                    ///        discarded (FieldMode.Read only).
                    valueProviderCallback: _ => innerTimer.Value);

            /// @brief STATUS register (offset 0x0C).
            ///        bit0 PENDING — set by the peripheral when a period
            ///        elapses, cleared by the CPU writing a 1
            ///        (`WriteOneToClear` semantics, common on ARM/RISC-V
            ///        peripherals). Bits 1..31 reserved.
            RegistersCollection.DefineRegister((long)Registers.Status)
                .WithFlag(0, out pendingFlag, FieldMode.Read | FieldMode.WriteOneToClear, name: "PENDING",
                    /// @brief The framework has already cleared the bit
                    ///        by the time this callback fires; we just
                    ///        need to drop the IRQ line if nothing else
                    ///        is pending.
                    writeCallback: (_, __) => UpdateInterrupt())
                .WithReservedBits(1, 31);
        }

        /// @brief Handler for `LimitTimer.LimitReached`.
        ///
        /// Called once per period on the machine's virtual clock, in
        /// the same thread that drives simulation, so it may safely
        /// touch peripheral state without extra locking.
        private void OnLimitReached()
        {
            /// @brief Diagnostic breadcrumb, visible under `logLevel -1
            ///        sysbus.simpletimer`. Level `Noisy` is filtered out
            ///        by default.
            this.Log(LogLevel.Noisy, "Period elapsed; raising IRQ");

            /// @brief Set STATUS.PENDING. Since the register is
            ///        WriteOneToClear, only the CPU (via a bus write)
            ///        can clear it — model behaviour matches typical
            ///        MCU peripherals.
            pendingFlag.Value = true;

            /// @brief Re-drive the IRQ line based on the new PENDING
            ///        state and the current IRQ_ENABLE mask.
            UpdateInterrupt();
        }

        // The IRQ line is level-sensitive: high while a tick is pending AND
        // interrupts are enabled. The CPU clears it by writing STATUS.

        /// @brief Recompute and drive the IRQ output line.
        ///
        /// Called from three places:
        ///   1. `OnLimitReached` — a new tick has become pending.
        ///   2. IRQ_ENABLE writeCallback — mask bit changed.
        ///   3. STATUS.PENDING writeCallback — CPU acked the interrupt.
        ///
        /// The line is level-sensitive so we set it to the boolean AND
        /// of `pendingFlag && irqEnableFlag`; the receiving interrupt
        /// controller will latch on the rising edge and hold as long as
        /// the line stays high.
        private void UpdateInterrupt()
        {
            var irq = pendingFlag.Value && irqEnableFlag.Value;

            /// @brief Trace every state transition for debugging.
            this.Log(LogLevel.Noisy, "Setting IRQ to {0}", irq);

            /// @brief `Set(bool)` is the level-oriented API on `GPIO`;
            ///        `true` drives the line high, `false` low. The
            ///        interrupt controller on the other end handles
            ///        edge detection.
            IRQ.Set(irq);
        }

        /// @brief Underlying periodic time base. Set at construction
        ///        and never reassigned.
        private readonly LimitTimer innerTimer;

        /// @brief Handle to CONTROL.ENABLE — captured via `out` in
        ///        `DefineRegisters` so `OnLimitReached` / `Reset` can
        ///        inspect the current mask without walking the register.
        private IFlagRegisterField enableFlag;

        /// @brief Handle to CONTROL.IRQ_ENABLE. Read by
        ///        `UpdateInterrupt` to compute the effective IRQ level.
        private IFlagRegisterField irqEnableFlag;

        /// @brief Handle to STATUS.PENDING. Written from
        ///        `OnLimitReached`, cleared by the CPU via
        ///        WriteOneToClear.
        private IFlagRegisterField pendingFlag;

        /// @brief Handle to RELOAD. Currently unused after construction
        ///        (writes drive `innerTimer.Limit` directly), but kept
        ///        available for future features (e.g. read-back).
        private IValueRegisterField reloadField;

        /// @brief Fallback clock rate: 1 MHz. Overridable in the .repl
        ///        via `frequency: 32768` (or any other integer).
        private const long DefaultFrequency = 1000000;

        /// @enum Registers
        /// @brief Symbolic names for register offsets.
        ///
        /// Prefer this over magic numbers in `DefineRegister` calls: it
        /// makes the layout self-documenting and matches how Renode's
        /// upstream peripherals are written.
        private enum Registers : long
        {
            Control = 0x00,  ///< bit0 ENABLE, bit1 IRQ_ENABLE
            Reload  = 0x04,  ///< 32-bit period in ticks (write clamped to 1)
            Counter = 0x08,  ///< live count, read-only
            Status  = 0x0C,  ///< bit0 PENDING, write-1-to-clear
        }
    }
}
