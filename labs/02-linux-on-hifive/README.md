# Lab 02 — Boot Linux on a SiFive HiFive Unleashed (RISC-V)

Full-system simulation: a real RV64 Linux kernel boots on the
modelled FU540 SoC. On first run Renode auto-fetches the kernel,
device tree blob, and initramfs (~50 MB total) into its cache.

## Run it

```bash
lab 02
```

In the Renode monitor that appears, watch the boot messages from
OpenSBI → kernel → initramfs over the next ~60 s. When the
BusyBox shell appears in the UART analyzer panel (noVNC tab on
port 6080), log in as `root` (no password). Try:

```sh
uname -a
cat /proc/cpuinfo
```

## Why this is interesting

You're running an actual RISC-V Linux kernel cycle-accurately on
a software-modelled CPU. Nothing on your laptop is RISC-V.
Renode's MMU, interrupt controller, and UART models are doing the
heavy lifting; the kernel is unmodified upstream.

## Stopping cleanly

Type `quit` at the Renode monitor (the prompt above the kernel
output, not inside Linux) to exit and save the simulation log.
