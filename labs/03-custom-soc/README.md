# Lab 03 — Custom RV64 SoC + bare-metal hello world

Build a tiny RISC-V program from source and run it on a 9-line
hand-written Renode platform: one CPU core, 64 MiB of RAM, and an
NS16550 UART. No bootloader, no kernel — just `_start` that
zeroes `.bss`, calls `main()`, and pokes characters into a
hard-coded UART register.

Where lab 01 ran a *bundled* board and lab 02 ran a *bundled*
SoC, this lab is the first one where **you own every line** —
the C code, the linker script, the platform description, and the
startup script.

## 1. Bring the SoC up

```bash
lab 03
```

This does three things in order:

1. Mirror `/labs/03-custom-soc/` (read-only image content) into
   `~/work/03-custom-soc/` (your editable scratch tree). On
   subsequent `lab 03` runs, `cp -ru` reuses the existing copy
   and never clobbers your edits.
2. `make` in `~/work/03-custom-soc/` — cross-compiles
   `src/start.S` + `src/hello.c` with `riscv64-unknown-elf-gcc`
   into `hello.elf` and `hello.bin`.
3. `renode --plain --disable-gui --console renode/mini-rv.resc`
   — launches the `mini-rv` platform, loads `hello.elf` at the
   RAM base (`0x80000000`), opens a UART analyzer, and starts
   the CPU.

So your editable copy of every file in this lab lives at
`~/work/03-custom-soc/`. After the first `lab 03`, you can also
run it directly:

```bash
cd ~/work/03-custom-soc
make
renode --console renode/mini-rv.resc
```

Within a second you should see in the **`uart` analyzer** window
(noVNC tab on port 6080, path `/vnc.html`):

```
*** Hello from custom RV64 SoC! ***
UART @ 0x10000000, RAM @ 0x80000000
```

If the noVNC tab is unavailable, jump to section 4 to read the
UART from the monitor instead.

## 2. What just happened

The `.resc` script is short enough to read in one screen — open
`renode/mini-rv.resc`:

```
mach create "mini-rv"
machine LoadPlatformDescription @renode/mini-rv.repl
sysbus LoadELF @hello.elf
showAnalyzer uart
start
```

The `@` paths are *relative*, so they resolve against whatever
directory Renode is launched from. The `lab 03` dispatcher
`cd`s into `~/work/03-custom-soc/` first, so `@hello.elf` finds
the freshly built ELF in the work tree.

Same three primitives as lab 01 and lab 02:

1. `mach create` — empty virtual machine.
2. `LoadPlatformDescription` — instantiate the SoC from
   `mini-rv.repl` (a 9-line file: CPU + RAM + UART, nothing
   else).
3. `LoadELF` + `start` — drop the firmware at `0x80000000` and
   run.

The platform itself (`renode/mini-rv.repl`):

```
cpu: CPU.RiscV64 @ sysbus
    cpuType: "rv64imac"
    privilegeArchitecture: PrivilegeArchitecture.Priv1_10
    timeProvider: empty

ram: Memory.MappedMemory @ sysbus 0x80000000
    size: 0x4000000

uart: UART.NS16550 @ sysbus 0x10000000
```

That is **the whole SoC**. Nine lines. The CPU fetches from
`0x80000000`, instructions execute, stores to `0x10000000` come
out a UART. The bones of every embedded chip you have ever used.

## 3. Files

| File | What it is |
|---|---|
| `src/start.S` | Reset vector — sets `sp`, zeros `.bss`, jumps to `main`, halts in `wfi`. |
| `src/hello.c` | The C program; pokes the NS16550 THR/LSR registers directly. |
| `src/link.ld` | Linker script — `.text` at `0x80000000`, defines `_stack_top`, `_bss_start`, `_bss_end`. |
| `Makefile` | Cross-compile rules. **Recipes use real TABs** — your editor must not convert them to spaces. |
| `renode/mini-rv.repl` | Renode platform description (CPU + RAM + UART). |
| `renode/mini-rv.resc` | Renode startup script (the 5 lines above). |

## 4. Read UART from the monitor (no GUI needed)

`Ctrl-C` once at the running monitor only interrupts the
simulation; the prompt comes back. Then:

```text
pause
sysbus.uart CreateFileBackend @/tmp/mini-rv.log true
machine Reset
sysbus LoadELF @hello.elf
start
```

In **another Codespace terminal**:

```bash
cat /tmp/mini-rv.log
```

The `*** Hello from custom RV64 SoC! ***` banner is in the file.
(`CreateFileBackend` only captures bytes written *after* it is
attached, which is why we reset and reload the ELF.)

For an interactive socket terminal:

```text
pause
emulation CreateServerSocketTerminal 3458 "mini-rv-tty"
connector Connect sysbus.uart mini-rv-tty
machine Reset
sysbus LoadELF @hello.elf
start
```

```bash
telnet localhost 3458    # in another Codespace terminal
```

