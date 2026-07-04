# Lab 01 — Bundled STM32F4 Discovery demo

The simplest Renode invocation. Loads a pre-built ARM Cortex-M4
firmware on the STM32F4 Discovery board model that ships with
Renode. Use this to confirm the image is working before moving on
to anything custom.

## 1. Bring the board up

```bash
lab 01
```

This runs (after mirroring `/labs/01-bundled-stm32f4/` into your
editable scratch tree at `~/work/01-bundled-stm32f4/` on first
invocation):

```
cd ~/work/01-bundled-stm32f4
renode $RENODE_GUI_FLAGS --console stm32f4.resc
```

`$RENODE_GUI_FLAGS` is empty when the environment variable
`LAB_GUI=1` and `--disable-gui` otherwise (headless console
mode is the default). See section 2 below for what each mode
gives you and how to switch.

`stm32f4.resc` simply `include`s the bundled
`scripts/single-node/stm32f4_discovery.resc` shipped inside
`/opt/renode/`. Edit `stm32f4.resc` (or anything else under
`~/work/01-bundled-stm32f4/`) and re-run `lab 01` — your changes
are preserved (the dispatcher uses `cp -ru`, which never
clobbers existing files). The bundled firmware is a pre-built **Contiki**
image (a tiny IoT OS) — *not* FreeRTOS — fetched once from
Antmicro's release server and cached on disk. After a couple of
seconds you will see:

- A `(STM32F4_Discovery)` prompt — the **Renode monitor** is now
  waiting for your commands.
- In GUI mode (`LAB_GUI=1`), an analyzer window for
  `sysbus.uart4` may appear in the noVNC desktop (port **6080**,
  path `/vnc.html`). The Contiki console is wired to **`uart4`**
  — *not* `usart2`. (`usart2` exists in the platform but the
  demo firmware does not use it, so it stays silent. If you
  attach a backend to `usart2` you'll see nothing — section 5
  covers the right command.)
- The script does not call `start`, so the CPU may be paused at
  the first prompt. If `sysbus.cpu PC` returns the same value
  twice in a row, type `start`. (`lab 01` on the current image
  may auto-start; either way, `start` is idempotent.)

If `lab 01` fails, nothing else in this lab will work either. Stop
and fix the environment first (see the troubleshooting table in
the top-level [`README.md`](../../README.md)).

## 2. Headless vs GUI mode

Renode can run in two modes on the Codespace. Pick one and stick
with it for the lab session — mixing is fine but requires a
Renode restart.

| Mode | Env var | What you get | Trade-off |
|---|---|---|---|
| **Headless** (default) | `LAB_GUI=0` or unset | Monitor prompt in your terminal, all output as text. UART logged via `CreateFileBackend` / socket. GPIO / LED observed via `logLevel -1` or direct `State` reads. | No GUI dependency, works in any terminal, deterministic paste-friendly output for automation and grading. |
| **GUI in noVNC** | `LAB_GUI=1` | Monitor in your terminal **plus** Renode's GUI analyzer windows (UART terminal window, GPIO widgets where supported) rendered into the Xvfb desktop, accessible via the noVNC tab on forwarded port **6080**, path `/vnc.html`. | Requires `entrypoint.sh` to have brought up Xvfb+fluxbox+x11vnc+websockify. Not all peripheral analyzers ship with every Renode build — if `showAnalyzer …` returns `No backend found for …`, that specific plugin isn't available in this image and you must fall back to the headless method. |

To switch to GUI mode from a running Codespace:

```bash
quit                      # exit any running lab NN
export LAB_GUI=1
entrypoint.sh true        # brings up Xvfb, fluxbox, x11vnc,
                          # websockify on port 6080 if not yet up
pgrep -af 'Xvfb|x11vnc'   # sanity check: should see both
lab 01                    # relaunches Renode without --disable-gui
```

Then open the noVNC tab (Ports panel → 6080 → globe icon → append
`/vnc.html` if needed). The UART analyzer window is the one
plugin known to work reliably; you'll see Contiki's boot log
scrolling there instead of (or in addition to) your terminal.

To go back to headless for later labs:

```bash
quit
export LAB_GUI=0
lab 01                    # or whichever lab
```

