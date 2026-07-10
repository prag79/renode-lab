# Lab 05 — Interrupts on the FE310: a machine-timer ISR

Lab 04 blinked the LED with a `delay()` busy-loop — the CPU burned
100% of its cycles spinning. This lab does it the way real firmware
does: the core **sleeps in `wfi`** and is woken by the **CLINT
machine timer interrupt**. Every tick, an interrupt service routine
re-arms the timer and toggles the LED.

This is the hardest lab so far because it touches the parts of
RISC-V you can't see from C alone:

- **Control & status registers (CSRs):** `mtvec`, `mstatus`, `mie`,
  `mcause` — programmed via `csrr`/`csrw`/`csrs`.
- **The trap vector:** `mtvec` points at the handler; we use *direct*
  mode (all traps enter one function).
- **The CLINT:** a 64-bit `mtime` free-running counter and a
  `mtimecmp` compare register. When `mtime >= mtimecmp`, the timer
  interrupt fires (CLINT line 1 → `cpu@7`).
- **64-bit registers on a 32-bit core:** `mtime`/`mtimecmp` are
  accessed as two 32-bit halves, which needs careful read/write
  sequencing.

## 1. Bring it up

```bash
lab 05
```

Mirrors `/labs/05-fe310-interrupts/` into `~/work/05-fe310-interrupts/`,
runs `make` (RV32IMAC, same toolchain as lab 04) to build `timer.elf`,
then launches `renode/timer-irq.resc`.

In the **`uart0` analyzer** (noVNC tab, port 6080) you'll see:

```
*** FE310 timer-interrupt blink ***
Setup: LED on GPIO pin 19, CLINT machine timer at 5 Hz.
The CPU sleeps in 'wfi' and only wakes when the timer
interrupt fires; each wake toggles the LED and prints a
line below. Watch the [IRQ #N] counter climb.

[IRQ #1] machine-timer fired -> re-armed mtimecmp, LED ON
[IRQ #2] machine-timer fired -> re-armed mtimecmp, LED OFF
[IRQ #3] machine-timer fired -> re-armed mtimecmp, LED ON
...
```

Each `[IRQ #N]` line is one ISR invocation — one machine-timer
interrupt, one LED toggle. The counter proves the interrupt is
*periodic* (re-armed each time), and the `LED ON`/`LED OFF` shows
the pin-19 state after the toggle. At `TICK_HZ = 5` that's five
interrupts a second. If the noVNC tab is unavailable, use the
`CreateFileBackend` recipe from lab 04 §4 (swap `blink.elf` →
`timer.elf`).

## 2. How the interrupt is wired

The setup sequence in `src/main.c` `main()`:

```c
set_mtimecmp(read_mtime() + INTERVAL);          // 1. first deadline
write_csr(mtvec, (unsigned long)&timer_isr);    // 2. trap vector (direct mode)
set_csr(mie, MIE_MTIE);                          // 3. enable timer irq (bit 7)
set_csr(mstatus, MSTATUS_MIE);                   // 4. global irq enable (bit 3)
for (;;) __asm__ volatile ("wfi");               // 5. sleep
```

When `mtime` reaches `mtimecmp`, the CLINT raises `cpu@7`. Because
`mie.MTIE` and `mstatus.MIE` are both set, the core traps to
`mtvec` → `timer_isr()`:

```c
void __attribute__((interrupt("machine"))) timer_isr(void) {
    tick_count++;
    set_mtimecmp(read_mtime() + INTERVAL);   // re-arm the next deadline
    GPIO_OUTPUT_VAL ^= LED_MASK;             // toggle LED
    int led_on = (GPIO_OUTPUT_VAL & LED_MASK) != 0;
    uart_puts("[IRQ #"); uart_putu(tick_count);
    uart_puts("] machine-timer fired -> re-armed mtimecmp, LED ");
    uart_puts(led_on ? "ON\n" : "OFF\n");
}
```

The `interrupt("machine")` attribute tells GCC to save/restore the
registers and end with `mret` instead of `ret`. Re-arming
`mtimecmp` inside the ISR is what makes the interrupt *periodic* —
forget that line and you'd get exactly one `[IRQ #1]` line, then
silence. `uart_putu` is a tiny bare-metal decimal printer (there's
no libc `printf` here).