## 5. Useful monitor commands to try

Same shape as labs 01 and 02. The single-CPU, no-OS environment
makes `Step` and raw memory pokes especially fun here.

| Command | What it does |
|---|---|
| `peripherals` | Confirms only `cpu`, `ram`, `uart` exist. Compare to lab 02! |
| `sysbus.cpu PC` | Read program counter. |
| `pause` / `start` | Freeze and resume the CPU. |
| `sysbus.cpu Step` | Single-step one RISC-V instruction. |
| `sysbus.cpu Step 100` | Step 100 instructions. |
| `sysbus.cpu LogFunctionNames true` | Log every function entry (`_start`, `main`, `uart_putc`, …). The ELF has full symbols. |
| `sysbus ReadDoubleWord 0x80000000` | Read the first 4 bytes of RAM — the reset vector instruction. |
| `sysbus WriteByte 0x10000000 0x41` | Write 'A' directly to the UART data register from the monitor. The character appears in the analyzer / log. |
| `sysbus ReadByte 0x10000005` | Read the NS16550 line-status register; bit 5 (`THRE`) tells you the TX FIFO is empty. |
| `logLevel 0 sysbus.uart` | Verbose-log every UART access. Reset with `logLevel 3 sysbus.uart`. |
| `emulation RunFor "0.001"` | Advance exactly 1 ms of simulated time, then auto-pause. |
| `machine Reset` | Reset the CPU; PC jumps back to `_start`. ELF stays loaded. |
| `quit` | Exit Renode. |

## 6. Mini-experiments (try at least one)

1. **Edit-build-run loop.** In VS Code, open
   `~/work/03-custom-soc/src/hello.c` and change the message:

   ```c
   uart_puts("\n*** My very own RISC-V SoC ***\n");
   ```

   Save, then re-run `lab 03`. The Makefile rebuilds only the
   changed file; you should see the new banner within a second.

   > Edit the **work-tree** copy at
   > `~/work/03-custom-soc/src/hello.c`, not the image copy at
   > `/labs/03-custom-soc/src/hello.c`. The image copy is
   > read-only and gets reset on container rebuild; the work-tree
   > copy persists across Codespace stop/start. To make changes
   > survive Codespace **deletion**, copy them back into
   > `/workspaces/renode-lab/labs/03-custom-soc/` and `git push`.

2. **Talk to the UART from the monitor.** Pause, then write
   bytes by hand:

   ```text
   pause
   sysbus WriteByte 0x10000000 0x48      # 'H'
   sysbus WriteByte 0x10000000 0x69      # 'i'
   sysbus WriteByte 0x10000000 0x0A      # '\n'
   ```

   `Hi` appears in the analyzer / log. You just bypassed the
   CPU entirely and drove the peripheral from the simulator.

3. **Single-step the reset vector.** Pause immediately after
   `start`, then:

   ```text
   pause
   machine Reset
   sysbus.cpu PC          # 0x80000000 — _start
   sysbus.cpu Step
   sysbus.cpu PC          # next instruction
   sysbus.cpu Step 20
   sysbus.cpu PC          # somewhere inside main()
   ```

   You can literally watch the `la sp, _stack_top` /
   `la t0, _bss_start` / `call main` sequence from `start.S`
   execute one instruction at a time.

4. **Add a second peripheral.** Open `renode/mini-rv.repl` and
   append:

   ```
   uart2: UART.NS16550 @ sysbus 0x10001000
   ```

   Re-run `lab 03`. `peripherals` now shows two UARTs. Modify
   `hello.c` to also write to `0x10001000` and you have a
   two-UART board — that easily.

5. **Inspect the ELF before loading it.** From a Codespace
   terminal:

   ```bash
   riscv64-unknown-elf-objdump -d labs/03-custom-soc/hello.elf | less
   riscv64-unknown-elf-nm labs/03-custom-soc/hello.elf | sort
   ```

   Confirm `_start` is at `0x80000000` and `main` lives a few
   bytes higher. That's the address `sysbus.cpu PC` will print
   immediately after `machine Reset`.

## 7. Stopping cleanly

The bare-metal program ends in `wfi; j hang` (see
`src/start.S`), so the CPU parks itself with no further output.
At the Renode monitor:

```text
quit
```

…or `Ctrl-D`. Done.

## What this lab proves

- A "real" SoC, in modelling terms, is just memory regions and
  peripherals on a bus. The 9-line `.repl` is enough to produce
  a CPU that fetches, executes, and emits characters.
- The toolchain → linker script → ELF → Renode pipeline works
  end-to-end with components you can read in five minutes.
- The same three Renode primitives (`mach create`,
  `LoadPlatformDescription`, `LoadELF` + `start`) scale from
  this 9-line platform up to the multi-hart Linux SoC of lab 02
  without changing shape.
- You can extend the SoC by editing one `.repl` file. Adding a
  peripheral is one line.
