# Lab 00-Demo — Bare-metal ARM Cortex-A9 + SmartTimer MMIO

A small scratch/demo lab: a bare-metal ARM (Cortex-A9) program that
reads and writes a block of memory-mapped registers — a stand-in for
a "SmartTimer" peripheral. It's the simplest possible illustration of
**memory-mapped I/O (MMIO)** on ARM, and a natural lead-in to lab 07
(where the placeholder becomes a *real* peripheral model).

> **Note on the "SmartTimer".** In `smarttimer_arm.repl` the SmartTimer
> is a `Memory.MappedMemory` window at `0x70000000`, **not** an active
> peripheral. So reads return whatever the firmware last wrote (RAM
> semantics) and nothing counts or fires an interrupt on its own. The
> goal here is to exercise the MMIO access path and observe it — see
> lab 07 to replace this with a behavioural C# timer that actually
> ticks and interrupts the CPU.

## 1. Run it

`00-Demo` is wired into the lab dispatcher:

```bash
lab 00          # canonical
lab demo        # back-compat alias (works the same)
```

This mirrors `/labs/00-Demo/` into `~/work/00-Demo/`, runs `make`
(cross-compiles with `arm-none-eabi-gcc` into `sw/bare.elf`), then
launches `renode --console bare_demo.resc`.

Or run it by hand after the first invocation:

```bash
cd ~/work/00-Demo
make
renode --console bare_demo.resc
```

The startup script (`bare_demo.resc`) loads the ELF but **does not
call `start`** (the last line is commented out), so the CPU sits at
the entry point waiting. At the monitor prompt, type:

```text
start
```

## 1a. Invoking the monitor in Codespaces

In Codespaces there is no native Renode GUI window — the labs run the
**Renode monitor as a text console inside your terminal** (the
dispatcher passes `--console --disable-gui`). Three ways to reach it,
easiest first:

**Console monitor (recommended).** Open a terminal and run `lab 00`.
The `--console` flag makes Renode's monitor appear inline as the
`(machine-0)` prompt in that same terminal, with the platform and ELF
already loaded. Type `start` to run, then drive it with monitor
commands (`pause`, `sysbus.cpu PC`, `sysbus ReadDoubleWord 0x70000000`,
`quit`, …). Running `lab monitor` (or `renode --console`) gives an
empty monitor with no script loaded.

**Telnet monitor over a forwarded port.** The devcontainer forwards
port **1234** ("Renode telnet monitor"). Expose the monitor on that
socket instead of stdio:

```bash
cd ~/work/00-Demo && make
renode --disable-gui --port 1234 bare_demo.resc
```

then connect from another terminal:

```bash
telnet localhost 1234
```

**noVNC desktop (GUI analyzers).** Port **6080** (path `/vnc.html`)
serves a browser desktop for Renode's GUI windows. The Demo has no UART
and the dispatcher runs `--disable-gui`, so the console monitor above is
the one you want; the desktop matters only for labs that pop UART/GUI
analyzers.

## 2. What just happened

`bare_demo.resc` does the usual three steps, plus turns on verbose
logging so you can watch the program:

```
mach create
machine LoadPlatformDescription @smarttimer_arm.repl
sysbus.cpu LogFunctionNames true     # log every C function entry
sysbus LogAllPeripheralsAccess true  # log bus accesses
logFile @/tmp/bare_demo.log
sysbus LoadELF @sw/bare.elf
```

The platform (`smarttimer_arm.repl`):

```
cpu:        CPU.ARMv7A @ sysbus        # cortex-a9
systemram:  Memory.MappedMemory @ 0x80000000  size 0x04000000  (64 MiB)
smarttimer: Memory.MappedMemory @ 0x70000000  size 0x1000      (placeholder)
```

The firmware (`src/main.c`) writes the SmartTimer's CONTROL, PERIOD,
DUTY and STATUS registers, then reads them all back. Because the
region is plain memory, the read-backs return exactly what was
written.

## 3. Files

| File | What it is |
|---|---|
| `src/start.S` | Cortex-A9 reset code — set `sp`, zero `.bss`, call `main`, idle in `wfi`. |
| `src/main.c` | Writes/reads the SmartTimer MMIO registers at `0x70000000`. |
| `src/link.ld` | Linker script — code/data into RAM at `0x80000000`; defines `_stack_top`, `_bss_*`. |
| `Makefile` | ARM cross-compile rules → `sw/bare.elf` + `sw/bare.bin`. **Real TABs in recipes.** |
| `smarttimer_arm.repl` | Platform: Cortex-A9 + system RAM + SmartTimer memory window. |
| `bare_demo.resc` | Startup script (loads the ELF; you type `start`). |

SmartTimer register map (as used by the firmware):

