# Renode Lab

Run Renode in your browser, no install required.

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/prag79/renode-lab?quickstart=1)

The first launch pulls a prebuilt image from GHCR (~30–60 s). Re-opening the same Codespace afterwards is instant.

## What you get

- Renode pre-installed at `/usr/local/bin/renode`.
- RISC-V (`riscv64-unknown-elf`) and ARM Cortex-M (`arm-none-eabi`) bare-metal toolchains, plus `riscv64-linux-gnu` for Linux user-mode binaries.
- A virtual desktop on port **6080** (auto-opened in a new tab) for Renode's GUI analyzer panels.
- Three exercises baked into the image under `/labs/` (read-only). On first run, `lab NN` mirrors the canonical lab into your editable scratch tree at `~/work/<lab-name>/` and runs from there. Edits survive Codespace stop/start.

## Quick start (in the Codespace terminal)

```bash
lab list                # see available exercises
lab 01                  # bundled STM32F4 demo (sanity check)
lab 02                  # boot Linux on SiFive HiFive Unleashed (RISC-V)
lab 03                  # custom RV64 SoC + bare-metal hello world
monitor                 # plain Renode interactive monitor
```

### Where your editable copy of each lab lives

The `lab NN` dispatcher follows a copy-on-first-use pattern:

| Path | What it is | Mutable? |
|---|---|---|
| `/labs/<lab-name>/` | Canonical content baked into the image | no — wiped on rebuild |
| `~/work/<lab-name>/` | Mirror created on first `lab NN` invocation | **yes — edit here** |
| `/workspaces/renode-lab/` | The cloned git repo | yes; `git push` to keep changes past Codespace deletion |

So the workflow is:

```bash
lab 01                  # mirrors /labs/01-bundled-stm32f4 -> ~/work/01-bundled-stm32f4 and runs it
# ...later, after Codespace restart...
cd ~/work/01-bundled-stm32f4
nano stm32f4.resc       # tweak it
lab 01                  # cp -ru is idempotent: your edits are preserved
```

Or just run from anywhere once the working copy exists:

```bash
cd ~/work/03-custom-soc && renode --console renode/mini-rv.resc
```

Edits to `~/work/...` persist across Codespace stop/start. To make changes survive Codespace **deletion**, copy them back into `/workspaces/renode-lab/labs/...` and `git push`.

## Exercises

| Lab | What it teaches | Source |
|---|---|---|
| `lab 01` | Renode binary works end-to-end | [`labs/01-bundled-stm32f4/`](labs/01-bundled-stm32f4/) |
| `lab 02` | Cycle-accurate Linux boot on a real SoC model | [`labs/02-linux-on-hifive/`](labs/02-linux-on-hifive/) |
| `lab 03` | Build a custom SoC from a 9-line `.repl`, write bare-metal C, run it | [`labs/03-custom-soc/`](labs/03-custom-soc/) |

## Step-by-step tutorials

Every lab follows the same shape — bring the board up, prove it's
running, capture UART, try a handful of monitor commands, do a
mini-experiment, exit cleanly. Below is the one-screen version of
each. The full walkthroughs (with command tables and 4–5
experiments per lab) live in each lab's own `README.md`.

Two pieces of background that apply to **all three** labs:

- The **Renode monitor** is the `(machine-N)` prompt that appears
  in your terminal after `lab NN`. Everything in code blocks
  marked "monitor" below is typed there. Type `help` for the full
  command list, or `help <command>` for per-command usage.
- The **noVNC desktop** on port 6080 (path `/vnc.html`) is where
  Renode's analyzer windows render. If the port isn't forwarded,
  use the `CreateFileBackend` / `CreateServerSocketTerminal`
  recipes shown below to read UART without a GUI.

### Lab 01 — STM32F4 bringup → first monitor commands

Full walkthrough: [`labs/01-bundled-stm32f4/README.md`](labs/01-bundled-stm32f4/README.md).

```bash
lab 01            # boots the bundled STM32F4 Discovery demo (Contiki + UART)
```

Once `(STM32F4_Discovery)` appears, in the **Renode monitor**:

```text
peripherals                              # see every modelled peripheral
sysbus.cpu PC                            # CPU is running — value changes between calls
pause
sysbus.cpu Step
sysbus.cpu PC                            # advanced by one instruction
start
sysbus.uart4 CreateFileBackend @/tmp/uart4.log true
```

Then in another Codespace terminal: `tail -f /tmp/uart4.log`
streams the Contiki boot banner and console output.

> The Contiki console is on **`uart4`**, not `usart2`. Attaching
> a backend to `usart2` will produce an empty file — that
> peripheral exists in the platform but the demo firmware
> doesn't use it.

Mini-experiment: toggle an on-board LED directly from the
monitor (no firmware change), via the GPIOD BSRR register:

```text
pause
sysbus WriteDoubleWord 0x40020C18 0x00001000   # set PD12
sysbus WriteDoubleWord 0x40020C18 0x10000000   # reset PD12
start
```

Exit: `quit` at the monitor.