**Recommendation for this lab (and for graded work):** stay in
headless mode. Every experiment in sections 3–6 works there and
produces clean, copy-pasteable output. GUI mode is a convenience,
not a requirement.

## 3. What just happened

The bundled `.resc` did three things behind the scenes:

1. `mach create "STM32F4_Discovery"` — created a virtual machine.
2. `machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl`
   — instantiated CPU, RAM, USARTs, GPIOs, timers, etc.
3. `sysbus LoadELF @...stm32f4discovery-demo.elf` (elf is downloaded from antmicro online repo link) followed by
   `start` — loaded the firmware and started the simulated CPU.

You can re-run any of these by hand from the monitor; they are
the building blocks you'll use in labs 02 and 03.

## 4. First demo: watch the firmware run

> **Always type `start` first.** The bundled `stm32f4_discovery.resc`
> loads the ELF and stops — it never calls `start` itself. Until
> you do, the Cortex-M4 is held at reset and `sysbus.cpu PC`
> returns `0x00000000`. That's not a broken simulation; the CPU
> simply hasn't fetched its reset vector yet.

At the `(STM32F4_Discovery)` prompt, type these in order. Each
line is a **Renode monitor command**.

```text
start
peripherals
```

`start` releases the CPU from reset; the Cortex-M boot sequence
runs (`SP = mem[0x00000000]`, `PC = mem[0x00000004]`) and
execution jumps into Contiki in flash.

`peripherals` lists every peripheral in the platform tree
(USARTs, GPIO ports, RCC, NVIC, the Cortex-M4 itself, …) — it
confirms what the `.repl` actually built.

```text
sysbus.cpu PC
sysbus.cpu PC
```

After `start`, PC is somewhere in flash (typically `0x0800xxxx`).
Run the command twice — the value **changes**, proving the CPU
is really executing instructions, not frozen. If it still shows
`0x00000000`, you skipped `start`; type it now.

```text
pause
sysbus.cpu PC
sysbus.cpu Step
sysbus.cpu PC
start
```

Pause the machine, single-step one instruction, observe PC
advanced, then resume. This is the bare minimum debug loop.

## 5. See the UART traffic from the monitor

> **Important:** the Contiki console on this board is on
> **`uart4`**, not `usart2`. Attaching a backend to `usart2`
> will produce an empty file and no telnet output — that
> peripheral exists in the platform but the demo firmware never
> writes to it.

Even without the GUI analyzer you can capture UART output:

```text
pause
sysbus.uart4 CreateFileBackend @/tmp/uart4.log true

```

Wait a few seconds, then in **another terminal** in the
Codespace:

```bash
tail -f /tmp/uart4.log
```
Then in the renode monitor 
```text
machine Reset
start

```

You should see Contiki's boot banner (something like
`Starting Contiki-NG-...`, MAC/IP address, then periodic
process output) streaming in. Stop with `Ctrl-C` in the tail
window; the simulation keeps running.

Alternatively, redirect UART to a TCP socket and `telnet` into
it:

```text
pause
emulation CreateServerSocketTerminal 3456 "uart-term"
connector Connect sysbus.uart4 uart-term
start
```

```bash
telnet localhost 3456    # in another Codespace terminal
```

If you already attached the GUI analyzer to `uart4` (the bundled
script does, via `showAnalyzer sysbus.uart4`) you may see a
warning about a second backend — that's harmless for read-only
inspection.

## 6. Useful monitor commands to try

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
| `sysbus WriteDoubleWord 0x40020C18 0x00001000` | Set GPIOD pin 12 (turn the user LED on) via the BSRR register. |
| `sysbus WriteDoubleWord 0x40020C18 0x10000000` | Reset GPIOD pin 12 (turn the user LED off) via BSRR. |
| `sysbus.gpioPortD.UserLED State` | Read the current LED state (`True`/`False`). |
| `sysbus.gpioPortD` | Inspect the GPIO port object; tab-complete to see its methods. |
| `logLevel 0 sysbus.uart4` | Verbose-log every read/write to the Contiki console UART. Set back with `logLevel 3 sysbus.uart4`. |
| `showAnalyzer sysbus.uart4` | Pop the UART analyzer window into noVNC. **Requires `LAB_GUI=1`** — otherwise you get `No backend found`. See "Headless vs GUI mode" below. |
| `emulation RunFor "0.5"` | Run for exactly 500 ms of virtual time, then auto-pause. Cycle-accurate determinism. |
| `machine StatisticalProfiler` | Start a sampling profiler; dump with `WriteToFile`. |
| `quit` | Exit Renode. |

