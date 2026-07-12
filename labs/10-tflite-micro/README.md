# Lab 10 — The real deal: TensorFlow Lite Micro on RISC-V

Lab 09 hand-rolled a neural network to show edge AI from first
principles. This lab runs the **real production stack**: unmodified
**TensorFlow Lite Micro** (TFLM) — Google's ML runtime for
microcontrollers — inside a **Zephyr RTOS** firmware image, on a
**LiteX/VexRiscv** RISC-V soft-SoC, classifying live accelerometer
gestures. Then, optionally, it runs the *same* kind of workload with a
**hardware accelerator** (a Verilated Custom Function Unit) co-simulated
cycle-by-cycle.

Nothing is faked: it's the actual TFLM interpreter and operator kernels,
the actual gesture model, fed real recorded sensor data through a
modelled ADXL345 accelerometer over I2C.

> **Credit / license.** The firmware, platform, gesture data and robot
> test come from Antmicro's
> [`litex-vexriscv-tensorflow-lite-demo`](https://github.com/antmicro/litex-vexriscv-tensorflow-lite-demo)
> (Apache-2.0). The prebuilt `zephyr.elf` is vendored so the lab runs
> offline; the upstream license is kept as `LICENSE.upstream`.

## 1. Run it (the Magic Wand demo)

```bash
lab 10
```

This mirrors the lab into your work tree and launches
`renode/magic-wand.resc`, which loads the **prebuilt** TFLM firmware
(`binaries/magic_wand/zephyr.elf` — no build needed) and streams a
sequence of recorded gestures into the accelerometer. On the `uart`
console you'll see Zephyr boot, then the model classifying each motion
and drawing it as ASCII art:

```
*** Booting Zephyr OS build ... ***
Got accelerometer, label: accel-0

RING:
          *
       *     *
     *         *
    *           *
     *         *
       *     *
          *

SLOPE:
        *
       *
      *
     *
    *
   *
  *
 * * * * * * * *
```

**This is real TFLM inference and it is compute-heavy** — the first
gesture can take ~30–60 s of wall-clock time as the interpreter runs on
the simulated CPU. That slowness is the point: you're watching an MCU do
the exact work it would do in silicon.

## 2. What's actually running

```
Zephyr RTOS
  └── TensorFlow Lite Micro interpreter
        └── "magic wand" CNN gesture model (int8)
              ↑ reads accelerometer samples over I2C
LiteX/VexRiscv SoC (rv32imac)  ── modelled by Renode
  ├── VexRiscv CPU        (runs the whole stack)
  ├── LiteX UART/timer    (console + ticks)
  └── LiteX I2C → ADXL345 (accelerometer, fed canned gestures)
```

- **The model** is a small convolutional net trained on the
  [Magic Wand](https://github.com/tensorflow/tensorflow/tree/master/tensorflow/lite/micro/examples/magic_wand)
  dataset (wing/ring/slope gestures), quantized to int8 — the same
  quantization idea you built by hand in lab 09, now done by the TFLM
  toolchain.
- **The sensor** is a modelled `ADXL345`. `i2c.adxl345 FeedSample`
  injects recorded motion (`renode/circle.data`, `renode/angle.data`)
  so the run is deterministic and repeatable — no real hardware, no
  waving your arm.
- **The firmware** is a normal Zephyr app; TFLM is just a library linked
  into it. Renode runs the ELF unmodified.

## 3. Seeing the output

- **GUI mode (`LAB_GUI=1`, default in Codespaces):** the script runs
  `showAnalyzer sysbus.uart`, opening a console window in the noVNC tab
  on port **6080** (`/vnc.html`). Everything from boot scrolls there.

- **Headless (`LAB_GUI=0`):** the run auto-`start`s. To capture the full
  log from boot, attach a file backend and restart the firmware:

  ```text
  sysbus.uart CreateFileBackend @/tmp/magic-wand.log true
  machine Reset
  start
  ```

  ```bash
  tail -f /tmp/magic-wand.log
  ```

## 4. Regression-testing real ML (optional, lab 06 style)

The same "assert on UART" idea from lab 06 works on ML output. `make
test` boots the firmware headless, feeds gestures, and asserts the model
prints the expected `RING` and `SLOPE` art:

```bash
cd ~/work/10-tflite-micro
make test          # runs tests/magic-wand.robot via renode-test
```

Because TFLM inference is slow, the terminal tester uses a long timeout;
the whole suite can take a few minutes. A green run is a real
end-to-end check that the ML model still recognizes the gestures.

## 5. Files

| File | What it is |
|---|---|
| `binaries/magic_wand/zephyr.elf` | **Prebuilt** Zephyr + TFLM firmware (vendored). |
| `renode/magic-wand.repl` | LiteX/VexRiscv SoC + ADXL345 accelerometer. |
| `renode/magic-wand.resc` | Loads the firmware, feeds gestures, runs. |
| `renode/circle.data`, `renode/angle.data` | Recorded accelerometer gestures. |
| `renode/cfu.resc` | The optional CFU-accelerator variant (§7). |
| `tests/magic-wand.robot` | Robot suite asserting RING/SLOPE detection. |
| `Makefile` | `make test`; notes on rebuilding from source. |
| `LICENSE.upstream` | Apache-2.0 license of the upstream demo. |

## 6. Useful monitor commands

| Command | What it does |
|---|---|
| `peripherals` | The LiteX SoC: cpu, ram, uart, timer0, i2c, adxl345. |
| `i2c.adxl345 FeedSample @renode/circle.data` | Inject another gesture at runtime. |
| `sysbus.cpu PC` | Watch the VexRiscv run TFLM. |
| `logLevel 0 sysbus.i2c` | Trace every I2C transaction the driver makes. |
| `machine Reset` then `start` | Re-run from boot. |
| `quit` | Exit Renode. |

## 7. Optional: TFLM with a hardware accelerator (CFU)

The natural sequel to lab 07 ("model your own peripheral") applied to
AI: run TFLM on a VexRiscv core that has a **Custom Function Unit** —
extra silicon that accelerates the int8 multiply-accumulate at the heart
of quantized inference — and **co-simulate that CFU with Verilator**.

```bash
lab 10 cfu
```

This wraps Renode's built-in
[CFU-Playground](https://github.com/google/CFU-Playground) demo. In the
UART menu, use `Functional CFU Test` and the `TfLM Models menu` to run
inference with the accelerator and compare cycle counts to the software
path — exactly how chip teams co-design an ML accelerator and its
firmware *before silicon exists*.

> **Caveats (why this isn't the default):** unlike labs 00–10, the CFU
> demo is **not** self-contained. Its first run **downloads** a prebuilt
> Verilated binary + TFLM software from `dl.antmicro.com` (needs
> internet), and that binary is compiled for **x86-64 Linux** — so it
> works in a standard Codespace but **not on arm64** hosts.

## 8. Why not STM32F746 / Arduino Nano 33 BLE directly?

Renode models both boards, and Antmicro's
[Kenning Zephyr Runtime](https://github.com/antmicro/kenning-zephyr-runtime)
can run this exact magic-wand model on `stm32f746g_disco` and
`nrf52840dk`. We use the LiteX/VexRiscv target here because it ships a
**prebuilt** binary, so the lab runs with zero build. Building the STM32
or Arduino version requires a full Zephyr `west` toolchain (Zephyr SDK,
large downloads) — a great exercise, but out of scope for a click-to-run
lab. Pointers if you want to try it:

- **STM32F746 / nRF52840 (Zephyr + TFLM):**
  `west build --board stm32f746g_disco app -- -DEXTRA_CONF_FILE=tflite.conf`
  then `west build -t board-repl`, and simulate `build/zephyr/zephyr.elf`
  in Renode (see the Kenning Zephyr Runtime docs).
- **Arduino Nano 33 BLE Sense:** Renode's
  `scripts/complex/arduino_nano/` demo loads a magic-wand sketch built in
  the Arduino IDE and uploaded over a simulated USB-IP link.

## What this lab proves

- Real, unmodified TensorFlow Lite Micro runs in Renode on a RISC-V MCU
  — the production edge-AI stack, not a toy — reading real sensor data
  through a modelled peripheral.
- The int8 quantization you hand-built in lab 09 is exactly what makes
  this production model fit and run on the CPU.
- You can regression-test ML firmware headless (`make test`) and, with
  the CFU variant, co-design and benchmark a hardware ML accelerator in
  simulation — the full edge-AI development loop, no hardware required.
