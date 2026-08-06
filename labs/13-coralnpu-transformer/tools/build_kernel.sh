#!/usr/bin/env bash
#
# build_kernel.sh - compile the CoralNPU transformer kernel (rv32im) and
# stage it where the Renode script loads it.
#
# The kernel is bare-metal rv32im, so any rv32 toolchain works. We default
# to the repo's riscv64-unknown-elf-* with -march=rv32im; override with CROSS=...
# (e.g. CROSS=riscv32-unknown-elf-). Produces:
#   coralnpu/kernel/npu_transformer.elf   (for coralnpu_sim, has entry+symbols)
#   coralnpu/kernel/npu_transformer.bin   (flat ITCM image for Renode)
#   binaries/coralnpu_kernel.bin          (what transformer.resc auto-loads)
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
KDIR="$HERE/coralnpu/kernel"
OUT_DIR="$HERE/binaries"

echo "Regenerating shared model (tools/gen_model.py)..."
python3 "$HERE/tools/gen_model.py"

echo "Building CoralNPU kernel (rv32im)..."
make -C "$KDIR" clean
make -C "$KDIR" ${CROSS:+CROSS="$CROSS"}

mkdir -p "$OUT_DIR"
cp "$KDIR/npu_transformer.bin" "$OUT_DIR/coralnpu_kernel.bin"

echo "Kernel built:"
echo "  $KDIR/npu_transformer.elf  (run on coralnpu_sim)"
echo "  $OUT_DIR/coralnpu_kernel.bin  ($(wc -c < "$OUT_DIR/coralnpu_kernel.bin") bytes; loaded by transformer.resc)"
echo "Now run: lab 13   (Renode)   or   tools/run_on_coralnpu_sim.sh   (standalone ISS)"
