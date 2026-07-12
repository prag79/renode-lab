# Lab 09 — Edge AI: run a neural network on a bare-metal RISC-V core

"Edge AI" / TinyML means running a trained machine-learning model
*on the device* — a microcontroller with no OS, no GPU, often no
floating-point unit — instead of shipping data to a server. This lab
does exactly that in Renode: a small **int8-quantized neural network**
classifies handwritten digits, running as plain integer arithmetic on
a bare-metal RV64 core. No accelerator, no ML framework, no float.

The point: an ML model on an MCU is *just code and a table of numbers*.
The "AI" is two integer matrix-vector multiplies you can read in full
in `src/digits.c`.

> Want the industrial-strength version — a real TensorFlow Lite Micro
> model with a hardware accelerator? See the **[appendix](#appendix--the-real-thing-tflite-micro--a-cfu-accelerator)**.

## 1. Run it

```bash
lab 09
```

This mirrors `/labs/09-edge-ai/` into your work tree, runs `make` to
cross-compile `digits.elf`, then launches `renode/edge-ai.resc`, which
loads the firmware and starts the CPU. On the `uart` console you'll see
each 8x8 digit drawn as ASCII art, the model's prediction, the raw
output "logits", and a running accuracy tally:

```
*** Edge AI on RISC-V: handwritten-digit recognition ***
Model: int8-quantized MLP (64->32->10), integer-only inference, no FPU.
Classifying 10 sample 8x8 digits...

---- sample 1 of 10 ----
                  ==**  .
              ..**##%%..
              ..##%%..==**
          ..**##  ..**##
        ..##%%##%%##@@++
        ..++%%##%%%%++
                  ..%%..
                  ..%%..
  prediction = 4  (true = 4)  OK
  logits: -1234 ... 5678 ...

...

==== accuracy: 10 / 10 correct ====
Inference complete. The CPU is now idle (wfi).
```

**Seeing the output:**

- **Headless (default, `LAB_GUI=0`):** the run auto-`start`s, so the
  output scrolls in the Renode monitor. To capture it to a file, re-run
  and before `start` (or after `machine Reset`) attach a backend:

  ```text
  sysbus.uart CreateFileBackend @/tmp/edge-ai.log true
  ```

  ```bash
  tail -f /tmp/edge-ai.log
  ```

- **GUI mode (`LAB_GUI=1`):** the script runs `showAnalyzer
  sysbus.uart`, opening a console window in the noVNC tab on port
  **6080** (`/vnc.html`).

Re-run the whole inference any time from the monitor:

```text
machine Reset
start
```

## 2. What the model is

A classic 2-layer **multilayer perceptron (MLP)** trained on
scikit-learn's 8x8 handwritten-digit dataset (a mini-MNIST: 64 grayscale
pixels, values 0–16, ten classes 0–9):

```
input: 64 pixels (0..16)
   │  W1 [32 x 64]  + b1
   ▼
ReLU  →  requantize (>> shift)          # 32 hidden units, int8
   │  W2 [10 x 32]  + b2
   ▼
argmax over 10 logits  →  predicted digit
```

Held-out accuracy is ~97% — genuinely learned, not hard-coded. The
float model was trained offline; then every weight was **quantized to
int8** and the whole forward pass converted to integers.

## 3. Why integer-only? (the core TinyML idea)

Real edge devices avoid floating point because it's big and slow (many
MCUs have no FPU at all — this RV64 core is built `rv64imac`, **no `f`/`d`
extension**). The standard trick is **quantization**: store weights as
8-bit integers and do the math with int32 accumulators.

- **Smaller:** int8 weights are 4× smaller than float32. This whole
  model is ~3 KB.
- **Faster:** integer multiply-accumulate, no soft-float library. Note
  the Makefile links `-nostdlib` with *no* `-lgcc` — that only works
  because there isn't a single float op in the inference.
- **Good enough:** int8 inference here actually matches the float model
  (~97%). Modern TinyML leans on this heavily.

The forward pass in `src/digits.c` is the whole story:

```c
/* Layer 1: hidden = requantize(ReLU(W1 * x + b1)) */
for (int j = 0; j < HID_DIM; j++) {
    int32_t acc = model_b1[j];
    const int8_t *w = &model_w1[j * IN_DIM];
    for (int i = 0; i < IN_DIM; i++)
        acc += (int32_t)w[i] * (int32_t)x[i];   // int8 * int8 -> int32
    if (acc < 0) acc = 0;                        // ReLU
    acc >>= REQUANT_SHIFT;                       // int32 -> int8 range
    if (acc > 127) acc = 127;
    hidden[j] = acc;
}
/* Layer 2: logits = W2 * hidden + b2, then argmax */
```

`REQUANT_SHIFT` (a right shift, i.e. divide by a power of two) rescales
the layer-1 accumulator back into the int8 range for layer 2. That single
shift *is* the requantization step; the training tool picked the shift
value that maximizes accuracy.

## 4. Where the weights come from (`tools/gen_model.py`)

`src/model.h` is **auto-generated** and checked in, so the lab builds
with no Python or internet. To regenerate it (retrain + requantize):

```bash
pip install numpy scikit-learn      # only needed to regenerate
make model                          # writes a fresh src/model.h
make                                # rebuild firmware with new weights
```