### Lab 02 — Boot Linux on RISC-V → poke around inside

Full walkthrough: [`labs/02-linux-on-hifive/README.md`](labs/02-linux-on-hifive/README.md).

```bash
lab 02            # boots an unmodified RV64 Linux kernel on the FU540 model
```

First run downloads ~50 MB of OpenSBI + kernel + initramfs into
Renode's cache (~30 s); subsequent runs start instantly. Watch
the boot log; when the BusyBox login appears in the `uart0`
analyzer, log in as `root` (no password). Inside Linux:

```sh
uname -a
cat /proc/cpuinfo                # 5 RISC-V harts (4×U54 + 1×E51)
cat /proc/device-tree/model      # "SiFive HiFive Unleashed A00"
dmesg | head -50
```

Switch to the **Renode monitor** (the prompt above the kernel
output) and try:

```text
peripherals                              # FU540 platform tree
sysbus.u54_1 PC                          # PC of application hart 1
pause
emulation RunFor "0.5"                   # advance exactly 0.5 s of sim-time
sysbus.u54_1 PC                          # PC after exactly 0.5 s — deterministic
start
sysbus.uart0 CreateFileBackend @/tmp/hifive-uart.log true
```

Mini-experiment: bus-level UART tracing while you type:

```text
logLevel 0 sysbus.uart0
```

Type one character into the BusyBox shell — the monitor logs
every register read/write the kernel's UART driver issues.
Reset with `logLevel 3 sysbus.uart0`.

Exit: `poweroff -f` inside Linux (optional), then `quit` at the
Renode monitor.

### Lab 03 — Build a custom SoC + run your own bare-metal C

Full walkthrough: [`labs/03-custom-soc/README.md`](labs/03-custom-soc/README.md).

```bash
lab 03            # cross-compiles src/, then boots the 9-line mini-rv platform
```

You should see in the `uart` analyzer (or in `/tmp/mini-rv.log`
if you set up a file backend):

```
*** Hello from custom RV64 SoC! ***
UART @ 0x10000000, RAM @ 0x80000000
```

In the **Renode monitor**:

```text
peripherals                              # only cpu, ram, uart — that's the whole SoC
sysbus.cpu PC                            # ELF entry: 0x80000000
pause
machine Reset
sysbus.cpu PC                            # back at _start
sysbus.cpu Step 20
sysbus.cpu PC                            # somewhere inside main()
sysbus WriteByte 0x10000000 0x41         # write 'A' to the UART, no CPU involved
start
```

Mini-experiment: change the UART message in `src/hello.c`, save,
re-run `lab 03`. The Makefile rebuilds only the changed file and
Renode picks up the new ELF — full edit-build-run loop in under a
second.

Bonus: append one line to `renode/mini-rv.repl` to add a second
UART:

```
uart2: UART.NS16550 @ sysbus 0x10001000
```

Re-run `lab 03`; `peripherals` now lists two UARTs. That is how
cheap it is to extend a Renode SoC.

Exit: `quit` at the monitor.

## How this works

```
You (this repo)  →  GitHub Actions  →  GHCR  →  Codespaces VM  →  Your browser
```

- `git push` to `main` triggers `.github/workflows/build.yml`, which builds the Docker image (~3 min) and pushes it to `ghcr.io/prag79/renode-lab:latest`.
- A student clicks the badge above; Codespaces boots a VM, pulls the image, attaches VS Code in the browser.
- `LAB_GUI=1` triggers `entrypoint.sh` to start `Xvfb` + `x11vnc` + `noVNC` on port 6080 so the Renode GUI analyzers are visible.

See `renode-codespaces-lab-guide.md` (in the parent `qemu_workspace/`) for the full design rationale.

## Cost (personal GitHub account)

| Meter | Free quota / month | Realistic usage |
|---|---|---|
| Compute | 120 core-hours | 30 wall-clock h on this 2-core machine |
| Storage | 15 GB-month | One persistent Codespace = ~16 GB-month → ~$0.07/mo overage, or $0 if deleted between sessions |

Stopping a Codespace stops compute billing. Storage keeps ticking until you **delete** it; `git push` your work first.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `lab` does nothing, no output | `lab` script wasn't copied or isn't executable | `ls -l /usr/local/bin/lab` — must be `-rwxr-xr-x`. Rebuild the image. |
| noVNC tab opens but says "Failed to connect" | `LAB_GUI` not set or entrypoint didn't start the desktop | `echo $LAB_GUI` (must print `1`); `pgrep -af x11vnc`. |
| `lab 03` fails at `make` with "missing separator" | Makefile lost real TABs (your editor converted to spaces) | `cat -et labs/03-custom-soc/Makefile \| grep -A1 hello.elf:` — recipe lines must show `^I`. |
| Codespace can't pull `ghcr.io/prag79/renode-lab` | GHCR package is private | Visit <https://github.com/prag79?tab=packages> → `renode-lab` → Package settings → Change visibility → Public. |
| Build fails in Actions | See the failing step | `gh run view <id> --log-failed` from inside this repo. |
