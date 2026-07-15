# Running the transformer *on* the Coral NPU (real toolchain)

This directory contains the transformer **compiled to run on the Coral NPU
itself** — a genuine functional offload, not the interface-only add-sample
demo. The kernel is bare-metal `rv32im` (the Coral core is a RISC-V rv32im
machine plus Kelvin SIMD, which a scalar int8 matmul doesn't need), halts
via the real Kelvin **`mpause`** instruction (`0x08000073`), and exchanges
data with the host through the NPU's DTCM.

```
kernel/
  crt0.S             entry: set sp in DTCM, zero .bss, call kernel_main, mpause
  npu_transformer.c  the int8 transformer (same weights as ../src/model.h)
  link.ld            .text/.rodata -> ITCM (0x0), .bss/stack -> DTCM (0x10000)
  Makefile           builds npu_transformer.elf (+ .bin) with rv32im
```

## Two ways to run it

### A) Inside the robot lab, via Renode `CPU.CoralNPU`

```bash
tools/build_kernel.sh     # compiles the kernel -> binaries/coralnpu_kernel.bin
lab 13                    # transformer.resc loads it into the NPU's ITCM
```

The host firmware (`../src/host.c`) then stages each tokenized command into
the DTCM mailbox, releases the NPU from reset, waits for `STATUS.HALTED`,
and reads the predicted intent back — **the whole transformer runs on the
Coral core**, the RV64 host only tokenizes and drives the actuator. When
the kernel is present the run prints `policy runs ON the NPU`; the
`# host_policy_*` vs `# npu_infer_*` markers give the per-core instruction
comparison.

The DTCM mailbox layout (NPU-local `0x10000` == host `0xE00010000`):

| Offset | Field | Direction |
|---|---|---|
| `0x000` | `LEN` (u32 token count) | host → NPU |
| `0x004` | `TOKENS[SEQ_LEN]` (u32) | host → NPU |
| `0x100` | `INTENT` (u32) | NPU → host |
| `0x104` | `LOGITS[N_CLASSES]` (i32) | NPU → host |
| `0x200` | `DONE` (u32 = `0x00C0DE00`) | NPU → host |

### B) Standalone on Google's Coral ISS (`coralnpu_sim`)

This runs the kernel on Google's own MPACT-Sim-based instruction simulator
from [`coralnpu-mpact`](https://github.com/google-coral/coralnpu-mpact) —
the highest-fidelity "it runs on Coral" proof, no Renode involved.

```bash
tools/setup_coralnpu_toolchain.sh   # builds coralnpu_sim (Linux x86_64; heavy)
tools/build_kernel.sh               # builds npu_transformer.elf
tools/run_on_coralnpu_sim.sh        # runs it; prints total cycles
tools/run_on_coralnpu_sim.sh --i    # interactive: `run`, then `reg info` (a0 = intent)
```

> In the Codespaces / devcontainer image `coralnpu_sim` is **prebuilt**
> (amd64), so you can skip `setup_coralnpu_toolchain.sh` and go straight to
> `build_kernel.sh` + `run_on_coralnpu_sim.sh`. To build the image without it,
> pass `--build-arg BUILD_CORALNPU_SIM=0`.

With no host staging, the kernel uses its built-in default command
(`"walk forward"` → `ADVANCE`, intent `0`), leaves the intent in `a0`, and
halts.

## Toolchain notes

- **Kernel build**: any `rv32im` toolchain. The Makefile defaults to the
  repo's `riscv64-elf-*` with `-march=rv32im -mabi=ilp32`; override with
  `CROSS=riscv32-unknown-elf-` if you have a dedicated rv32 build. `mpause`
  is emitted as a raw `.word 0x08000073` (stock assemblers don't know the
  mnemonic), per the `coralnpu-mpact` ISA definition.
- **`coralnpu_sim` build**: bazel (pinned via `.bazelversion`) + MPACT-Sim +
  its LLVM/protobuf deps. This is **Linux x86_64** and a large build — it is
  meant for the Codespaces / CI image, not macOS/arm64. The kernel itself
  builds anywhere; only path (B) needs the ISS.
- **Higher-fidelity SIMD**: to accelerate the matmul with the Coral vector
  engine, compile the kernel with the Kelvin LLVM toolchain and vector
  intrinsics instead of scalar `rv32im`. The mailbox/ABI here stays the same.
