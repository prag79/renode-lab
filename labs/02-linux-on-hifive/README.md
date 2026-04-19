# Lab 02 — Boot Linux on a SiFive HiFive Unleashed (RISC-V)

Full-system simulation: an unmodified RV64 Linux kernel boots on
the modelled FU540 SoC. On first run Renode auto-fetches OpenSBI,
the kernel image, the device tree blob, and the initramfs (~50 MB
total) into its on-disk cache; subsequent runs start in seconds.

## 1. Bring the board up

```bash
lab 02
```

This runs (after mirroring `/labs/02-linux-on-hifive/` into your
editable scratch tree at `~/work/02-linux-on-hifive/` on first
invocation):

```
cd ~/work/02-linux-on-hifive
renode --plain --disable-gui --console linux-hifive.resc
```

`linux-hifive.resc` simply `include`s the bundled
`scripts/complex/hifive_unleashed/hifive_unleashed.resc` shipped
inside `/opt/renode/`. Edit `linux-hifive.resc` (e.g. swap in a
different upstream `.resc`, add `mach create` overrides, change
the kernel command line) and re-run `lab 02` — your edits are
preserved. After ~30 s of cache fetching (first run
only), boot proceeds in this order:

1. `OpenSBI v0.9` banner.
2. `Linux version 5.x …` printk stream.
3. initramfs unpacks, `udev` settles, `/sbin/init` runs.
4. A BusyBox login prompt appears in the **`uart0` analyzer**
   window (noVNC tab on port 6080, path `/vnc.html`).

Log in as `root` (no password). You're now inside a real
RISC-V Linux userspace.

If the noVNC tab is not available, skip ahead to section 4 to
read the UART from the monitor instead.

## 2. What just happened

The bundled `.resc` is doing essentially the same three steps as
lab 01, just with a much bigger platform:

1. `mach create "HiFive-Unleashed"` — virtual machine.
2. `machine LoadPlatformDescription @platforms/cpus/sifive-fu540.repl`
   — 4×U54 + 1×E51 cores, CLINT, PLIC, UART, DDR controller, …
3. `sysbus LoadELF` (OpenSBI), `sysbus LoadFdt` (device tree),
   `sysbus LoadBinary` (kernel + initramfs), then `start`.

The kernel is **upstream Linux**, unmodified. Renode's CPU, MMU,
PLIC, and UART models are doing the heavy lifting.

## 3. First demo: poke around inside Linux

In the BusyBox shell (the analyzer window, **not** the Renode
monitor):

```sh
uname -a                       # Linux ... riscv64 GNU/Linux
cat /proc/cpuinfo              # 5 RISC-V harts (4×U54 + 1×E51)
cat /proc/device-tree/model    # "SiFive HiFive Unleashed A00"
ls /sys/class/                 # devices the kernel discovered
dmesg | head -50               # boot log from kernel POV
free -m                        # ~2 GB DDR (modelled)
```

Try a small workload to feel the simulated time:

```sh
time dd if=/dev/zero of=/dev/null bs=1M count=64
```

The wall-clock time is whatever your laptop runs at; the
**simulated** time `time` reports is exactly what a real FU540
would have taken. That's the point of cycle-accurate simulation.

## 4. Read UART from the monitor (no GUI needed)

Switch focus to the **Renode monitor** (the `(hifive-unleashed)` prompt
in your terminal — *above* the BusyBox area). All commands in
this section are monitor commands.

```text
peripherals
```

Lists everything in the FU540 platform tree. Compare to lab 01's
much shorter output.

```text
sysbus.uart0 CreateFileBackend @/tmp/hifive-uart.log true
```

Now in **another Codespace terminal**:

```bash
tail -f /tmp/hifive-uart.log
```

Every byte the kernel writes to the console also lands in that
file — useful for grepping `dmesg` later.

To get a second interactive console without the GUI:

```text
emulation CreateServerSocketTerminal 3457 "linux-tty"
connector Connect sysbus.uart0 linux-tty
```

```bash
telnet localhost 3457    # in another Codespace terminal
```

> Note: the bundled `.resc` already attached an analyzer to
> `uart0`. Connecting a second terminal to the same UART
> duplicates output; that's fine for read-only inspection.

## 5. Useful monitor commands to try

Same shape as lab 01 — type `help` for the full list. The
interesting ones for a Linux boot are:

| Command | What it does |
|---|---|
| `peripherals` | Tree of every modelled peripheral (PLIC, CLINT, …). |
| `machine` | Machine name, status, loaded images. |
| `sysbus.e51 PC` | Read the PC of the management hart (E51). |
| `sysbus.u54_1 PC` | Read the PC of application hart 1 (one of four U54s). |
| `pause` / `start` | Freeze and resume *all* harts simultaneously. |
| `sysbus.u54_1 Step` | Step a single hart by one instruction. |
| `sysbus.u54_1 LogFunctionNames true` | Stream every kernel function entry on hart 1. Very chatty — use with `pause` first, then `start`, then `pause` again after a few seconds. |
| `sysbus ReadDoubleWord 0x10000000` | Peek at the UART base register. |
| `logLevel 0 sysbus.uart0` | Verbose log of every UART register access. Reset with `logLevel 3 sysbus.uart0`. |
| `emulation RunFor "1.0"` | Advance exactly 1.0 s of simulated time, then auto-pause. Deterministic. |
| `machine StatisticalProfiler` | Sampling profiler; dump samples with its `WriteToFile` method. |
| `showAnalyzer sysbus.uart0` | Re-open the UART analyzer window in noVNC. |
| `quit` | Exit Renode. |

## 6. Mini-experiments (try at least one)

1. **Pause the kernel in flight.** Once you have a shell,
   jump to the Renode monitor and type `pause`. Run
   `sysbus.u54_1 PC`, then look up which kernel function that
   PC lives in (`riscv64-linux-gnu-addr2line -e <vmlinux> <PC>`
   if you have a vmlinux; the bundled image does not ship
   symbols, but the address still proves the kernel was caught
   mid-instruction). `start` to resume.

2. **Deterministic timing.** Pause, then:

   ```text
   emulation RunFor "0.5"
   sysbus.u54_1 PC
   ```

   Repeat. The sequence of PCs is byte-identical between Renode
   sessions — useful for reproducing race-condition bugs.

3. **Trace UART traffic at the bus level.** Pause, then:

   ```text
   logLevel 0 sysbus.uart0
   start
   ```

   Type a single character into the BusyBox shell and watch the
   monitor log every register read/write the driver does. Very
   illustrative if you've never written a UART driver. Reset
   with `logLevel 3 sysbus.uart0`.

4. **Use Linux to talk to the simulated hardware.** From the
   BusyBox shell:

   ```sh
   cat /proc/interrupts            # PLIC routing table
   echo hello > /dev/ttySIF0       # writes back through UART
   ```

## 7. Stopping cleanly

In the BusyBox shell:

```sh
poweroff -f       # optional, mirrors a real shutdown
```

Then in the **Renode monitor** (the prompt above the kernel
output, not inside Linux):

```text
quit
```

…or `Ctrl-D`. The Codespace stays alive; you're ready for
`lab 03`.

## What this lab proves

- Renode can simulate a full multi-hart RV64 SoC fast enough to
  boot a real Linux kernel.
- The same three primitives as lab 01 (`mach create`,
  `LoadPlatformDescription`, `LoadELF` + `start`) scale up to a
  100×-larger platform without changing shape.
- You can attach UART backends, single-step individual harts,
  and freeze/replay simulated time even while the kernel is
  running.
- Nothing on your laptop is RISC-V. The kernel doesn't know.
