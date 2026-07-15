# Lab 12 — Edge-AI robot: on-device model → skill plan (optional)

> **Optional capstone.** This lab is inspired by
> [Liquid AI's LFM2.5-230M robotics demo](https://www.liquid.ai/blog/lfm2-5-230m),
> where a small language model runs **on-device** on a Jetson Orin and
> acts as a *skill-selection layer*: it takes one natural-language
> command and decomposes it into a sequence of tool calls that invoke
> pre-trained low-level skills. We reproduce that **architecture** — not
> the LLM — in Renode: a bare-metal RV64 "robot controller" runs a tiny
> **int8 intent model** that turns a command into a plan of low-level
> skills and drives a memory-mapped actuator. No GPU, no ML framework,
> no network — pure integer math, fully offline.

**Why not the real thing?** Renode has generic 64-bit **Cortex-A78**
Linux support, but **no Jetson Orin board model and no GPU/NVDLA
simulation** — so the GPU-accelerated 230M LLM that makes the LFM2.5 demo
fast simply can't run here. What Renode *is* great at is the firmware /
SoC / actuator / decision-loop side, so this lab keeps the **shape** of
that system (command → on-device model → skill plan → hardware) at a
scale that runs as plain firmware. See the root README's lab 12 note for
the full "what Renode can and can't do for Jetson" discussion.

## The idea in one picture

```
  natural language        on-device model            low-level skills
  "walk forward"   ──►   tokenize → int8 W·x  ──►   [WALK 100cm/s 300cm]
                          → argmax = ADVANCE         [STOP]
                                                         │  MMIO writes
                                                         ▼
                                                   actuator @ 0x90000000
                                                   (SKILL_ID, PARAM0/1, STEP)
```

The learned part — the "AI" — is one **int8 matrix-vector multiply plus
argmax**, readable in full in `src/robot.c`. The skills it selects are
pre-provided primitives, exactly the "model selects skills, skills are
pre-trained" split the LFM2.5 writeup describes (their skills come from
NVIDIA's SONIC framework; ours are a small hand-written library).

## 1. Run it

```bash
lab 12
```

This mirrors `/labs/12-edge-ai-robot/` into your work tree, runs `make` to
cross-compile `robot.elf`, then launches `renode/robot.resc`, which loads
the firmware and starts the CPU. For each command the controller prints
the input, the recognized tokens, the chosen intent (with its model
logit), and the expanded skill plan it executes:

```
*** Edge-AI robot: natural-language command -> skill plan ***
On-device int8 intent model (W*x, argmax) selects low-level skills.

---- command 1 of 8 ----
input : "walk forward"
tokens: walk forward
intent: ADVANCE  (logit 7)
plan  : 2 skills
    [0] WALK  vel=100cm/s dist=300cm
    [1] STOP

---- command 3 of 8 ----
input : "kneel and hold still"
tokens: kneel hold still
intent: KNEEL_HOLD  (logit 12)
plan  : 4 skills
    [0] HOLD  2s
    [1] KNEEL  5s
    [2] HOLD  2s
    [3] STOP
...
```

**Seeing the output:**

- **Headless (default, `LAB_GUI=0`):** the run auto-`start`s and scrolls
  in the Renode monitor. To capture it, attach a backend before `start`
  (or after `machine Reset`):

  ```text
  sysbus.uart CreateFileBackend @/tmp/robot.log true
  ```

  ```bash
  tail -f /tmp/robot.log
  ```

- **GUI mode (`LAB_GUI=1`):** `showAnalyzer sysbus.uart` opens a console
  window in the noVNC tab on port **6080** (`/vnc.html`).

Re-run any time from the monitor: `machine Reset` then `start`.

**Headless regression (lab 06 style):**

```bash
make test
```

`renode-test` boots the firmware, watches the UART, and asserts each
sample command classifies into the expected intent — pass/fail with a
non-zero exit code on regressions.

## 2. How the model works (the core idea)

A **linear intent classifier** — the smallest thing that still counts as
a learned model:

```
input command ──► bag-of-words vector x  (counts over a 20-word vocab)
                  logits = W · x          (W: int8 [7 intents × 20 words])
                  intent = argmax(logits)
```

`W` (in `src/model.h`) holds one row per intent; each row scores high on
that intent's keywords. Classifying is `int8 × int32 → int32` accumulate
then argmax — **no floating point anywhere**, which is exactly why it
fits a bare-metal MCU with no FPU (`rv64imac`, no `f`/`d`). The forward
pass is the whole "AI":

```c
for (int i = 0; i < NUM_INTENTS; i++) {
    int32_t acc = 0;
    for (int j = 0; j < VOCAB_SIZE; j++)
        acc += (int32_t)model_W[i][j] * x[j];   // int8 * int32 -> int32
    logits[i] = acc;                             // argmax picks the intent
}
```

The three steps around it are just as simple: `encode()` tokenizes the
string into `x` (words not in the vocabulary are ignored, like a real
tokenizer), the planner maps the intent to a fixed skill sequence, and
`drive()` writes each skill to the actuator.

## 3. From intent to skills (the "plan")

Each intent expands into a sequence of **low-level skills** — the robot's
pre-provided primitives. This is the decomposition step: one command
becomes an ordered plan of tool calls.

| Intent | Example command | Plan (skills) |
|---|---|---|
| `ADVANCE` | "walk forward" | WALK → STOP |
| `RETREAT` | "walk backward" | WALK(reverse) → STOP |
| `KNEEL_HOLD` | "kneel and hold still" | HOLD → KNEEL → HOLD → STOP |
| `TURN_AROUND` | "turn around" | TURN 180° → STOP |
| `PATROL` | "patrol the area" | WALK → TURN → WALK → TURN → WALK → DOCK |
| `RETURN_HOME` | "return home" / "go to the dock" | TURN → WALK → DOCK |
| `STOP` | "stop and wait" | STOP |

The skill library (`WALK`, `TURN`, `KNEEL`, `HOLD`, `STOP`, `DOCK`) and
the per-intent plans live in `src/robot.c`.

## 4. The actuator (memory-mapped "hardware")

Each skill is written to a small MMIO register block at `0x90000000`
(modeled as a **custom C# peripheral** in `peripherals/RobotActuator.cs`,
lab 07 pattern), so the robot's commanded state is observable from the
monitor — both by reading registers and by watching the actuator log
lines Renode prints on each skill write:

| Offset | Register | Meaning |
|---|---|---|
| `0x00` | `SKILL_ID` | current skill (1=WALK … 6=DOCK) |
| `0x04` | `PARAM0` | velocity (cm/s) or turn angle (deg) |
| `0x08` | `PARAM1` | distance (cm) or duration (s) |
| `0x0C` | `STEP` | index of this skill within the plan |

Inspect the last commanded skill after a run:

```text
pause
sysbus ReadDoubleWord 0x90000000    # SKILL_ID
sysbus ReadDoubleWord 0x90000004    # PARAM0
```

## 5. Files

| File | What it is |
|---|---|
| `src/robot.c` | Firmware: tokenize → int8 classify → plan → drive actuator + print. |
| `src/model.h` | **Checked-in** int8 weights, vocabulary, intent names, sample commands. |
| `src/start.S`, `src/link.ld` | RV64 startup + linker script (8-byte-aligned `.bss`). |
| `renode/robot.repl` | Board: RV64 core + 64 MiB RAM + UART + actuator peripheral. |
| `renode/robot.resc` | Loads the C# actuator, `robot.elf`, UART console, starts. |
| `peripherals/RobotActuator.cs` | Custom C# actuator: mirrors MMIO writes and logs each skill. |
| `tools/gen_model.py` | Optional: regenerate `src/model.h` (plain Python, no deps). |
| `tests/robot.robot` | Headless UART regression suite (`make test`). |
| `Makefile` | Builds `robot.elf`; `make model` regenerates weights; `make test` runs Robot. **Real TABs.** |

## 6. Useful monitor commands

| Command | What it does |
|---|---|
| `peripherals` | The board: `cpu`, `ram`, `uart`, `actuator`. |
| `sysbus.cpu PC` | Where the CPU is (in `wfi` after the last command). |
| `sysbus.cpu ExecutedInstructions` | Rough cost of running all 8 commands + plans. |
| `sysbus ReadDoubleWord 0x90000000` | Last commanded skill id at the actuator. |
| `machine Reset` then `start` | Re-run the whole demo. |
| `logLevel 0 sysbus.uart` | Trace every UART register access. |
| `quit` | Exit Renode. |

## 7. Mini-experiments (try at least one)

1. **Give it a new command.** Add a string to `sample_commands[]` in
   `src/model.h` (using words from `vocab`), re-run `lab 12`, and watch it
   classify + plan. Try `"go home"` or `"turn left and walk forward"`.

2. **Teach a new word.** Add a synonym to `vocab` and give it weight on an
   intent's row in `model_W` (e.g. add `"advance"` with weight 5 to the
   `ADVANCE` row). Re-run — `"advance"` now triggers the walk plan.

