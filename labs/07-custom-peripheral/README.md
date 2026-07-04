# Lab 07 — Model your own peripheral (a custom timer IP)

Every prior lab used peripherals Renode already ships. This lab
crosses the line into Renode's real superpower: **you write the
hardware**. We model a small memory-mapped timer IP in C#, Renode
compiles it on the fly, and bare-metal RISC-V firmware drives it —
interrupt and all — exactly as if it were a real block on a real SoC.

This is the capstone. It combines lab 03 (build firmware + platform),
lab 05 (RISC-V interrupts), and a brand-new piece: a hand-written
peripheral model that raises an IRQ into the CPU.

## 1. Run it

```bash
lab 07
```

Mirrors `/labs/07-custom-peripheral/` into your work tree, runs
`make` to build `firmware.elf`, then launches
`renode/custom-timer.resc`. That script **compiles the C# peripheral**
(`i @peripherals/SimpleTimer.cs`) before loading the platform.

**Where to read UART output (important):**

Renode does **not** echo UART traffic to the monitor console. In the
default headless mode (`LAB_GUI=0`) nothing appears in noVNC either.
The bundled `.resc` always attaches a file backend, so open a **second
terminal** and run:

```bash
tail -f /tmp/uart.log
```

You should see (first tick ~1 s after `start` — the timer period is
1,000,000 ticks at 1 MHz):

```
*** Custom peripheral lab: SimpleTimer IP ***
Programming the timer and going to sleep...
tick from my custom timer IP
tick from my custom timer IP
...
```

Each `tick` is your C# peripheral firing its `LimitReached` event,
raising the IRQ line you wired to the CPU, and the firmware's trap
handler acknowledging it.

**Optional — noVNC GUI window (`LAB_GUI=1` only):**

The `.resc` calls `showAnalyzer sysbus.uart` only when
`LAB_GUI=1`. Switch modes, then relaunch:

```bash
quit
export LAB_GUI=1
entrypoint.sh true
lab 07
```

Open forwarded port **6080** (`/vnc.html`) and look for the **UART
analyzer window** on the virtual desktop (not the VS Code terminal).
See lab 01 §2 for details. If the widget still does not appear, use
`tail -f /tmp/uart.log` — that path always works.

**Troubleshooting — no ticks at all (even in `/tmp/uart.log`):**

1. Confirm your working copy has the Zicsr CPU fix — the `.repl` must
   say `rv64imac_zicsr_zifencei`, not bare `rv64imac`:

   ```bash
   grep cpuType ~/work/07-custom-peripheral/renode/custom-timer.repl
   ```

   If stale, refresh from the repo:

   ```bash
   cd /workspaces/renode-lab && git pull
   cp /workspaces/renode-lab/labs/07-custom-peripheral/renode/custom-timer.{repl,resc} \
      ~/work/07-custom-peripheral/renode/
   lab 07
   ```

2. At the monitor prompt, verify the timer is counting:

   ```text
   logLevel -1 sysbus.mytimer
   sysbus ReadDoubleWord 0x10001008
   ```

   Re-run the read a few times — `COUNTER` should increase. You should
   also see `[NOISY] mytimer: Period elapsed; raising IRQ` about once
   per second.

3. Check the CPU took the interrupt:

   ```text
   sysbus.cpu MCAUSE
   ```

   After a tick, this should show `0x800000000000000b` (interrupt +
   external code 11).

## 2. The peripheral (`peripherals/SimpleTimer.cs`)

This is a real Renode peripheral model — the same shape as the
hundreds in Renode's library. The essentials:

