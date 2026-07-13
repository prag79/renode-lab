# Renode Lab

Run Renode in your browser, no install required.

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/prag79/renode-lab?quickstart=1)

The first launch pulls a prebuilt image from GHCR (~30–60 s). Re-opening the same Codespace afterwards is instant.

> **Students:** start with [`HANDOUT.md`](HANDOUT.md) — a one-document walkthrough of how to launch the Codespace from a personal GitHub account, what Linux/SoC background you should already have, and a one-page cheat sheet of the most useful Renode monitor commands.

## What you get

- Renode pre-installed at `/usr/local/bin/renode`.
- RISC-V (`riscv64-unknown-elf`) and ARM Cortex-M (`arm-none-eabi`) bare-metal toolchains, plus `riscv64-linux-gnu` for Linux user-mode binaries.
- A virtual desktop on port **6080** (auto-opened in a new tab) for Renode's GUI analyzer panels.
- Twelve exercises baked into the image under `/labs/` (read-only) — eight core plus four optional capstones (multi-node IoT, edge AI from scratch, real TensorFlow Lite Micro, and cloud IoT). On first run, `lab NN` mirrors the canonical lab into your editable scratch tree at `~/work/<lab-name>/` and runs from there. Edits survive Codespace stop/start.

## Quick start (in the Codespace terminal)

```bash
lab list                # see available exercises
lab 00                  # bare-metal ARM Cortex-A9 + SmartTimer MMIO demo (also: lab demo)
lab 01                  # bundled STM32F4 demo (sanity check)
lab 02                  # boot Linux on SiFive HiFive Unleashed (RISC-V)
lab 03                  # custom RV64 SoC + bare-metal hello world
lab 04                  # bare-metal on a SiFive FE310 (HiFive1): UART + GPIO blink
lab 05                  # FE310 timer interrupts: blink from a CLINT ISR
lab 06                  # headless regression testing with the Robot framework
lab 07                  # model your own peripheral (custom C# timer IP)
lab 08                  # (optional) multi-node IoT network: 3x FE310 over a shared UART bus
lab 09                  # (optional) edge AI: run an int8 neural net on a bare-metal RISC-V core
lab 10                  # (optional) real TensorFlow Lite Micro gesture recognition on LiteX/VexRiscv
lab 10 cfu              # (optional) same, with a Verilated CFU hardware accelerator (x86-64 + internet)
lab 11                  # (optional) cloud IoT: stream JSON telemetry to AWS IoT Core / Azure IoT Hub
lab 11 fleet            # (optional) multi-node sensors -> gateway -> cloud (labs 08 + 11 combined)
lab monitor             # plain Renode interactive monitor
```

### Where your editable copy of each lab lives

The `lab NN` dispatcher follows a copy-on-first-use pattern:

| Path | What it is | Mutable? |
|---|---|---|
| `/labs/<lab-name>/` | Canonical content baked into the image | no — wiped on rebuild |
| `~/work/<lab-name>/` | Mirror created on first `lab NN` invocation | **yes — edit here** |
| `/workspaces/renode-lab/` | The cloned git repo | yes; **`git push` to your fork** to keep changes past Codespace deletion |

So the day-to-day workflow is:

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

### Persisting your work past Codespace deletion

