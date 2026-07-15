#!/usr/bin/env bash
#
# run_on_coralnpu_sim.sh - run the transformer kernel on Google's CoralNPU
# instruction simulator (coralnpu_sim), proving the model executes on the
# real Coral ISA without Renode.
#
# Prereqs: tools/setup_coralnpu_toolchain.sh (builds coralnpu_sim) and
#          tools/build_kernel.sh (builds npu_transformer.elf).
#
# With no host staging the kernel uses its built-in default command
# ("walk forward" -> ADVANCE = intent 0), leaves the predicted intent in
# a0, and halts via `mpause`. Batch mode reports the cycle count; add --i
# for an interactive shell where `run` then `reg info` shows a0 = intent.
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
ELF="$HERE/coralnpu/kernel/npu_transformer.elf"
SIM="$(command -v coralnpu_sim || echo "${CORALNPU_PREFIX:-$HOME/coralnpu}/bin/coralnpu_sim")"

[ -x "$SIM" ] || { echo "coralnpu_sim not found; run tools/setup_coralnpu_toolchain.sh first." >&2; exit 1; }
[ -f "$ELF" ] || { echo "$ELF missing; run tools/build_kernel.sh first." >&2; exit 1; }

if [ "${1:-}" = "--i" ] || [ "${1:-}" = "-i" ]; then
    echo "Interactive: type 'run' then 'reg info' (a0 = predicted intent), then 'quit'."
    exec "$SIM" --i "$ELF"
else
    echo "Running transformer on coralnpu_sim (batch)..."
    "$SIM" "$ELF"
    echo
    echo "The kernel ran to 'mpause'. For the predicted intent, re-run with --i"
    echo "and inspect a0 via 'reg info', or read DTCM 0x10100 (OUT_INTENT)."
fi