```csharp
public class SimpleTimer : IDoubleWordPeripheral,
        IProvidesRegisterCollection<DoubleWordRegisterCollection>, IKnownSize
{
    public SimpleTimer(IMachine machine, long frequency = 1000000)
    {
        IRQ = new GPIO();                                   // interrupt line
        innerTimer = new LimitTimer(machine.ClockSource, frequency, this,
            "simpletimer", limit: uint.MaxValue,
            workMode: WorkMode.Periodic, eventEnabled: true);
        innerTimer.LimitReached += OnLimitReached;          // fires each period
        RegistersCollection = new DoubleWordRegisterCollection(this);
        DefineRegisters();
        Reset();
    }
    ...
}
```

Three ideas do all the work:

- **Bus interface.** Implementing `IDoubleWordPeripheral` means the
  system bus routes 32-bit reads/writes at this peripheral's address
  to `ReadDoubleWord`/`WriteDoubleWord`, which defer to the
  **Register Framework** (`RegistersCollection`). Each register is
  described declaratively with `.WithFlag(...)` / `.WithValueField(...)`,
  including access modes like `WriteOneToClear`.
- **Time.** A `LimitTimer` (driven by the machine clock) counts up to
  `RELOAD` and raises `LimitReached` every period — that's the
  hardware behaviour, modelled in a few lines.
- **Interrupts.** `IRQ` is a `GPIO`. `IRQ.Set(true/false)` asserts or
  clears the line. We keep it level-sensitive: high while a tick is
  pending **and** interrupts are enabled, cleared when firmware writes
  `STATUS`.

Register map:

| Offset | Name | Meaning |
|---|---|---|
| `0x00` | `CONTROL` | bit0 `ENABLE`, bit1 `IRQ_ENABLE` |
| `0x04` | `RELOAD` | period in timer ticks |
| `0x08` | `COUNTER` | current count (read-only) |
| `0x0C` | `STATUS` | bit0 `PENDING` (read; write 1 to clear) |

## 3. Wiring it into the SoC (`renode/custom-timer.repl`)

```
uart:    UART.NS16550 @ sysbus 0x10000000
mytimer: Timers.SimpleTimer @ sysbus 0x10001000
    IRQ -> cpu@11
```

The CPU is declared as `rv64imac_zicsr_zifencei` (not bare
`rv64imac`) because the firmware programs CSRs (`mtvec`, `mie`,
`mstatus`) and uses `wfi`. Without the `Zicsr` extension Renode
logs *RISC-V Zicsr instruction set is not enabled for this CPU*
and the interrupt path never comes up.

`Timers.SimpleTimer` is the type Renode just compiled from our `.cs`
(its namespace is `Antmicro.Renode.Peripherals.Timers`). `IRQ ->
cpu@11` connects the peripheral's interrupt line straight to the
RISC-V **machine-external-interrupt** input — no PLIC needed for a
single source.

The order in the `.resc` matters: `i @peripherals/SimpleTimer.cs`
**before** `LoadPlatformDescription`, because the platform references
the freshly-compiled type.

## 4. The firmware (`src/main.c`)

Same interrupt setup as lab 05, but the source is *our* peripheral:

```c
TIMER_RELOAD  = 1000000;                   // 1 MHz timer -> ~1 s period
TIMER_CONTROL = CTRL_ENABLE | CTRL_IRQ_ENABLE;
write_csr(mtvec, (unsigned long)&trap_handler);
set_csr(mie, 1UL << 11);                   // MEIE: machine external irq
set_csr(mstatus, 1UL << 3);                // global interrupt enable
for (;;) __asm__ volatile ("wfi");
```

The trap handler acknowledges the interrupt by writing `STATUS`
(write-1-to-clear), which makes the C# `UpdateInterrupt()` drop the
IRQ line — proving the round trip from firmware back into your model.

## 5. Files

| File | What it is |
|---|---|
| `peripherals/SimpleTimer.cs` | **The IP model.** Compiled by Renode at runtime. |
| `renode/custom-timer.repl` | SoC: CPU + RAM + UART + your timer (IRQ → cpu@11). |
| `renode/custom-timer.resc` | Compiles the `.cs`, builds the machine, runs. |
| `src/main.c` | Firmware: programs the timer, handles its IRQ. |
| `src/start.S`, `src/link.ld` | RV64 startup + linker script. |
| `Makefile` | Builds `firmware.elf`. **Real TABs in recipes.** |