## 7. Mini-experiments (try at least one)

1. **Blink an LED from the monitor.** The board model wires
   `UserLED` to `gpioPortD` pin 12 (PD12 — the green LED on a
   real STM32F4 Discovery). The Contiki demo never blinks it on
   its own, so we'll drive it from the simulator.

   First quiet the UART log spam so the prompt stays usable, and
   pause the CPU so our writes aren't immediately overwritten by
   the firmware:

   ```text
   logLevel 3 sysbus.uart4
   pause
   ```

   GPIOD lives at `0x40020C00` on the STM32F4. The relevant
   registers are:

   | Offset | Reg | Purpose |
   |---|---|---|
   | `0x14` | `ODR`  | Output data — bit *n* = pin *n* current level |
   | `0x18` | `BSRR` | Atomic set/reset — low 16 bits *set* pins, high 16 bits *reset* pins |

   So the absolute address of GPIOD's BSRR is `0x40020C18`.

   ### a) One-shot toggle + read-back

   The cleanest proof the LED responded:

   ```text
   sysbus.gpioPortD.UserLED State          ; before  -> False
   sysbus WriteDoubleWord 0x40020C18 0x00001000   ; set PD12 (LED on)
   sysbus.gpioPortD.UserLED State          ; after   -> True
   sysbus WriteDoubleWord 0x40020C18 0x10000000   ; reset PD12 (LED off)
   sysbus.gpioPortD.UserLED State          ; after   -> False
   ```

   `True` / `False` flipping in the monitor *is* the LED
   blinking, just as observed by the model. Same physical event,
   no GUI required.

   ### b) Live blink loop you can watch

   Toggle 10 times with a 200 ms simulated delay between each
   transition. Two files ship with the lab:

   - [`blink.py`](blink.py) — the actual Python loop (executes
     `WriteDoubleWord` / `RunFor` / `State` for ten iterations).
   - [`blink.resc`](blink.resc) — a one-line loader:
     `python "execfile('blink.py')"`.

   Two files instead of one because the Renode monitor's `.resc`
   tokenizer does **not** accept multi-line double-quoted strings —
   an inline `python "..."` block that spans lines fails with
   `Could not tokenize here: python "`. Keeping the Python in a
   `.py` file dodges that.

   At the `(STM32F4_Discovery)` prompt (machine paused):

   ```text
   pause
   include @blink.resc
   ```

   You'll see `True / False / True / False …` scroll past — that
   is the LED blinking at 2.5 Hz of *simulated* time. (Wall-clock
   speed depends on how fast Renode is running; the **virtual**
   period is exactly 400 ms per blink.)

   Prefer to skip the files entirely? Just paste the six
   `WriteDoubleWord` / `RunFor` / `State` lines from `blink.py`
   at the monitor prompt manually — no Python, no loop, no
   escaping.

   ### c) Trace every state change to the console

   Have Renode log a line every time `UserLED` changes state, no
   matter who flipped it (your `WriteDoubleWord`, the firmware,
   or anyone else):

   ```text
   logLevel -1 sysbus.gpioPortD.UserLED
   ```

   Now any `WriteDoubleWord` to BSRR produces a `[NOISY]
   gpioPortD.UserLED: state = True/False` log line. Reset with
   `logLevel 3 sysbus.gpioPortD.UserLED`.

   When you're done, resume the firmware with `start` so Contiki
   keeps running.

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

   The console fills with Contiki function names — `process_run`,
   `etimer_process`, `clock_time`, networking-stack helpers —
   you're watching the Contiki scheduler in real time. Very
   chatty; turn off with `sysbus.cpu LogFunctionNames false`.

4. **Reset and replay.** From the monitor:

   ```text
   machine Reset
   start
   ```

   The firmware re-runs from the ELF entry point; the Contiki
   boot banner reappears in your `uart4` log / telnet session.

## 8. Exit cleanly

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