Edits under `~/work/...` survive Codespace **stop/start** but are lost when the Codespace is **deleted** (along with everything else in the container). To keep them, you must copy them back into the cloned repo at `/workspaces/renode-lab/...` and `git push` them — but **not to `prag79/renode-lab`** (you don't have write access). Push to **your own fork** instead. **A pull request is *not* needed just to keep your work** — it's only needed if you want to contribute changes back to upstream.

Run these commands once, on any new Codespace, the first time you want to save something:

```bash
# (Run inside the Codespace, in the cloned repo.)
cd /workspaces/renode-lab

# 1. One-time: fork prag79/renode-lab on GitHub and rewire the local clone
#    so 'origin' points at YOUR fork and 'upstream' points at prag79/renode-lab.
gh repo fork --remote --remote-name=origin
git remote -v       # sanity check: origin = <you>/renode-lab, upstream = prag79/renode-lab

# 2. (Recommended) work on a branch, not main:
git checkout -b my-experiments

# 3. Copy whatever you edited under ~/work/... back into the tracked tree
#    (inside the cloned repo — NOT the read-only /labs/ image path):
cp -ru ~/work/03-custom-soc/. /workspaces/renode-lab/labs/03-custom-soc/
cp -ru ~/work/00-Demo/.       /workspaces/renode-lab/labs/00-Demo/
# ...repeat for any other lab you tweaked.

# 4. Stage, commit, push to YOUR fork:
git add -A
git status                                             # review what changed
git commit -m "lab 03: my UART experiments"
git push -u origin my-experiments                      # pushes to <you>/renode-lab
```

Verify the branch landed by visiting `https://github.com/<your-username>/renode-lab/tree/my-experiments` in a browser. Once you see your files there, the Codespace itself is now disposable — `gh codespace delete -c <name>` is safe.

To grab those changes back in a fresh Codespace: open a new Codespace **from your fork** (click "Code → Codespaces → Create on my-experiments" on `<you>/renode-lab`), and your edits are already there.

**Optional — contribute upstream.** If you want your improvements merged into `prag79/renode-lab` (e.g. you fixed a typo in a lab README), open a PR after step 4:

```bash
gh pr create --repo prag79/renode-lab \
    --base main --head "$(gh api user -q .login):my-experiments" \
    --title "lab 03: my UART experiments" \
    --body  "Short description of what changed and why."
```

## Exercises

| Lab | What it teaches | Source |
|---|---|---|
| `lab 00` (or `lab demo`) | Smallest possible MMIO example — bare-metal ARM Cortex-A9 reads/writes a stub "SmartTimer" memory window | [`labs/00-Demo/`](labs/00-Demo/) |
| `lab 01` | Renode binary works end-to-end | [`labs/01-bundled-stm32f4/`](labs/01-bundled-stm32f4/) |
| `lab 02` | Cycle-accurate Linux boot on a real SoC model | [`labs/02-linux-on-hifive/`](labs/02-linux-on-hifive/) |
| `lab 03` | Build a custom SoC from a 9-line `.repl`, write bare-metal C, run it | [`labs/03-custom-soc/`](labs/03-custom-soc/) |
| `lab 04` | Bare-metal on a **real** SiFive FE310 (HiFive1): RV32, SiFive UART + GPIO | [`labs/04-sifive-fe310/`](labs/04-sifive-fe310/) |
| `lab 05` | RISC-V interrupts: drive the LED from a CLINT machine-timer ISR | [`labs/05-fe310-interrupts/`](labs/05-fe310-interrupts/) |
| `lab 06` | Headless CI: assert firmware behaviour with a Renode Robot suite | [`labs/06-robot-testing/`](labs/06-robot-testing/) |
| `lab 07` | Model your own peripheral in C#: a memory-mapped timer IP that interrupts the CPU | [`labs/07-custom-peripheral/`](labs/07-custom-peripheral/) |
| `lab 08` | *(optional)* Multi-node IoT network: three FE310 machines on a shared UART hub, sensors reporting to a gateway | [`labs/08-multi-node-iot/`](labs/08-multi-node-iot/) |
| `lab 09` | *(optional)* Edge AI / TinyML: an int8-quantized neural net classifies handwritten digits with integer-only inference on a bare-metal RV64 core | [`labs/09-edge-ai/`](labs/09-edge-ai/) |
| `lab 10` | *(optional)* The real deal: unmodified **TensorFlow Lite Micro** (+ Zephyr) doing gesture recognition on a LiteX/VexRiscv SoC, plus an optional Verilated **CFU** hardware accelerator | [`labs/10-tflite-micro/`](labs/10-tflite-micro/) |
| `lab 11` | *(optional)* Cloud IoT: a simulated RISC-V node streams JSON telemetry over a socket to a host **gateway bridge** that forwards it to **AWS IoT Core / Azure IoT Hub** + a live dashboard; `lab 11 fleet` adds lab 08's multi-node bus (2 sensors → gateway → cloud) | [`labs/11-cloud-iot/`](labs/11-cloud-iot/) |

Labs are ordered by difficulty: **00** is a 5-minute taste of MMIO on
ARM, 01–02 run bundled images, 03 builds a minimal custom SoC, 04–05
move to a real SiFive chip with real peripherals and interrupts, 06
turns it all into an automated regression test, 07 has you write a
brand-new peripheral model that the CPU talks to, **08** *(optional)*
runs three machines on a shared UART bus as a multi-node IoT capstone,
**09** *(optional)* runs a quantized neural network on a bare-metal
core — edge AI from first principles — **10** *(optional)* runs the
real production stack: unmodified TensorFlow Lite Micro on RISC-V, with
an optional hardware ML accelerator, and **11** *(optional)* takes a
node's telemetry off-chip: a host gateway bridge forwards JSON to AWS
IoT Core / Azure IoT Hub, closing the edge → gateway → cloud loop.

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

### Lab 00 — 5-minute MMIO taste on bare-metal ARM

Full walkthrough: [`labs/00-Demo/README.md`](labs/00-Demo/README.md).

```bash
lab 00            # cross-compiles ARM bare-metal code, boots a Cortex-A9
```

The smallest possible "is your toolchain wired up?" lab: an ARM
Cortex-A9 program writes four 32-bit values into a memory-mapped
"SmartTimer" register window at `0x70000000` and reads them back. The
SmartTimer is a `Memory.MappedMemory` stub here — see lab 07 for the
behavioural-model upgrade.

After `start` in the monitor:

```text
pause
sysbus ReadDoubleWord 0x70000000      # CONTROL  -> 0x3 (ENABLE | IRQ_ENABLE)
sysbus ReadDoubleWord 0x70000004      # PERIOD   -> 0xF4240
sysbus ReadDoubleWord 0x70000008      # DUTY     -> 0x7A120
sysbus ReadDoubleWord 0x7000000C      # STATUS   -> 0x1
sysbus WriteDoubleWord 0x70000004 0x1234
sysbus ReadDoubleWord  0x70000004      # -> 0x1234   (you just poked the bus)
```

Exit: `quit` at the monitor.

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

### Lab 04 — Bare-metal on a real SiFive FE310 (HiFive1)

Full walkthrough: [`labs/04-sifive-fe310/README.md`](labs/04-sifive-fe310/README.md).

```bash
lab 04            # builds RV32 firmware, runs it on the FE310 platform
```

Same build-and-run shape as lab 03, but the platform uses the **real
SiFive FE310** memory map and two genuine SiFive peripherals (the
SiFive UART — not an NS16550 — and the SiFive GPIO). The firmware
prints a banner and blinks GPIO pin 19, narrating each toggle:

```
*** Hello from a SiFive FE310 (HiFive1)! ***
LED on
LED off
...
```

Mini-experiment: watch the LED bit flip in the GPIO register from the
monitor:

```text
pause
sysbus ReadDoubleWord 0x1001200C         # bit 19 = LED state
emulation RunFor "0.05"
sysbus ReadDoubleWord 0x1001200C          # flipped
```

Exit: `quit` at the monitor.

### Lab 05 — Interrupts: blink from a CLINT timer ISR

Full walkthrough: [`labs/05-fe310-interrupts/README.md`](labs/05-fe310-interrupts/README.md).

```bash
lab 05            # CPU sleeps in wfi; the CLINT timer wakes it to blink
```

The hardest bare-metal lab: set up `mtvec`, enable `mie.MTIE` and
`mstatus.MIE`, program the CLINT `mtimecmp`, and toggle the LED from
the machine-timer interrupt handler. Each interrupt prints `tick`:

```
*** FE310 timer-interrupt blink ***
tick
tick
...
```

Mini-experiment: confirm the trap cause after a tick:

```text
pause
sysbus.cpu MCAUSE         # 0x80000007 = interrupt (top bit) + timer (7)
```

Exit: `quit` at the monitor.

### Lab 06 — Headless regression testing with Robot

Full walkthrough: [`labs/06-robot-testing/README.md`](labs/06-robot-testing/README.md).

```bash
lab 06            # builds firmware, then runs a Robot suite with renode-test
```

No GUI, no eyeballing — a Robot Framework suite boots the firmware,
waits for specific UART lines, and asserts pass/fail with an exit
code you can gate CI on:

```
Self-Test Should Pass                          | PASS |
Platform Should Expose The UART                | PASS |
2 tests, 2 passed, 0 failed
```

Mini-experiment: break a check in `src/selftest.c` (e.g. make the ALU
test compare against the wrong value), re-run `lab 06`, and watch the
suite go **red** and `renode-test` exit non-zero — exactly what fails
a real CI job.

### Lab 07 — Model your own peripheral (custom timer IP)

Full walkthrough: [`labs/07-custom-peripheral/README.md`](labs/07-custom-peripheral/README.md).

```bash
lab 07            # compiles a C# peripheral, runs firmware that drives it
```

The capstone: you write the *hardware*. `peripherals/SimpleTimer.cs`
is a memory-mapped timer IP (CONTROL/RELOAD/COUNTER/STATUS registers,
a `LimitTimer`, and a `GPIO` interrupt line). Renode compiles it on
the fly (`i @peripherals/SimpleTimer.cs`), the platform wires its IRQ
to `cpu@11`, and bare-metal firmware programs it and handles the
interrupt:

```
*** Custom peripheral lab: SimpleTimer IP ***
tick from my custom timer IP
tick from my custom timer IP
...
```

Mini-experiment: add a read-only `TICK_COUNT` register to the C# model
in ~4 lines, re-run, and read it from the monitor with
`sysbus ReadDoubleWord 0x10001010` — you've just extended a hardware
block. Your peripheral shows up in `peripherals`, logs bus accesses,
and raises real interrupts, indistinguishable from a built-in model.

### Lab 08 — Multi-node IoT network (optional)

Full walkthrough: [`labs/08-multi-node-iot/README.md`](labs/08-multi-node-iot/README.md).

```bash
lab 08            # boots 3 FE310 machines on one shared UART bus
```

Every prior lab ran a single machine; this one runs **three** in one
emulation. A shared `UARTHub` (`emulation CreateUARTHub "bus"`) is the
broadcast medium; each node's `uart1` is wired onto it with
`connector Connect sysbus.uart1 bus`. One C source builds three images
(`-DNODE_ID`): two sensor nodes broadcast fake readings, and a gateway
node collects them:

```
[gateway] report: node1 t=21 seq=0
[gateway] report: node2 t=22 seq=0
[gateway] report: node1 t=22 seq=1
...
```

Mini-experiment: `connector Disconnect sysbus.uart1 bus` on `sensor2`
and its reports stop reaching the gateway — the connector *is* the
network. The same `connector` pattern scales to Ethernet, CAN, and
true 802.15.4/BLE wireless meshes.

### Lab 09 — Edge AI: a neural net on a bare-metal core (optional)

Full walkthrough: [`labs/09-edge-ai/README.md`](labs/09-edge-ai/README.md).

```bash
lab 09            # cross-compiles an int8 MLP, classifies 8x8 digits on RV64
```

TinyML from first principles: a 2-layer, int8-quantized neural network
(trained offline on mini-MNIST, ~97% accuracy) runs as **integer-only**
arithmetic on a bare-metal RV64 core — no OS, no ML framework, no FPU.
Each 8x8 digit is drawn as ASCII art next to the model's prediction:

```
---- sample 1 of 10 ----
        ..##%%##%%##@@++
        ..++%%##%%%%++
  prediction = 4  (true = 4)  OK
...
==== accuracy: 10 / 10 correct ====
```

The whole "AI" is two integer matrix-vector multiplies in
`src/digits.c`; the weights live in an auto-generated `src/model.h`.

Mini-experiment: `pause` then `sysbus.cpu ExecutedInstructions` to read
the model's inference cost, or edit a `sample_images[...]` row in
`src/model.h` to feed it your own hand-drawn digit. Lab 10 picks up
where this leaves off with the real production stack.

### Lab 10 — Real TensorFlow Lite Micro on RISC-V (optional)

Full walkthrough: [`labs/10-tflite-micro/README.md`](labs/10-tflite-micro/README.md).

```bash
lab 10            # boots Zephyr + TFLM, classifies accelerometer gestures
lab 10 cfu        # same workload with a Verilated CFU accelerator (x86-64 + internet)
```

The industrial counterpart to lab 09: unmodified **TensorFlow Lite
Micro** (Google's MCU ML runtime) runs inside a **Zephyr** firmware on a
**LiteX/VexRiscv** RISC-V SoC, classifying real recorded gestures fed
through a modelled ADXL345 accelerometer. The prebuilt firmware is
vendored, so `lab 10` runs offline with no build:

```
Got accelerometer, label: accel-0
RING:
          *
       *     *
    ...
SLOPE:
        *
       *
    ...
```

This is real TFLM inference — compute-heavy, so the first gesture takes
~30–60 s. `make test` regression-tests the ML output headless (lab 06
style). `lab 10 cfu` runs the same class of workload with a **Custom
Function Unit** hardware accelerator co-simulated in Verilator — the
edge-AI accelerator co-design loop, no silicon required. (The CFU
variant downloads an x86-64 Verilated binary at runtime; see the lab
README for caveats.)

### Lab 11 — Cloud IoT: telemetry to AWS IoT Core / Azure IoT Hub (optional)

Full walkthrough: [`labs/11-cloud-iot/README.md`](labs/11-cloud-iot/README.md).

```bash
lab 11            # a RISC-V node streams JSON telemetry on tcp://localhost:3456
lab 11 fleet      # 2 sensors -> gateway -> the same uplink (labs 08 + 11 combined)
# then, in a SECOND terminal:
cd ~/work/11-cloud-iot && python3 tools/bridge.py            # local dashboard only
python3 tools/bridge.py --cloud aws                          # + AWS IoT Core
python3 tools/bridge.py --cloud azure                        # + Azure IoT Hub
```

Where lab 08 stopped at the gateway and lab 09/10 stayed on-chip, this
lab crosses the last boundary — **off the device to a real cloud IoT
broker**. The firmware stays a dumb, deterministic sensor that only
knows how to print a JSON line on its UART; Renode exposes that UART on
a TCP socket; and a host-side **bridge** (`tools/bridge.py`) plays the
gateway: it reads the telemetry, always serves a live Chart.js dashboard
on `http://localhost:8000`, and *optionally* forwards each message to
**AWS IoT Core** (MQTT over mutual TLS) or **Azure IoT Hub** (device
SDK). Credentials live only on the host (git-ignored `certs/` / `.env`),
never in firmware. `lab 11 fleet` bolts lab 08's shared-bus multi-node
network onto the front: two sensor nodes broadcast on a UART hub, a
gateway node relays the merged stream out the cloud uplink.

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

Stopping a Codespace stops compute billing. Storage keeps ticking until you **delete** it; push your work to your fork first (see [Persisting your work past Codespace deletion](#persisting-your-work-past-codespace-deletion)).

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `lab` does nothing, no output | `lab` script wasn't copied or isn't executable | `ls -l /usr/local/bin/lab` — must be `-rwxr-xr-x`. Rebuild the image. |
| noVNC tab opens but says "Failed to connect" | `LAB_GUI` not set or entrypoint didn't start the desktop | `echo $LAB_GUI` (must print `1`); `pgrep -af x11vnc`. |
| `lab 03` fails at `make` with "missing separator" | Makefile lost real TABs (your editor converted to spaces) | `cat -et labs/03-custom-soc/Makefile \| grep -A1 hello.elf:` — recipe lines must show `^I`. |
| Codespace can't pull `ghcr.io/prag79/renode-lab` | GHCR package is private | Visit <https://github.com/prag79?tab=packages> → `renode-lab` → Package settings → Change visibility → Public. |
| Build fails in Actions | See the failing step | `gh run view <id> --log-failed` from inside this repo. |
