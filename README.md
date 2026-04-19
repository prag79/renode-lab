# Renode Lab

Run Renode in your browser, no install required.

[![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/prag79/renode-lab?quickstart=1)

The first launch pulls a prebuilt image from GHCR (~30–60 s). Re-opening the same Codespace afterwards is instant.

## What you get

- Renode pre-installed at `/usr/local/bin/renode`.
- RISC-V (`riscv64-unknown-elf`) and ARM Cortex-M (`arm-none-eabi`) bare-metal toolchains, plus `riscv64-linux-gnu` for Linux user-mode binaries.
- A virtual desktop on port **6080** (auto-opened in a new tab) for Renode's GUI analyzer panels.
- Three exercises preloaded under `/labs/` and visible in the file tree under `labs/`.

## Quick start (in the Codespace terminal)

```bash
lab list                # see available exercises
lab 01                  # bundled STM32F4 demo (sanity check)
lab 02                  # boot Linux on SiFive HiFive Unleashed (RISC-V)
lab 03                  # custom RV64 SoC + bare-metal hello world
monitor                 # plain Renode interactive monitor
```

## Exercises

| Lab | What it teaches | Source |
|---|---|---|
| `lab 01` | Renode binary works end-to-end | [`labs/01-bundled-stm32f4/`](labs/01-bundled-stm32f4/) |
| `lab 02` | Cycle-accurate Linux boot on a real SoC model | [`labs/02-linux-on-hifive/`](labs/02-linux-on-hifive/) |
| `lab 03` | Build a custom SoC from a 9-line `.repl`, write bare-metal C, run it | [`labs/03-custom-soc/`](labs/03-custom-soc/) |

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
