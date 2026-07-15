#!/usr/bin/env bash
#
# setup_coralnpu_toolchain.sh - build Google's CoralNPU instruction
# simulator (coralnpu-mpact, MPACT-Sim based) so you can run the transformer
# kernel directly on the real Coral ISA.
#
# WHAT THIS BUILDS
#   * bazelisk (pinned bazel launcher; the repo pins bazel via .bazelversion)
#   * coralnpu-mpact @ //sim:coralnpu_sim   - the standalone Coral NPU ISS
#   * (optional) //sim/renode               - the Renode co-simulation lib
#
# REQUIREMENTS / CAVEATS
#   * Linux x86_64. MPACT-Sim + its LLVM/protobuf deps are Linux-oriented;
#     this is intended for the Codespaces / CI image, NOT macOS. It is a
#     large build (LLVM etc.): expect a multi-GB download and a long first
#     build. It does NOT run on arm64 hosts.
#   * A C++20 clang toolchain, git, and ~15 GB free disk.
#
# The Coral NPU *kernel* (coralnpu/kernel) does NOT need this simulator to
# build - a stock rv32im toolchain compiles it (see coralnpu/README.md).
# This script is for the higher-fidelity "run it on Google's own ISS" path.
set -euo pipefail

CORALNPU_MPACT_URL="https://github.com/google-coral/coralnpu-mpact.git"
CORALNPU_MPACT_REF="${CORALNPU_MPACT_REF:-main}"
PREFIX="${CORALNPU_PREFIX:-$HOME/coralnpu}"
SRC="$PREFIX/coralnpu-mpact"
BIN_DIR="$PREFIX/bin"

log() { printf '\n=== %s ===\n' "$*"; }

arch="$(uname -m)"; os="$(uname -s)"
if [ "$os" != "Linux" ] || { [ "$arch" != "x86_64" ] && [ "$arch" != "amd64" ]; }; then
    echo "WARNING: coralnpu-mpact expects Linux x86_64; you are on $os/$arch." >&2
    echo "         The build may fail. The kernel itself builds anywhere (see README)." >&2
fi

mkdir -p "$PREFIX" "$BIN_DIR"

# --- bazelisk (respects the repo's .bazelversion) --------------------------
if ! command -v bazel >/dev/null 2>&1 && ! command -v bazelisk >/dev/null 2>&1; then
    log "Installing bazelisk"
    curl -fsSL -o "$BIN_DIR/bazel" \
        "https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64"
    chmod +x "$BIN_DIR/bazel"
    export PATH="$BIN_DIR:$PATH"
    echo "Add to PATH: export PATH=\"$BIN_DIR:\$PATH\""
fi
BAZEL="$(command -v bazelisk || command -v bazel)"

# --- clone + build ---------------------------------------------------------
if [ ! -d "$SRC/.git" ]; then
    log "Cloning coralnpu-mpact ($CORALNPU_MPACT_REF)"
    git clone "$CORALNPU_MPACT_URL" "$SRC"
    git -C "$SRC" checkout "$CORALNPU_MPACT_REF"
fi

log "Building //sim:coralnpu_sim (this is the long one)"
( cd "$SRC" && "$BAZEL" build //sim:coralnpu_sim )

SIM="$SRC/bazel-bin/sim/coralnpu_sim"
if [ -x "$SIM" ]; then
    ln -sf "$SIM" "$BIN_DIR/coralnpu_sim"
    log "Done"
    echo "coralnpu_sim -> $BIN_DIR/coralnpu_sim"
    echo "Next: tools/build_kernel.sh && tools/run_on_coralnpu_sim.sh"
else
    echo "Build finished but $SIM not found; check bazel output." >&2
    exit 1
fi
