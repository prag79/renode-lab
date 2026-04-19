# Lab 03 — Custom RV64 SoC + bare-metal hello world

Build a tiny RISC-V program from source and run it on a 9-line
hand-written Renode platform: one CPU core, 64 MiB of RAM, and an
NS16550 UART. No bootloader, no kernel — just `_start` that calls
`main()` and prints to a hard-coded UART address.

## Run it

```bash
lab 03
```

Behind the scenes this does:

1. `make` in this directory — cross-compiles `start.S` + `hello.c`
   with `riscv64-unknown-elf-gcc` into `hello.elf`.
2. `renode --plain --disable-gui --console renode/mini-rv.resc` —
   launches the `mini-rv` platform, loads `hello.elf` at the RAM
   base (`0x80000000`), opens a UART analyzer, and starts the CPU.

You should see in the UART analyzer (noVNC tab on port 6080):

```
*** Hello from custom RV64 SoC! ***
UART @ 0x10000000, RAM @ 0x80000000
```

## Files

| File | What it is |
|---|---|
| `src/start.S` | Reset vector — sets `sp`, zeros `.bss`, jumps to `main` |
| `src/hello.c` | The C program; pokes the UART directly |
| `src/link.ld` | Linker script — puts `.text` at `0x80000000` (RAM base) |
| `Makefile` | Cross-compile rules (real TABs in recipes!) |
| `renode/mini-rv.repl` | Renode platform definition (CPU + RAM + UART) |
| `renode/mini-rv.resc` | Renode startup script |

## Modify and re-run

Edit any of the source files in your VS Code editor, then re-run
`lab 03`. Changes pick up immediately — no image rebuild needed.

Try changing the UART message in `src/hello.c` to confirm the
edit-build-run loop works.

## What this teaches

That a "real" SoC, in modelling terms, is just memory regions and
peripherals on a bus. The 9-line `mini-rv.repl` is enough to
produce a CPU that reads instructions, executes them, and emits
characters out a UART — the same bones as the much larger HiFive
model in `lab 02`.