`tools/gen_model.py` trains the float MLP, quantizes each layer to int8
with a per-layer scale, then **simulates the exact integer pipeline the
C runs** to (a) pick the best `REQUANT_SHIFT` and (b) choose sample
digits the integer model classifies correctly. Because the Python and C
compute identical integer math, the predictions match bit-for-bit.

## 5. Files

| File | What it is |
|---|---|
| `renode/edge-ai.repl` | The board: one RV64 core + 64 MiB RAM + a UART. |
| `renode/edge-ai.resc` | Loads `digits.elf` and runs it. |
| `src/digits.c` | Firmware: the integer forward pass + UART printing. |
| `src/model.h` | **Auto-generated** int8 weights, biases, shift, samples. |
| `src/start.S`, `src/link.ld` | RV64 startup + linker script. |
| `tools/gen_model.py` | Dev-time: train + quantize + emit `model.h`. |
| `Makefile` | Builds `digits.elf`; `make model` regenerates weights. |

## 6. Useful monitor commands

| Command | What it does |
|---|---|
| `peripherals` | The whole "board": `cpu`, `ram`, `uart`. |
| `sysbus.cpu PC` | Where the CPU is (in `wfi` after inference). |
| `sysbus.cpu ExecutedInstructions` | Rough cost of classifying 10 digits. |
| `machine Reset` then `start` | Re-run the full inference. |
| `logLevel 0 sysbus.uart` | Trace every UART register access. |
| `quit` | Exit Renode. |

## 7. Mini-experiments (try at least one)

1. **Count the work.** After it halts, `pause` then
   `sysbus.cpu ExecutedInstructions`. That's how many instructions the
   whole 10-digit classification took — your model's "inference cost".

2. **Feed it your own digit.** Edit one `sample_images[...]` row in
   `src/model.h` (values 0–16, an 8x8 grid), re-run `lab 09`, and watch
   the ASCII art and prediction change. Drawing a clear `1` (a vertical
   stroke) is easy to get right.

3. **Retrain a different shape.** In `tools/gen_model.py` change
   `HID_DIM` to 16 (or 64), `make model && make`, re-run. Fewer hidden
   units = smaller/faster but usually lower accuracy — the classic edge
   size-vs-accuracy trade-off, measured live.

4. **Break the quantization.** In `gen_model.py`, force `shift` to a bad
   value (e.g. hard-code `shift = 0`), regenerate, and see accuracy
   collapse — a concrete demo of why the requant scale matters.

5. **Watch it think.** `logLevel 0 sysbus.uart` before `start`: every
   character the model prints is a UART bus write you can see on the
   bus, the same way you'd debug a real device's console.

## What this lab proves

- A neural network on an MCU is just weights + a few loops of integer
  multiply-accumulate — no magic, no framework required.
- **Quantization** (int8 weights, int32 accumulators, a shift to
  requantize) is what makes ML fit and run on a CPU with no FPU.
- Renode runs this exactly like real firmware: you can measure the
  instruction cost, trace the bus, reset and re-run — a real
  bring-up/debug loop for edge-AI firmware, before any hardware exists.

---

## Appendix — the real thing: TFLite Micro + a CFU accelerator

> **This is now [lab 10](../10-tflite-micro/).** `lab 10` runs the real
> production stack described below (prebuilt, offline); `lab 10 cfu` adds
> the Verilated hardware accelerator. Read on for the concepts.

The lab above is deliberately self-contained. Production edge AI adds
two things this lab omits, and Renode models **both**:

### A. A real TensorFlow Lite Micro model

Antmicro (Renode's authors) maintain full TFLM examples — magic-wand
gesture recognition, micro-speech keyword spotting, person detection —
running on modelled boards such as the **Arduino Nano 33 BLE Sense
(nRF52840)** and **STM32F746**. Those run the *same idea* as this lab
(quantized ops on the CPU) but through the real TFLM interpreter and
operator kernels, driven by real sensor data fed into modelled
peripherals. They're heavier to build (you vendor the TFLM library and a
`.tflite` model), which is why this lab hand-rolls the forward pass
instead.

### B. A hardware accelerator (CFU)

Renode ships a genuine ML-accelerator demo: **LiteX + VexRiscv with a
Custom Function Unit (CFU)** from Google's
[CFU-Playground](https://github.com/google/CFU-Playground). The CFU is
custom silicon that accelerates the int8 multiply-accumulate at the
heart of quantized inference — the hardware answer to section 3's
software loop. Renode co-simulates the CFU with Verilator, so you run
*real accelerated TFLM* (e.g. MobileNetV2) cycle-by-cycle.

Try it (needs internet the first time — it downloads a prebuilt
Verilated binary + software from Antmicro's asset server, and works on
**x86-64** hosts):

```bash
lab monitor
```

then in the Renode monitor:

```text
i @scripts/single-node/litex_vexriscv_verilated_cfu.resc
start
```

Pick the `TfLM Models menu` / `Functional CFU Test` options in the UART
menu to run models with and without the accelerator and compare cycle
counts. This is the same workflow chip teams use to co-design an ML
accelerator and its firmware *before the silicon exists* — the natural
sequel to lab 07 (model your own peripheral) applied to AI hardware.

> Caveat: the CFU demo relies on a host-architecture Verilated binary
> (x86-64) downloaded at runtime, so unlike labs 00–09 it is **not**
> fully offline or arm64-safe. That trade-off is exactly why the main
> lab is the self-contained integer MLP.
