# Student Handout — Renode Lab on GitHub Codespaces

Welcome. Over the next few hours you will simulate three different
embedded systems — a Cortex-M4 microcontroller, a multi-core
RISC-V Linux SoC, and a custom 9-line RV64 SoC of your own — all
inside a browser tab, with **nothing installed on your laptop**.

This document walks you through the setup once. After that,
everything you need is in the per-lab `README.md` files in the
repository.

## 1. What you need before starting

### 1.1 A personal GitHub account

If you don't already have one, create a **free personal account**
at <https://github.com/join>. School-issued accounts work too,
but a personal account is preferred so the Codespace and any
edits you make follow you after the course ends.

GitHub's free tier includes:

| Resource | Free quota / month |
|---|---|
| Codespaces compute | 120 core-hours (≈ 60 hours on a 2-core machine) |
| Codespaces storage | 15 GB-month |

The whole renode-lab fits comfortably in those limits if you
**stop your Codespace** when you take a break and **delete it**
when you're done for the day. Leaving a Codespace running idle
burns compute hours; leaving it created-but-stopped burns
storage. We'll cover both at the end.

### 1.2 A modern web browser

Chrome, Firefox, Safari, or Edge — anything from the last two
years. You will spend the entire lab in two browser tabs:

1. The **VS Code** tab that GitHub auto-opens (your terminal +
   editor live here).
2. The **noVNC desktop** tab on port 6080 (Renode's GUI
   analyzers — UART windows, GPIO/LED indicators, etc.).

### 1.3 Working knowledge of basic Linux

You should be comfortable doing all of these from a terminal
without looking them up:

- Navigating: `pwd`, `cd`, `ls`, `ls -la`
- Reading files: `cat`, `less`, `head`, `tail`, `tail -f`
- Editing files: `nano`, `vim`, or just VS Code's editor pane
- Pipes and redirection: `|`, `>`, `>>`, `2>&1`
- Background jobs: `&`, `Ctrl-C`, `Ctrl-Z`, `jobs`, `fg`
- Inspecting processes: `ps`, `pgrep -af <name>`
- Reading exit codes and recognising errors

You do **not** need to know Docker, Kubernetes, or anything
about Codespaces internals.

### 1.4 Familiarity with basic SoC concepts

You should already understand, at a hand-wave level:

- A CPU **fetches** instructions from memory addresses, then
  **executes** them. The address it's about to fetch from is
  the **program counter (PC)**.
- A **bus** lets the CPU talk to memory regions and **memory-
  mapped peripherals** (UART, GPIO, timers, interrupt
  controller, etc.). Each peripheral lives at a known base
  address.
- A **UART** sends bytes one wire at a time; on most cores
  there's a "transmit holding register" and a "line status
  register" with a `TX-FIFO-empty` bit.
- A **GPIO port** has registers for direction (input/output),
  output data (`ODR`), and atomic set/reset (`BSRR` on STM32).
- An **ELF** is a packaged binary with a known **entry point**;
  on Cortex-M, after reset the CPU loads SP from address `0x0`
  and PC from address `0x4` (the vector table).
- A **bootloader** (BBL, OpenSBI, U-Boot) initialises the SoC
  enough to hand control to a kernel.
- A **kernel** schedules processes; a **userspace shell** like
  BusyBox is the program you type commands into.

Lab 01 will exercise the first four bullets, lab 02 the last
three, and lab 03 lets you build the bus + RAM + UART picture
from nine lines of declarative platform description.

## 2. First launch (only do this once)

### 2.1 Open the lab repository

Visit <https://github.com/prag79/renode-lab>. Sign in to your
GitHub account if prompted.

### 2.2 Click the "Open in GitHub Codespaces" badge

Near the top of the page you'll see this badge:

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/prag79/renode-lab?quickstart=1)

Click it. GitHub will:

1. Show a **"Create codespace"** screen — accept the defaults
   (2-core machine, 8 GB RAM, 32 GB storage). Click **Create
   codespace**.
