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
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.Timers;

namespace Antmicro.Renode.Peripherals.Timers
{
    // IDoubleWordPeripheral  -> 32-bit bus accesses
    // IProvidesRegisterCollection -> lets us use the Register Framework
    // IKnownSize             -> we advertise our footprint on the bus
    public class SimpleTimer : IDoubleWordPeripheral, IProvidesRegisterCollection<DoubleWordRegisterCollection>, IKnownSize
    {
        public SimpleTimer(IMachine machine, long frequency = DefaultFrequency)
        {
            IRQ = new GPIO();

            // The actual time-keeping is delegated to a LimitTimer driven by
            // the machine clock. WorkMode.Periodic makes it auto-reload, so it
            // fires LimitReached once per period.
            innerTimer = new LimitTimer(machine.ClockSource, frequency, this, "simpletimer",
                limit: uint.MaxValue, workMode: WorkMode.Periodic, eventEnabled: true);
            innerTimer.LimitReached += OnLimitReached;

            RegistersCollection = new DoubleWordRegisterCollection(this);
            DefineRegisters();
            Reset();
        }

        public uint ReadDoubleWord(long offset)
        {
            return RegistersCollection.Read(offset);
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            RegistersCollection.Write(offset, value);
        }

        public void Reset()
        {
            RegistersCollection.Reset();
            innerTimer.Reset();
            IRQ.Unset();
        }

        public long Size => 0x100;

        public GPIO IRQ { get; }

        public DoubleWordRegisterCollection RegistersCollection { get; }

        private void DefineRegisters()
        {
            RegistersCollection.DefineRegister((long)Registers.Control)
                .WithFlag(0, out enableFlag, name: "ENABLE",
                    writeCallback: (_, value) => { innerTimer.Enabled = value; })
                .WithFlag(1, out irqEnableFlag, name: "IRQ_ENABLE",
                    writeCallback: (_, __) => UpdateInterrupt())
                .WithReservedBits(2, 30);

            RegistersCollection.DefineRegister((long)Registers.Reload)
                .WithValueField(0, 32, out reloadField, name: "RELOAD",
                    writeCallback: (_, value) => { innerTimer.Limit = (value == 0 ? 1UL : value); });

            RegistersCollection.DefineRegister((long)Registers.Counter)
                .WithValueField(0, 32, FieldMode.Read, name: "COUNTER",
                    valueProviderCallback: _ => innerTimer.Value);

            RegistersCollection.DefineRegister((long)Registers.Status)
                .WithFlag(0, out pendingFlag, FieldMode.Read | FieldMode.WriteOneToClear, name: "PENDING",
                    writeCallback: (_, __) => UpdateInterrupt())
                .WithReservedBits(1, 31);
        }

        private void OnLimitReached()
        {
            this.Log(LogLevel.Noisy, "Period elapsed; raising IRQ");
            pendingFlag.Value = true;
            UpdateInterrupt();
        }

        // The IRQ line is level-sensitive: high while a tick is pending AND
        // interrupts are enabled. The CPU clears it by writing STATUS.
        private void UpdateInterrupt()
        {
            var irq = pendingFlag.Value && irqEnableFlag.Value;
            this.Log(LogLevel.Noisy, "Setting IRQ to {0}", irq);
            IRQ.Set(irq);
        }

        private readonly LimitTimer innerTimer;

        private IFlagRegisterField enableFlag;
        private IFlagRegisterField irqEnableFlag;
        private IFlagRegisterField pendingFlag;
        private IValueRegisterField reloadField;

        private const long DefaultFrequency = 1000000;

        private enum Registers : long
        {
            Control = 0x00,
            Reload  = 0x04,
            Counter = 0x08,
            Status  = 0x0C,
        }
    }
}
