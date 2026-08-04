# Lab 13 — Transformer robot policy on a Coral NPU (optional)

> **Optional capstone.** An **end-to-end robot controller** in the spirit
> of lab 12 — natural-language command → on-device model → skill plan →
> memory-mapped actuator — but with a **transformer** brain that runs **on a
> Google Coral NPU**. A bare-metal RV64 "host" tokenizes a command; the
> **int8 transformer policy** (embedding → self-attention → feed-forward →
> classifier) executes **on the Coral core** — a real `rv32im` kernel
> (`coralnpu/kernel/`) loaded into the NPU's instruction TCM, halting via the
> Kelvin `mpause` instruction — and the host reads the predicted **intent**
> back from DTCM, expands it into low-level **skills**, and drives the
> actuator. Renode's per-core instruction counts show the transformer's
> matmul work moving off the host onto the NPU
> ([Antmicro's Coral demo](https://antmicro.com/blog/2026/07/renode-and-verilator-for-coral-npu)
> methodology). The same kernel also runs standalone on Google's own Coral
> instruction simulator from
> [`coralnpu-mpact`](https://github.com/google-coral/coralnpu-mpact).

> **⚠️ Needs Renode NIGHTLY.** `CPU.CoralNPU` ships only in Renode
> **nightly** builds. To avoid destabilising labs 00–12 the dev image
> installs the nightly **side-by-side** with the pinned stable release;
> lab 13 uses `renode-nightly` / `renode-test-nightly`, everything else
> keeps using stable `renode`. See [§7](#7-renode-versions-stable-vs-nightly).

## The idea in one picture

```
 "walk forward"                                     Coral NPU (CPU.CoralNPU)
      │  tokenize                                  ┌────────────────────────┐
      ▼                                            │ ITCM 0xE00000000 kernel│
  [walk, forward]   ── transformer policy ──►      │ DTCM 0xE00010000 data  │
      │              embed→attn→ffn→classify  ◄──── │ CSR  0xE00030000 ctrl  │
      ▼              (matmuls offloaded here)       │  vector/matrix engine  │
   intent: ADVANCE                                  └────────────────────────┘
      │  plan
      ▼
  [WALK vel=100 dist=300] ─► actuator @ 0x90000000 (SKILL_ID, PARAM0/1, STEP)
  [STOP]                     (RobotActuator.cs — observable from the monitor)
```

The learned part — the "AI" — is a genuine transformer encoder block, all
integer (int8 weights, int32 accumulate, an integer-softmax LUT). The
skills it triggers are pre-provided primitives, the same "model selects
skills, skills are pre-built" split as lab 12 (and the LFM2.5/Jetson demo
it echoes), now with attention/FFN matmuls that are the natural NPU
offload target.

## 1. Run it

```bash
lab 13
```

This mirrors `/labs/13-coralnpu-transformer/` into your work tree, runs
`make` to cross-compile `host.elf`, then launches `renode/transformer.resc`
with **`renode-nightly`** — which compiles + loads the C# actuator, loads a
Coral NPU kernel into the NPU's instruction TCM, loads the firmware, and
starts the host CPU.

For the **functional offload** (transformer running *on* the NPU), build
the kernel first (one-time, needs an `rv32im` toolchain):

```bash
cd ~/work/13-coralnpu-transformer && tools/build_kernel.sh   # -> binaries/coralnpu_kernel.bin
```

If that kernel is present the policy runs **on the Coral core**; otherwise
the run falls back to the host CPU (still a complete robot) and tells you so.
See [`coralnpu/README.md`](coralnpu/README.md). You'll see:

```
*** Transformer robot policy accelerated by a Coral NPU ***
command -> tokenize -> int8 transformer -> intent -> skills -> actuator
Host: bare-metal RV64. Policy engine: CPU.CoralNPU (rv32im kernel).

Coral NPU: transformer kernel online -> policy runs ON the NPU.

=== where does the transformer run? command: "walk forward" ===
# host_policy_start
# host_policy_done
host CPU  : intent ADVANCE, 4224 MACs on the host
# npu_infer_start
# npu_infer_done
Coral NPU : intent ADVANCE  (matches host)
            host did almost no work; the matmuls ran on the NPU.

=== executing commands ===

---- command 1 of 8 ----
input : "walk forward"
tokens: walk forward
intent: ADVANCE  (on Coral NPU)
plan  : 2 skills
    [0] WALK  vel=100cm/s dist=300cm
    [1] STOP
...
---- command 7 of 8 ----
input : "return home"
tokens: return home
intent: RETURN_HOME  (on Coral NPU)
plan  : 3 skills
    [0] TURN  angle=180deg
    [1] WALK  vel=80cm/s dist=500cm
    [2] DOCK

All commands executed. Robot idle (wfi).
```

On the marker lines the `.resc` prints a **per-core instruction count**: the
`# host_policy_*` phase burns thousands of **host** instructions on the
matmuls, while `# npu_infer_*` shows those instructions retire on the
**NPU** instead (the host just stages tokens and polls) — the contrast that
motivates the accelerator.

**Seeing the output** (as in the other labs): headless it scrolls in the
monitor (`sysbus.uart CreateFileBackend @/tmp/coral.log true` to capture);
GUI mode (`LAB_GUI=1`) opens a console analyzer in the noVNC tab. Re-run
with `machine Reset` then `start`. Watch the "hardware" with
`logLevel 0 sysbus.actuator` — each skill logs an actuator line.

**Headless regression:**

```bash
make test          # uses renode-test-nightly
```

## 2. The pipeline

Once per command, in `src/host.c`:

1. **Tokenize** — split the command on spaces; each word is matched against
   the vocabulary (`vocab` in `model.h`) to a token id. Unknown words are
   ignored, like a real tokenizer (`"return to base"` → `return`, `base`).
2. **Policy (transformer)** — a one-block encoder over the token sequence:

   ```
   tokens ─► embedding[id]          (int8 lookup, D_MODEL=16 wide)
          ─► self-attention          Q,K,V,O projections = int8 matmuls
                                      scores = Q·Kᵀ, integer-softmax LUT, Σ w·V
          ─► + residual
          ─► feed-forward             W1 (D_MODEL→D_FF), ReLU, W2 (→D_MODEL)
          ─► + residual, mean-pool over the sequence
          ─► classifier               logits = pooled · W_cls, argmax → intent
   ```

   The projections, FFN and classifier are `int8 × int32 → int32`
   multiply-accumulate loops — **no floating point** (hence `rv64imac`).
   Softmax avoids `exp()` via a small quantized LUT. Weights are
   hand-constructed so the routing is interpretable: every vocabulary word
   belongs to an intent, and the network routes a command to the intent its
   words vote for.
3. **Plan** — each intent expands to a fixed sequence of low-level skills:

   | Intent | Example command | Plan (skills) |
   |---|---|---|
   | `ADVANCE` | "walk forward", "move ahead" | WALK(100,300) → STOP |
   | `RETREAT` | "retreat backward", "reverse now" | WALK(−50,200) → STOP |
   | `TURN_AROUND` | "turn around", "spin left" | TURN(180°) → STOP |
   | `RETURN_HOME` | "return home", "return to base" | TURN(180°) → WALK(80,500) → DOCK |

4. **Drive** — write each skill's `SKILL_ID / PARAM0 / PARAM1 / STEP` to the
   actuator register block; the C# peripheral mirrors and logs it.

## 3. How the host drives the Coral NPU

`CPU.CoralNPU` exposes three windows on the system bus (base
`0xE00000000`):

| Window | Address | Purpose |
|---|---|---|
| **ITCM** | `0xE00000000` | instruction TCM — the NPU kernel is loaded here |
| **DTCM** | `0xE00010000` | data TCM — inputs and outputs live here |
| **CSR** | `0xE00030000` | control/status registers |

CSRs (see `src/host.c`): `RESET_CONTROL` (`0x00`, bit0 `RESET`, bit1
`CLOCK_GATE`), `STATUS` (`0x08`, bit0 `HALTED`, bit1 `FAULT`). To run one
inference the firmware stages the tokenized command into the DTCM mailbox,
then performs the standard accelerator handshake:

```c
MB_LEN = len;                       // DTCM mailbox: token count + ids
for (i = 0; i < len; i++) MB_TOKENS[i] = tokens[i];
MB_DONE = 0;
NPU_CSR(RESET_CONTROL) = 0x3;       // hold in reset, clock gated
NPU_CSR(RESET_CONTROL) = 0x1;       // release clock gate
NPU_CSR(RESET_CONTROL) = 0x0;       // release reset -> the kernel executes
while (!(NPU_CSR(STATUS) & 1))      // poll until HALTED (kernel ends in mpause)
    ;
int intent = MB_INTENT;             // read the prediction back from DTCM
```

The kernel (`coralnpu/kernel/npu_transformer.c`) reads the token sequence
from DTCM, runs the whole int8 transformer on the Coral core, writes the
predicted intent + logits + a `DONE` magic back to DTCM, and halts. The host
detects the magic to confirm a real transformer kernel is loaded (vs. the
plain add sample, which triggers the host fallback).

## 4. The transformer kernel that runs on the NPU

The functional offload lives in [`coralnpu/`](coralnpu/README.md): a
bare-metal **`rv32im`** kernel (the Coral core is a RISC-V rv32im machine),
built with a stock RISC-V toolchain and halting via the real Kelvin
**`mpause`** instruction (`0x08000073`, from the `coralnpu-mpact` ISA). It
uses the *same weights* as the host reference (`src/model.h`), so both
engines agree bit for bit.

```bash
tools/build_kernel.sh                 # rv32im kernel -> binaries/coralnpu_kernel.bin
```

You can also run that exact kernel on **Google's own Coral instruction
simulator** (no Renode) from
[`coralnpu-mpact`](https://github.com/google-coral/coralnpu-mpact):

```bash
tools/setup_coralnpu_toolchain.sh     # builds coralnpu_sim (Linux x86_64; heavy: bazel + MPACT-Sim)
tools/run_on_coralnpu_sim.sh          # runs the transformer on the Coral ISS; prints cycle count
```

**Going faster with the vector engine.** The scalar `rv32im` kernel already
executes on the NPU; to use the Coral SIMD/matrix unit, recompile the same
kernel with the Kelvin LLVM toolchain and vector intrinsics — the DTCM
mailbox/ABI is unchanged, so `host.c` and the `.resc` need no edits.

## 5. Files

| File | What it is |
|---|---|
| `src/host.c` | Host firmware: tokenize → offload policy to NPU (host fallback) → plan → drive actuator. |
| `src/model.h` | **Checked-in** int8 weights, softmax LUT, vocabulary, intents, sample commands. |
| `src/start.S`, `src/link.ld` | RV64 host startup + linker script. |
| `coralnpu/kernel/npu_transformer.c` | The int8 transformer **compiled to run on the Coral NPU** (rv32im). |
| `coralnpu/kernel/crt0.S`, `link.ld`, `Makefile` | Kernel entry (`mpause` halt), ITCM/DTCM layout, rv32im build. |
| `coralnpu/README.md` | The functional-offload kernel + toolchain (Renode and standalone `coralnpu_sim`). |
| `renode/transformer.repl` | Board: RV64 host + RAM + UART + actuator + `CPU.CoralNPU`. |
| `renode/transformer.resc` | Loads the C# actuator + transformer kernel + `host.elf`, sets up instruction-count hooks. |
| `peripherals/RobotActuator.cs` | Custom C# actuator: mirrors MMIO writes and logs each skill. |
| `tools/gen_model.py` | Regenerate `src/model.h` (plain Python, no deps). |
| `tools/build_kernel.sh` | Build the rv32im transformer kernel → `binaries/coralnpu_kernel.bin`. |
| `tools/setup_coralnpu_toolchain.sh` | Build Google's `coralnpu_sim` ISS (bazel + MPACT-Sim; Linux x86_64). |
| `tools/run_on_coralnpu_sim.sh` | Run the kernel on the Coral ISS standalone. |
| `tools/fetch_npu_kernel.sh` | Fetch the Antmicro add sample into `binaries/` (interface-only fallback). |
| `tests/transformer.robot` | Headless regression (`make test`, **nightly**). |
| `Makefile` | Builds `host.elf`; `make model`; `make kernel`; `make test` (nightly). **Real TABs.** |

## 6. Useful monitor commands

| Command | What it does |
|---|---|
| `peripherals` | The board: `cpu`, `ram`, `uart`, `actuator`, Coral NPU. |
| `sysbus.cpu ExecutedInstructions` | Host instructions retired so far. |
| `sysbus.npu ExecutedInstructions` | Instructions the Coral NPU has run (cumulative). |
| `sysbus ReadDoubleWord 0x90000000` | Last commanded skill id at the actuator. |
| `sysbus ReadDoubleWord 0xE00010100` | The NPU's predicted intent in DTCM (`MB_INTENT`). |
| `logLevel 0 sysbus.actuator` | Log every actuator register access. |
| `machine Reset` then `start` | Re-run the whole demo. |
| `quit` | Exit Renode. |

## 7. Renode versions: stable vs nightly

`CPU.CoralNPU` is a recent addition available **only in Renode nightly**.
The dev container installs both, side by side, so nothing else breaks:

| | Path | Symlink | Used by |
|---|---|---|---|
| **Stable** (pinned) | `/opt/renode` | `renode`, `renode-test` | labs 00–12 |
| **Nightly** | `/opt/renode-nightly` | `renode-nightly`, `renode-test-nightly` | **lab 13 only** |

`lab 13` and this lab's `make test` invoke the nightly explicitly; every
other lab is untouched. On **arm64** hosts the nightly is not published as a
portable tarball, so lab 13 is skipped there while labs 00–12 keep working —
Codespaces / CI are amd64, where lab 13 runs.

## 8. Mini-experiments (try at least one)

1. **Give the robot a new command.** Add a string to `SAMPLE_COMMANDS` in
   `tools/gen_model.py` using words from the vocab (e.g. `"go home"`,
   `"spin right"`), `make model && make`, re-run, and watch it route +
   plan.
2. **Teach a new word.** Add a synonym to a `VOCAB_BY_INTENT` list (e.g.
   `"forwards"` under `ADVANCE`), regenerate, rebuild — the transformer now
   routes commands containing it.
3. **Change a skill plan.** Edit `plan_home[]` (or any plan) in `src/host.c`
   — add a mid-plan `TURN`, change velocities/distances — and watch the
   executed skills and actuator writes follow.
4. **Watch the hardware.** `logLevel 0 sysbus.actuator` before `start`, or
   `pause` mid-run and read `0x90000000` to see the exact skill executing.
5. **Read the NPU state live.** `pause` and read `STATUS`
   (`sysbus ReadDoubleWord 0xE00030008`) and the predicted intent in DTCM
   (`0xE00010100`).
6. **Run the kernel on Google's ISS.** Build `coralnpu_sim`
   (`tools/setup_coralnpu_toolchain.sh`) and run the transformer on it
   standalone (`tools/run_on_coralnpu_sim.sh --i`, then `run`, `reg info` —
   `a0` is the predicted intent). See [§4](#4-the-transformer-kernel-that-runs-on-the-npu).
7. **Accelerate with the vector engine** (advanced): recompile
   `coralnpu/kernel` with the Kelvin LLVM toolchain + vector intrinsics; the
   mailbox ABI is unchanged, so nothing else needs editing.

## What this lab proves

- The **language-policy-as-robot-controller** pattern — command →
  transformer → skill plan → actuators — is just code, and Renode models
  the whole loop (host CPU, accelerator, memory-mapped actuator) **entirely
  offline**, before any silicon exists.
- A **real int8 transformer** runs as plain firmware with no FPU, and its
  cost is dominated by **matmuls** — precisely the workload NPUs accelerate.
- Renode can **co-simulate a host CPU and the Google Coral NPU** on one bus,
  with the host driving the accelerator over a realistic **CSR / TCM MMIO**
  protocol, and its **per-core instruction counting** quantifies the
  offload.
- The transformer is **compiled for and executed on the Coral core** (an
  `rv32im` kernel halting via Kelvin `mpause`), so the offload is functional,
  not just architectural — and the *same* kernel runs on Google's own
  `coralnpu-mpact` instruction simulator, cross-checking the Renode model.
- The same structure scales up to a vector/matrix-engine kernel (Kelvin LLVM
  toolchain) and a real quantized policy without changing the host or wiring.