> **Why park `mtimecmp` high before writing?** On a 32-bit core the
> 64-bit compare is two stores. If you wrote the low half while the
> old high half still matched `mtime`, a spurious interrupt could
> fire mid-update. `set_mtimecmp()` writes `0xffffffff` to the high
> word first, then the low word, then the real high word.

## 3. Files

| File | What it is |
|---|---|
| `src/start.S` | RV32 reset vector — `sp`, zero `.bss`, call `main`. |
| `src/main.c` | CSR helpers, CLINT access, the timer ISR, and `main`. |
| `src/link.ld` | 16 KiB DTIM layout (same as lab 04). |
| `Makefile` | RV32 cross-compile rules. **Real TABs in recipes.** |
| `renode/hifive1.repl` | FE310 platform (CLINT frequency = 62 MHz). |
| `renode/timer-irq.resc` | Startup script. |

## 4. Useful monitor commands to try

Interrupts make the CPU state much more interesting to inspect.

| Command | What it does |
|---|---|
| `sysbus.cpu PC` | After `start`, the core is usually parked in the `wfi` loop. |
| `sysbus.cpu MTVEC` | Read the trap vector you programmed — equals `&timer_isr`. |
| `sysbus.cpu MSTATUS` | Bit 3 (`MIE`) is set; bit 7 (`MPIE`) toggles around traps. |
| `sysbus.cpu MIE` | Bit 7 (`MTIE`) is set. |
| `sysbus.cpu MCAUSE` | After a tick: `0x80000007` — top bit = interrupt, code 7 = machine timer. |
| `sysbus.cpu LogFunctionNames true` | Watch `main` → `wfi`, then `timer_isr` entries scroll by. |
| `sysbus ReadDoubleWord 0x0200BFF8` | Read CLINT `mtime` low word — it's counting. |
| `sysbus ReadDoubleWord 0x02004000` | Read `mtimecmp` low word — the next deadline. |
| `sysbus ReadDoubleWord 0x1001200C` | GPIO `output_val`; bit 19 flips every interrupt. |
| `emulation RunFor "1.0"` | Advance 1 s of sim-time; you should count ~5 `[IRQ #N]` lines. |
| `quit` | Exit Renode. |

## 5. Mini-experiments (try at least one)

1. **Catch the ISR in the act.** Let it run, then at the monitor:

   ```text
   pause
   sysbus.cpu LogFunctionNames true
   emulation RunFor "0.3"
   ```

   The log shows the core sitting in `main`/`wfi`, then jumping to
   `timer_isr` each tick and returning. Turn off with
   `sysbus.cpu LogFunctionNames false`.

2. **Confirm the trap cause.** Pause right after a tick and read
   `sysbus.cpu MCAUSE` — `0x80000007`. The high bit means "this was
   an interrupt, not an exception"; `7` is the machine-timer code.

3. **Change the blink rate.** Edit `TICK_HZ` in `src/main.c`
   (try `1` for one tick/second, or `20` for a fast strobe), save,
   re-run `lab 05`. The `tick` cadence and the LED follow.

4. **Break it on purpose.** Comment out the `set_mtimecmp(...)`
   line *inside* `timer_isr` and re-run. You get exactly one
   `[IRQ #1]` line, then silence — proof that the periodicity comes
   from re-arming the compare register, not from the timer itself.

5. **Disable interrupts and watch it stall.** From the monitor
   while running:

   ```text
   pause
   sysbus.cpu MIE 0
   start
   ```

   Clearing `mie` stops new ticks; the LED freezes. Restore with
   `machine Reset` + `start`.

## 6. Stopping cleanly

```text
quit
```

…or `Ctrl-D`. Next, `lab 06` flips perspective entirely: instead of
watching firmware by eye, you'll **assert** its behaviour
automatically with a headless Renode test — the feature that makes
Renode a CI tool, not just a debugger.

## What this lab proves

- You can stand up the full RISC-V machine-mode interrupt path from
  scratch: trap vector, CSR enables, and a periodic timer source.
- Renode models the CLINT, CSRs, and trap delivery faithfully enough
  that real interrupt-driven firmware "just works" — and you can
  inspect `mcause`/`mtvec`/`mie` mid-trap, which is hard on real
  hardware.
- `wfi` + interrupts is the low-power idiom every MCU uses; you now
  have it running on a simulated SiFive part.
