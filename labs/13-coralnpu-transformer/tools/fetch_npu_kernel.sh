#!/usr/bin/env bash
# Fetch a prebuilt Coral NPU kernel binary for offline use.
#
# The .resc can load Antmicro's hosted sample directly over the network,
# but grabbing a local copy makes `lab 13` work without internet and keeps
# runs reproducible. The binary is a Coral NPU (RISC-V rv32) payload that
# adds two operands staged in DTCM -- the firmware's NPU interface demo
# writes 2 and 5 and expects 7 back.
#
# This mirrors lab 10's "vendored prebuilt binary" approach: the heavy
# Coral toolchain (bazel + MLIR) is NOT required to run the lab. To build
# your own kernel (e.g. a real matmul/attention kernel), see the README.
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$HERE/binaries"
OUT="$OUT_DIR/coralnpu_kernel.bin"
URL="https://dl.antmicro.com/projects/renode/coralnpu_v2_hello_world_add_floats.bin-s_65648-0e3f5d6ae173fa2e06f6b5f91906ef721516de4c"

mkdir -p "$OUT_DIR"
echo "Fetching Coral NPU kernel -> $OUT"
if command -v wget >/dev/null 2>&1; then
    wget -q "$URL" -O "$OUT"
elif command -v curl >/dev/null 2>&1; then
    curl -fsSL "$URL" -o "$OUT"
else
    echo "need wget or curl" >&2; exit 1
fi
echo "done ($(wc -c < "$OUT") bytes). Now run: lab 13"
