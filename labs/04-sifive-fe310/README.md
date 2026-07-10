# Lab 04 — Bare-metal on a SiFive FE310 (HiFive1)

Lab 03 invented a SoC. This lab targets a **real** one: the SiFive
**FE310-G000**, the RV32IMAC chip on the HiFive1 board. The platform
description uses the chip's actual datasheet addresses, and your C
talks to two genuine SiFive peripherals — the SiFive UART (which is
**not** an NS16550; different register map) and the SiFive GPIO
block.

Where lab 03 was "any SoC you like", this is "the exact registers a
HiFive1 firmware engineer pokes". Two new wrinkles versus lab 03:

- **32-bit core.** RV32IMAC, not RV64 — different `-march`/`-mabi`
  and 4-byte words in the startup code.
- **Real peripherals.** The UART has a `txdata`/`txctrl` protocol
  (not a THR/LSR pair), and the GPIO uses `output_en`/`output_val`.
- **Tiny RAM.** The FE310 has just **16 KiB** of on-chip SRAM, so
  the linker script and stack are sized accordingly.

## 1. Bring the SoC up

```bash
lab 04
```

This does three things in order:

1. Mirror `/labs/04-sifive-fe310/` into your editable scratch tree
   at `~/work/04-sifive-fe310/` (`cp -ru`, never clobbers edits).
2. `make` — cross-compiles `src/start.S` + `src/blink.c` with
   `riscv64-unknown-elf-gcc -march=rv32imac -mabi=ilp32` into
   `blink.elf` / `blink.bin`.
3. `renode --plain --disable-gui --console renode/hifive1.resc` —
   loads the FE310 platform, drops `blink.elf` at the RAM base
   (`0x80000000`), opens the `uart0` analyzer, and starts the CPU.

After the first run you can also drive it directly:

```bash
cd ~/work/04-sifive-fe310
make
renode --console renode/hifive1.resc
```

In the **`uart0` analyzer** window (noVNC tab on port 6080, path
`/vnc.html`) you should see:

```
*** Hello from a SiFive FE310 (HiFive1)! ***
UART0 @ 0x10013000, GPIO @ 0x10012000, RAM @ 0x80000000
LED on
LED off
LED on
...
```

The `LED on` / `LED off` lines stream forever — that's the blink
loop narrating itself. If the noVNC tab is unavailable, jump to
section 4 to read the UART from the monitor.

## 2. What just happened

`renode/hifive1.repl` is the SoC. The interesting blocks:

```
cpu:   CPU.RiscV32 @ sysbus            # rv32imac_zicsr_zifencei
dtim:  Memory.MappedMemory @ 0x80000000  size 0x4000   # 16 KiB SRAM
uart0: UART.SiFive_UART @ 0x10013000
gpio:  GPIOPort.SiFive_GPIO @ 0x10012000
    19 -> led@0                        # LED wired to pin 19
led:   Miscellaneous.LED @ gpio 19
clint: IRQControllers.CoreLevelInterruptor @ 0x02000000  # used in lab 05
plic:  IRQControllers.PlatformLevelInterruptController @ 0x0C000000
```

The `led` on pin 19 is what the firmware blinks. A GPIO output pin
needs a receiver connected, or Renode logs *Trying to write a pin
that isn't configured for writing* on every toggle. Connecting the
LED silences that and gives you `gpio.led State` to read.

Every address is the real FE310 memory map — compare it to SiFive's
FE310-G000 manual and they line up. Same three primitives as every
prior lab: `mach create`, `LoadPlatformDescription`, `LoadELF` +
`start`.

## 3. Files

| File | What it is |
|---|---|
| `src/start.S` | RV32 reset vector — set `sp`, zero `.bss` (4-byte stores), call `main`. |
| `src/blink.c` | Inits UART0, prints the banner, blinks GPIO pin 19. |
| `src/link.ld` | Links everything into the 16 KiB DTIM at `0x80000000`; 4 KiB stack. |
| `Makefile` | RV32 cross-compile rules. **Recipes use real TABs.** |
| `renode/hifive1.repl` | FE310 platform (CPU + RAM + UART + GPIO + CLINT + PLIC). |
| `renode/hifive1.resc` | Startup script. |

## 4. Read UART from the monitor (no GUI needed)

`Ctrl-C` once at the running monitor to get the prompt back, then:

```text
pause
sysbus.uart0 CreateFileBackend @/tmp/hifive1.log true
machine Reset
start
```

In **another Codespace terminal**:

```bash
tail -f /tmp/hifive1.log
```