| Offset | Name | Bits used |
|---|---|---|
| `0x00` | `CONTROL` | bit0 `ENABLE`, bit1 `IRQ_ENABLE` |
| `0x04` | `PERIOD` | period value |
| `0x08` | `DUTY` | duty value |
| `0x0C` | `STATUS` | bit0 `PENDING` |

## 3a. Inspecting the ELF with objdump

Before (or instead of) running it, you can look inside `sw/bare.elf`
with the cross-toolchain's binary utilities. Use the **`arm-none-eabi-`**
prefixed tools, not the host ones, so they understand ARM code.

**Disassemble the code** (`-d`):

```bash
arm-none-eabi-objdump -d sw/bare.elf
```

You'll see `_start` at `0x80000000` (the RAM base from `link.ld`),
followed by `main`, `mmio_write32`, and `mmio_read32`.

**Disassemble interleaved with C source** (`-S`, works because we build
with `-g`):

```bash
arm-none-eabi-objdump -S sw/bare.elf
```

**List the sections and their addresses/sizes** (`-h`):

```bash
arm-none-eabi-objdump -h sw/bare.elf
```

Shows `.text`, `.rodata`, `.data`, `.bss` placed in RAM — the layout
`link.ld` dictated.

**Show the file/entry-point header** (`-f`):

```bash
arm-none-eabi-objdump -f sw/bare.elf      # 'start address 0x80000000'
```

**List symbols** (`-t`, or the dedicated `nm` tool):

```bash
arm-none-eabi-objdump -t sw/bare.elf
arm-none-eabi-nm -n sw/bare.elf           # symbols sorted by address
```

Look for `_start`, `main`, `_stack_top`, `_bss_start`, `_bss_end` —
the symbols `start.S` and the linker script define.

**For ELF metadata specifically**, `readelf` is the sharper tool:

```bash
arm-none-eabi-readelf -h sw/bare.elf      # ELF header (class, machine, entry)
arm-none-eabi-readelf -l sw/bare.elf      # program headers (load segments)
arm-none-eabi-readelf -S sw/bare.elf      # section headers
```

> See `BUILD_INTERNALS.md` in the ./doc folder for a deeper explanation of
> what an ELF file is and what each of these parts contains.

## 4. What to observe

Once you type `start`, `main()` runs in a fraction of a second and
then the core parks in `wfi`. Two ways to see it work:

**Function-name trace (in the console):** because the ELF has symbols
and `LogFunctionNames` is on, you'll see entries for `_start`, `main`,
`mmio_write32`, `mmio_read32` scroll by — the program executing
instruction-by-instruction through the model.

The script also logs traces in /tmp/bare_demo.log.

**Read the registers from the monitor:** the values the firmware wrote
are still in the SmartTimer memory window. After `start`:

```text
pause
sysbus ReadDoubleWord 0x70000000      # CONTROL -> 0x3 (ENABLE | IRQ_ENABLE)
sysbus ReadDoubleWord 0x70000004      # PERIOD  -> 0xF4240 (1000000)
sysbus ReadDoubleWord 0x70000008      # DUTY    -> 0x7A120 (500000)
sysbus ReadDoubleWord 0x7000000C      # STATUS  -> 0x1
```

You can also poke it yourself, exactly as the CPU would:

```text
sysbus WriteDoubleWord 0x70000004 0x1234
sysbus ReadDoubleWord  0x70000004      # -> 0x1234
```

## 5. Useful monitor commands

| Command | What it does |
|---|---|
| `start` / `pause` | Release / freeze the CPU. |
| `peripherals` | Show the platform tree (cpu, systemram, smarttimer). |
| `sysbus.cpu PC` | Read the program counter. |
| `sysbus.cpu Step` | Single-step one ARM instruction. |
| `sysbus.cpu LogFunctionNames true` | Toggle function-entry logging. |
| `sysbus ReadDoubleWord 0x70000000` | Read a SmartTimer register. |
| `sysbus WriteDoubleWord 0x70000000 0x3` | Write a SmartTimer register. |
| `quit` | Exit Renode. |

## 6. Next step

The SmartTimer here is inert memory. **Lab 07** shows how to make a
block like this *behave* — a hand-written C# peripheral with the same
register layout that actually counts and raises an interrupt the CPU
handles. Compare `src/main.c` here with lab 07's firmware: the MMIO
access pattern is identical; only the peripheral on the other end
changes from a memory stub to a live model.

## What this lab proves

- MMIO is just loads and stores to fixed addresses; bare-metal ARM
  reaches a peripheral exactly like it reaches RAM.
- A peripheral can be stubbed as a memory window for early bring-up,
  then upgraded to a behavioural model without changing the firmware.
- The Cortex-A9 startup (`start.S`) + linker script (`link.ld`)
  pattern is the same shape as the RISC-V labs, just ARM instructions.