3. **Change a skill plan.** Edit `plan_patrol[]` (or any plan) in
   `src/robot.c` — add a `KNEEL` mid-patrol, change velocities/distances —
   and see the executed sequence and actuator writes follow.

4. **Watch the "hardware".** `logLevel 0 sysbus.uart` before `start`, or
   `pause` mid-run and read the actuator registers at `0x90000000` to see
   the exact skill the robot is executing.

5. **Regenerate the model.** `make model` rewrites `src/model.h` from the
   readable spec in `tools/gen_model.py`; add an intent there (plus a plan
   in `robot.c`) to grow the robot's repertoire, then `make`.

6. **Extend the actuator.** Edit `peripherals/RobotActuator.cs` — e.g.
   raise an IRQ when a skill finishes, count simulated distance, or
   reject invalid skill ids — turning the register block into a richer
   motor controller.

## What this lab proves

- The **edge-AI-as-robot-controller** pattern — natural language →
  on-device model → skill plan → actuators — is just code: a matrix
  multiply, an argmax, a table of skills, and some MMIO writes.
- A **learned decision layer runs with no FPU, no accelerator, no
  framework** (int8 integer math), which is exactly why TinyML fits on
  constrained robot MCUs.
- Renode models the whole loop — CPU, memory-mapped actuator, and a
  bring-up/debug workflow (reset, re-run, trace, inspect registers) —
  **entirely offline**, before any hardware exists.
- The same structure scales up: swap the int8 classifier for a real
  quantized model, the MMIO window for a modeled motor controller (lab
  07), or add sensor nodes on a bus (lab 08). The Jetson/LFM2.5 version is
  this shape with a GPU-class model and real skills — which is the part
  Renode leaves to hardware.
