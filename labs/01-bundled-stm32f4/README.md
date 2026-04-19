# Lab 01 — Bundled STM32F4 Discovery demo

The simplest Renode invocation. Loads a pre-built ARM Cortex-M4
firmware on the STM32F4 Discovery board model that ships with
Renode. Use this to confirm the image is working before moving on
to anything custom.

## 1. Bring the board up

```bash
lab 01
```

This runs:

```
renode --plain --disable-gui --console /labs/01-bundled-stm32f4/stm32f4.resc
```

which simply `include`s the bundled
`scripts/single-node/stm32f4_discovery.resc` shipped inside
`/opt/renode/`. After a couple of seconds you will see:

- A `(machine-0)` prompt — the **Renode monitor** is now waiting
  for your commands.
- UART output of the demo firmware (the bundled binary prints
  `Hello world!` from a FreeRTOS task on **USART2** roughly once
  per second).
- An analyzer window for `sysbus.usart2` opens in the noVNC
  desktop (port **6080**, path `/vnc.html`). If the port is not
  forwarded, the analyzer is not visible — that's fine, the next
  section shows how to read UART from the monitor itself.

If `lab 01` fails, nothing else in this lab will work either. Stop
and fix the environment first (see the troubleshooting table in
the top-level [`README.md`](../../README.md)).

## 2. What just happened

The bundled `.resc` did three things behind the scenes:

1. `mach create "STM32F4_Discovery"` — created a virtual machine.
2. `machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl`
   — instantiated CPU, RAM, USARTs, GPIOs, timers, etc.
3. `sysbus LoadELF @...stm32f4discovery-demo.elf` followed by
   `start` — loaded the firmware and started the simulated CPU.

You can re-run any of these by hand from the monitor; they are
the building blocks you'll use in labs 02 and 03.

## 3. First demo: watch the firmware run

At the `(machine-0)` prompt, try these in order. Each line is a
**Renode monitor command** — Renode is paused-on-prompt only when
you ask it to be, so the firmware is already executing.

```text
peripherals
```

Lists every peripheral in the platform tree (USARTs, GPIO ports,
RCC, NVIC, the Cortex-M4 itself, …). Confirms what the `.repl`
actually built.

```text
sysbus.cpu PC
```

Prints the current program counter. Run it twice — the value
changes, proving the CPU is really executing instructions, not
frozen.

```text
pause
sysbus.cpu PC
sysbus.cpu Step
sysbus.cpu PC
start
```

Pause the machine, single-step one instruction, observe PC
advanced, then resume. This is the bare minimum debug loop.

## 4. See the UART traffic from the monitor

Even without the GUI analyzer you can capture UART output:

```text
pause
sysbus.usart2 CreateFileBackend @/tmp/uart2.log true
start
```

Wait a few seconds, then in **another terminal** in the
Codespace:

```bash
tail -f /tmp/uart2.log
```

You should see `Hello world!` lines streaming in. Stop with
`Ctrl-C` in the tail window; the simulation keeps running.

Alternatively, redirect UART to a TCP socket and `telnet` into
it:

```text
pause
emulation CreateServerSocketTerminal 3456 "uart-term"
connector Connect sysbus.usart2 uart-term
start
```

```bash
telnet localhost 3456    # in another Codespace terminal
```

## 5. Useful monitor commands to try

A short menu of things worth experimenting with on this board.
Type `help` at any time for the full list, or `help <command>`
for usage.

| Command | What it does |
|---|---|
| `help` | List every monitor command. |
| `help peripherals` | Per-command help. Works for any command. |
| `peripherals` | Tree of every peripheral the `.repl` instantiated. |
| `machine` | Show machine name, status, loaded ELF. |
| `mach` | Switch between machines (only one here, but lab 03 has more). |
| `pause` / `start` | Freeze and resume the CPU. |
| `sysbus.cpu PC` | Read the program counter. |
| `sysbus.cpu Step` | Execute exactly one instruction. |
| `sysbus.cpu Step 100` | Step 100 instructions. |
| `sysbus.cpu LogFunctionNames true` | Log every C function entry to the console (needs symbols from the ELF — the bundled demo has them). |
| `sysbus ReadDoubleWord 0x40023800` | Peek at the RCC base register. Confirms memory-mapped I/O works. |
| `sysbus WriteDoubleWord 0x40020C14 0x00008000` | Toggle GPIOD bit 15 (blue LED) directly from the monitor. |
| `sysbus.gpioPortD` | Inspect the GPIO port object; tab-complete to see its methods. |
| `logLevel 0 sysbus.usart2` | Verbose-log every read/write to USART2. Set back with `logLevel 3 sysbus.usart2`. |
| `showAnalyzer sysbus.usart2` | Pop the UART analyzer window (only visible in the noVNC tab). |
| `emulation RunFor "0.5"` | Run for exactly 500 ms of virtual time, then auto-pause. Cycle-accurate determinism. |
| `machine StatisticalProfiler` | Start a sampling profiler; dump with `WriteToFile`. |
| `quit` | Exit Renode. |

## 6. Mini-experiments (try at least one)

1. **Blink an LED from the monitor.** Pause the CPU, then write
   to the GPIOD BSRR register to toggle PD12–PD15 (the four
   on-board LEDs):

   ```text
   pause
   sysbus WriteDoubleWord 0x40020C18 0x00001000   # set PD12
   sysbus WriteDoubleWord 0x40020C18 0x10000000   # reset PD12
   ```

   The `gpioPortD` analyzer (if visible in the GUI) will flash
   the corresponding LED.

2. **Time-travel.** Pause, then advance simulated time in
   precise chunks:

   ```text
   pause
   emulation RunFor "0.100"
   sysbus.cpu PC
   emulation RunFor "0.100"
   sysbus.cpu PC
   ```

   Note the same input always produces the same PC sequence —
   that's the cycle-accurate determinism you're paying Renode
   for.

3. **Trace function calls.** With the ELF symbols already
   loaded, turn on function-name logging:

   ```text
   sysbus.cpu LogFunctionNames true
   ```

   The console fills with names like `vTaskDelay`,
   `prvIdleTask`, `xQueueGenericSend` — you're watching FreeRTOS
   schedule itself in real time. Turn off with
   `sysbus.cpu LogFunctionNames false`.

4. **Reset and replay.** From the monitor:

   ```text
   machine Reset
   start
   ```

   The firmware re-runs from the ELF entry point; UART output
   restarts from `Hello world!` #1.

## 7. Exit cleanly

```text
quit
```

…or just `Ctrl-D` at the monitor prompt. The Codespace stays
alive; you're back at the bash prompt and ready for `lab 02`.

## What this lab proves

- Mono runtime is healthy.
- Renode binary launches without errors.
- The `scripts/` tree shipped inside `/opt/renode/` is reachable.
- You know the three primitives (`mach create`,
  `LoadPlatformDescription`, `LoadELF` + `start`) that every
  later lab is built on.
- You can drive a simulated MCU from the monitor: read/write
  memory, single-step, capture UART, advance virtual time.