## 6. Useful monitor commands to try

Your peripheral is a first-class citizen in the monitor.

| Command | What it does |
|---|---|
| `peripherals` | Lists `mytimer` alongside `cpu`, `ram`, `uart`. |
| `sysbus.mytimer` | Inspect the object; tab-complete its methods/properties. |
| `sysbus ReadDoubleWord 0x10001008` | Read the live `COUNTER` register. |
| `sysbus ReadDoubleWord 0x1000100C` | Read `STATUS`; bit 0 = pending. |
| `logLevel -1 sysbus.mytimer` | Show the `Noisy` logs from `OnLimitReached`/`UpdateInterrupt`. |
| `sysbus.cpu MCAUSE` | After a tick: `0x800...00B` (interrupt + external code 11). |
| `emulation RunFor "2.5"` | Advance 2.5 s of sim-time; expect ~2 ticks. |
| `sysbus WriteDoubleWord 0x10001000 0x0` | Disable the timer from the monitor; ticks stop. |
| `quit` | Exit Renode. |

## 7. Mini-experiments (try at least one)

1. **Change the period.** Edit `PERIOD_TICKS` in `src/main.c` (e.g.
   `250000` for ~4 ticks/second), re-run `lab 07`. The `tick` cadence
   follows.

2. **Add a register to the model.** In `SimpleTimer.cs`, add a
   `TickCount` register that returns how many times `LimitReached`
   fired:
   - add `TickCount = 0x10` to the `Registers` enum,
   - increment a `private ulong tickCount;` in `OnLimitReached`,
   - define it: `RegistersCollection.DefineRegister((long)Registers.TickCount)
     .WithValueField(0, 32, FieldMode.Read, valueProviderCallback: _ => tickCount, name: "TICK_COUNT");`

   Re-run `lab 07`, then `sysbus ReadDoubleWord 0x10001010` in the
   monitor — your new register counts up. You just extended a hardware
   block in ~4 lines.

3. **Make it one-shot.** Change `WorkMode.Periodic` to
   `WorkMode.OneShot` in the constructor. Now the timer fires exactly
   once after enable — you get a single `tick`. This is how you'd
   model a watchdog or a delay timer.

4. **Break the IRQ wiring.** In `custom-timer.repl`, comment out the
   `IRQ -> cpu@11` line and re-run. The timer still counts (read
   `COUNTER`), but no interrupt is ever delivered, so no `tick` prints
   — a vivid demo of what the interrupt connection actually does.

5. **Drive it entirely from the monitor (no firmware).** Run
   `lab monitor`, then by hand:

   ```text
   mach create
   i @peripherals/SimpleTimer.cs
   machine LoadPlatformDescription @renode/custom-timer.repl
   sysbus WriteDoubleWord 0x10001004 1000000     # RELOAD
   sysbus WriteDoubleWord 0x10001000 1           # ENABLE (no IRQ)
   start
   sysbus ReadDoubleWord 0x10001008              # COUNTER advancing
   ```

## 8. Stopping cleanly

```text
quit
```

…or `Ctrl-D`.

## What this lab proves

- A Renode peripheral is just a C# class implementing a bus interface;
  the Register Framework and `LimitTimer` make non-trivial behaviour
  (periodic interrupts, write-1-to-clear status) only a few lines.
- Renode compiles your model at runtime — no rebuild of Renode — so
  the edit-compile-run loop on *hardware behaviour* is as fast as on
  firmware.
- Your model is indistinguishable from a built-in peripheral: it
  appears in `peripherals`, logs bus accesses, raises real interrupts
  into the CPU, and is inspectable from the monitor.
- This is how new chips get modelled before silicon exists — and how
  you'd add the device your own firmware needs.