2. Spin up a small Linux VM somewhere in their cloud (≈30 s).
3. Pull the prebuilt image `ghcr.io/prag79/renode-lab:latest`
   from the GitHub Container Registry (≈30–60 s, only on first
   launch — afterwards it's cached).
4. Open VS Code in your browser, attached to the running
   container.

A second tab will auto-open at `*.app.github.dev:6080/vnc.html` —
that's the noVNC desktop. If it shows "Failed to connect", give
it 10 seconds and click **Connect** in the noVNC page.

### 2.3 Verify the environment

In the VS Code terminal (open one with **Ctrl-`** if it isn't
already), type:

```bash
lab list
```

You should see the three available labs:

```
Available labs:
  01       - bundled STM32F4 demo (sanity check)
  02       - Linux on SiFive HiFive Unleashed (RISC-V)
  03       - custom RV64 SoC + bare-metal hello world
  monitor  - plain Renode interactive monitor
```

Then run lab 01:

```bash
lab 01
```

You should land at a `(STM32F4_Discovery)` prompt within a
couple of seconds. That's the **Renode monitor** — Renode's
interactive command line. From here, type:

```text
start
sysbus.cpu PC
sysbus.cpu PC
```

If the second `sysbus.cpu PC` prints a different value than the
first, your environment is healthy. Type `quit` to exit.

If anything above fails, see **§ 5 Troubleshooting**.

## 3. The three exercises

Each lab has its own detailed `README.md` with a 7-section
walkthrough — bring up, what just happened, first commands,
headless UART recipes, useful monitor command tables, mini-
experiments, and clean exit.

| Lab | Time | What you'll do | Detailed README |
|---|---|---|---|
| **01** | ~20 min | Boot a bundled Contiki firmware on a simulated STM32F4 Discovery board. Read UART, single-step the CPU, blink the on-board LED from the simulator. | [`labs/01-bundled-stm32f4/README.md`](labs/01-bundled-stm32f4/README.md) |
| **02** | ~30 min | Boot an unmodified RISC-V Linux kernel (5 cores, OpenSBI, BusyBox userspace) on the SiFive HiFive Unleashed model. Poke around `/proc`, trace UART traffic at the bus level. | [`labs/02-linux-on-hifive/README.md`](labs/02-linux-on-hifive/README.md) |
| **03** | ~45 min | Cross-compile bare-metal C for RV64. Run it on a 9-line custom SoC you can edit. Add a second peripheral with one line. | [`labs/03-custom-soc/README.md`](labs/03-custom-soc/README.md) |
| **04** | ~45 min | Bare-metal on a **real** SiFive FE310 (HiFive1). Drive the SiFive UART and GPIO at their datasheet addresses; blink an LED on RV32. | [`labs/04-sifive-fe310/README.md`](labs/04-sifive-fe310/README.md) |
| **05** | ~60 min | RISC-V machine-mode interrupts: program `mtvec`/`mie`/`mstatus` and the CLINT timer, then blink the LED from an interrupt handler. | [`labs/05-fe310-interrupts/README.md`](labs/05-fe310-interrupts/README.md) |
| **06** | ~45 min | Headless CI: write a Robot Framework suite that boots firmware, asserts UART output, and fails the build (non-zero exit) on regressions. | [`labs/06-robot-testing/README.md`](labs/06-robot-testing/README.md) |
| **07** | ~75 min | Model your own peripheral: write a memory-mapped timer IP in C#, compiled by Renode at runtime, that raises an interrupt the firmware handles. | [`labs/07-custom-peripheral/README.md`](labs/07-custom-peripheral/README.md) |

Do them **in order** — they increase in difficulty. Each one
introduces a concept the next assumes (the three Renode primitives
`mach create`, `LoadPlatformDescription`, `LoadELF` + `start`; then
real peripherals, interrupts, and automated testing).

## 4. Where your edits live

The `lab NN` command does not run the lab from `/labs/...`
directly. On first invocation it copies the lab into a
**writable scratch tree** under your home directory and runs it
from there:

| Path | What it is | Survives Codespace stop? | Survives container rebuild? | Survives Codespace delete? |
|---|---|:---:|:---:|:---:|
| `/labs/<lab-name>/` | Read-only image content | yes | no | no |
| `~/work/<lab-name>/` | Editable copy made by `lab NN` | **yes** | no | no |
| `/workspaces/renode-lab/` | The cloned git repo | yes | yes | only if `git push`-ed |

**Practical rules:**

- If you just want to tweak a `.resc` or a `hello.c` and re-run,
  edit `~/work/<lab-name>/...`. The next `lab NN` reuses your
  edits (`cp -ru` is idempotent — it never overwrites existing
  files).
- If you want changes to **outlive Codespace deletion**, copy
  them into `/workspaces/renode-lab/labs/...` and `git push`.
  That requires you to fork the repo first (GitHub will offer
  this when you try to push).

## 5. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `lab 01` prompt appears but `sysbus.cpu PC` keeps returning `0x00000000` | You haven't typed `start` yet — the bundled `.resc` loads the ELF but doesn't release the CPU from reset | Type `start` once. |
| The terminal is unreadable: UART log lines stream non-stop and your typing doesn't echo | UART output is being mirrored to the console at INFO level | `pause` (type blindly, then **Enter**), then `logLevel 3 sysbus.uart4`, then `start`. |
| noVNC tab shows "Failed to connect" or HTTP 502 | Port 6080's WebSocket handshake hadn't completed when the tab opened | Wait 10 s and click **Connect** in the noVNC page. If still broken: in VS Code's **Ports** panel, right-click port 6080 → **Port Visibility → Public** (or sign in to the popup). |
| Browser tab opens but shows the noVNC welcome screen, not a desktop | URL is missing the path | Append `/vnc.html` to the URL, then **Connect**. |
| `pgrep -af 'Xvfb\|fluxbox\|x11vnc\|websockify'` shows fewer than four processes | The headless desktop didn't fully start | Run `/usr/local/bin/entrypoint.sh true`. Check the per-service logs in `/tmp/Xvfb_:1.log`, `/tmp/fluxbox.log`, `/tmp/x11vnc_-display_:1.log`, `/tmp/websockify.log`. |
| `lab` does nothing or says command not found | You're in a shell where `/usr/local/bin` isn't on `$PATH` (rare) | Run it with the full path: `/usr/local/bin/lab list`. |
| Codespace fails to start with "Failed to pull image" | GHCR rate-limited you or the image package is private | Visit <https://github.com/prag79?tab=packages> → `renode-lab` → check it's **Public**. Retry. |
| You changed something under `/labs/...`, ran `lab NN`, and your change had no effect | Edits to `/labs/...` are overlaid on a read-only image and discarded on container rebuild — the dispatcher uses `~/work/<lab-name>/...` | Edit the file under `~/work/<lab-name>/...` instead, then `lab NN` again. |

## 6. Cost discipline (so you don't burn quota)

GitHub will not charge you anything as long as you stay inside
the free tier. Two habits keep you there:

1. **Stop the Codespace when you walk away.** In the bottom-left
   of VS Code (or the **Codespaces** menu in your GitHub
   profile) click **Stop codespace**. Compute billing pauses
   immediately. Storage keeps ticking at ~$0.07 / GB / month —
   tiny, but cumulative.
2. **Delete the Codespace when you're done for the day** — but
   only after committing or copying out anything you want to
   keep. From your laptop terminal:

   ```bash
   gh codespace list                                    # find the name
   gh codespace delete --codespace <name> --force
   ```

   …or use the web UI at <https://github.com/codespaces>.

You can always recreate a Codespace from the badge in the
README. The first launch is the only slow one (~60 s); after
that it's instant.

## 7. Where to go after the labs

- The Renode user docs: <https://renode.readthedocs.io/>
- Renode's bundled platform `.repl` files for ~100 other boards:
  `/opt/renode/platforms/` inside your Codespace
- The Antmicro YouTube channel for SoC-modelling walkthroughs.

If you find a bug or have an improvement, open a PR against
<https://github.com/prag79/renode-lab>. The full toolchain (lab
content → Docker image → GHCR → Codespaces) is in this one
repository and welcomes contributions.

---

**Quick reference card** (print this if you want a single-page
cheat sheet):

```
Open lab:           click the badge in the README
Verify install:     lab list && lab 01 -> start -> sysbus.cpu PC -> quit
Edit + re-run:      edit ~/work/<lab>/..., then lab NN
Read UART:          (in monitor) sysbus.uartN CreateFileBackend @/tmp/uartN.log true
                    (in another terminal) tail -f /tmp/uartN.log
Quiet log spam:     logLevel 3 sysbus.uartN
Pause/resume CPU:   pause / start
Read PC:            sysbus.cpu PC
Step one insn:      sysbus.cpu Step
Reset firmware:     machine Reset
Exit Renode:        quit
Stop Codespace:     bottom-left of VS Code -> Stop codespace
Delete Codespace:   gh codespace delete -c <name> --force
```