The banner and the `LED on` / `LED off` stream land in the file.
(`CreateFileBackend` only captures bytes written *after* it is
attached; `machine Reset` re-runs the reset macro, which reloads
`blink.elf` and restarts from `_start`.)

## 5. Useful monitor commands to try

The SiFive UART and GPIO have different register maps from lab 01's
STM32 and lab 03's NS16550 — worth poking by hand.

| Command | What it does |
|---|---|
| `peripherals` | Confirms the FE310 block list (cpu, uart0, gpio, clint, plic, …). |
| `sysbus.cpu PC` | Read the program counter (RV32, so 32-bit values). |
| `pause` / `start` | Freeze and resume the CPU. |
| `sysbus.cpu Step 50` | Step 50 RV32 instructions. |
| `sysbus.cpu LogFunctionNames true` | Log entries to `_start`, `main`, `uart_putc`, `delay`, … |
| `sysbus ReadDoubleWord 0x1001200C` | Read GPIO `output_val`; bit 19 tracks the LED. |
| `sysbus ReadDoubleWord 0x10012008` | Read GPIO `output_en`; bit 19 is set once `main` runs. |
| `gpio.led State` | Read the connected LED object directly (`True`/`False`). |
| `logLevel -1 gpio.led` | Log every LED state change as the blink loop runs. |
| `sysbus WriteByte 0x10013000 0x41` | Write 'A' straight to the UART `txdata` register. |
| `logLevel 0 sysbus.gpio` | Verbose-log every GPIO register access. Reset with `logLevel 3 sysbus.gpio`. |
| `logLevel 0 sysbus.uart0` | Same for the UART. |
| `emulation RunFor "0.01"` | Advance exactly 10 ms of simulated time, then auto-pause. |
| `machine Reset` | Run the `reset` macro — reloads `blink.elf`, PC back to `_start`. |
| `quit` | Exit Renode. |

## 6. Mini-experiments (try at least one)

1. **Watch the LED bit flip from the monitor.** Pause, then read
   the GPIO output register repeatedly across simulated time:

   ```text
   pause
   sysbus ReadDoubleWord 0x1001200C       # bit 19 = 0 or 0x80000
   emulation RunFor "0.05"
   sysbus ReadDoubleWord 0x1001200C       # flipped
   emulation RunFor "0.05"
   sysbus ReadDoubleWord 0x1001200C       # flipped back
   ```

   Bit 19 (`0x00080000`) toggling *is* the LED blinking, observed at
   the register level — same physical event lab 01 showed on the
   STM32, here on a SiFive part.

2. **Change the blink pin or message.** Edit `src/blink.c` — set
   `LED_PIN` to `21` (blue) or change the banner text — save, re-run
   `lab 04`. The Makefile rebuilds only the changed file.

3. **Speed it up.** The `delay()` busy-loop count controls the
   blink rate. Halve `200000` and re-run; `LED on`/`LED off` scrolls
   twice as fast.

4. **Drive the UART without the CPU.** Pause, then poke `txdata`
   directly:

   ```text
   pause
   sysbus WriteByte 0x10013000 0x48      # 'H'
   sysbus WriteByte 0x10013000 0x69      # 'i'
   sysbus WriteByte 0x10013000 0x0A      # '\n'
   ```

   `Hi` appears in the analyzer / log — you bypassed the core and
   drove the peripheral straight from the simulator.

5. **Inspect the RV32 ELF.** From a Codespace terminal:

   ```bash
   riscv64-unknown-elf-objdump -d ~/work/04-sifive-fe310/blink.elf | less
   riscv64-unknown-elf-nm ~/work/04-sifive-fe310/blink.elf | sort
   ```

   Confirm `_start` is at `0x80000000` and the code fits inside the
   16 KiB DTIM.

## 7. Stopping cleanly

The blink loop never returns, so just interrupt and quit:

```text
quit
```

…or `Ctrl-D`. On to `lab 05`, where the FE310's timer fires the
LED from an interrupt handler instead of a busy-loop.

## What this lab proves

- You can build and run bare-metal firmware on a **real, named SoC**
  (the SiFive FE310) using nothing but its datasheet addresses.
- Peripheral register maps are vendor-specific: the SiFive UART and
  GPIO look nothing like the STM32 or the NS16550, and the firmware
  has to match.
- RV32 vs RV64 is a one-line toolchain change (`-march`/`-mabi`) plus
  word-size care in the startup assembly.
- The same Renode primitives scale from a 3-line custom SoC to a
  real-chip platform without changing shape.
