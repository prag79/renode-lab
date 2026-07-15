# syntax=docker/dockerfile:1.7

# ============================================================================
# Stage 1: build Google's CoralNPU instruction simulator (coralnpu_sim) from
# google-coral/coralnpu-mpact, so lab 13 can run the transformer kernel on the
# real Coral ISS with no build step at runtime.
#
# This is a MULTI-STAGE builder on purpose: the toolchain it needs (bazel +
# MPACT-Sim + protobuf/abseil/antlr) is several GB, but ONLY the final
# stripped `coralnpu_sim` binary is copied into the shipped image (see the
# `COPY --from=coralnpu-builder` near the end), so the runtime image grows by
# only the binary (~tens of MB), not the build tree.
#
# amd64 only (MPACT-Sim is x86_64-oriented); on arm64 we emit a stub that
# tells the user to build it on-demand. Toggle with --build-arg
# BUILD_CORALNPU_SIM=0 to skip the heavy build entirely (lab 13's Renode path
# and the on-demand tools/setup_coralnpu_toolchain.sh still work).
# ============================================================================
FROM ubuntu:22.04 AS coralnpu-builder
ARG BUILD_CORALNPU_SIM=1
ARG CORALNPU_MPACT_REF=main
ENV DEBIAN_FRONTEND=noninteractive
RUN if [ "$BUILD_CORALNPU_SIM" = "1" ] && [ "$(dpkg --print-architecture)" = "amd64" ]; then \
      apt-get update && apt-get install -y --no-install-recommends \
        git curl ca-certificates build-essential clang lld \
        python3 python3-dev default-jdk-headless zip unzip \
      && rm -rf /var/lib/apt/lists/* ; \
    fi
RUN mkdir -p /opt/coralnpu/bin
# The bazel cache is a BuildKit cache mount, so it speeds rebuilds WITHOUT
# ending up in this stage's layers (and it is a builder stage anyway).
RUN --mount=type=cache,target=/root/.cache/bazel \
    set -eu ; \
    if [ "$BUILD_CORALNPU_SIM" = "1" ] && [ "$(dpkg --print-architecture)" = "amd64" ]; then \
      curl -fsSL -o /usr/local/bin/bazel \
        https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64 && \
      chmod +x /usr/local/bin/bazel && \
      git clone --depth 1 --branch "$CORALNPU_MPACT_REF" \
        https://github.com/google-coral/coralnpu-mpact.git /src 2>/dev/null || \
        git clone https://github.com/google-coral/coralnpu-mpact.git /src && \
      cd /src && git checkout "$CORALNPU_MPACT_REF" && \
      CC=clang bazel build -c opt //sim:coralnpu_sim && \
      cp -L bazel-bin/sim/coralnpu_sim /opt/coralnpu/bin/coralnpu_sim && \
      ( strip /opt/coralnpu/bin/coralnpu_sim || true ) && \
      bazel shutdown || true ; \
    else \
      { echo '#!/bin/sh' ; \
        echo 'echo "coralnpu_sim was not prebuilt in this image" >&2' ; \
        echo 'echo "(amd64-only, or built with BUILD_CORALNPU_SIM=0)." >&2' ; \
        echo 'echo "Build it on demand: tools/setup_coralnpu_toolchain.sh" >&2' ; \
        echo 'exit 1' ; } > /opt/coralnpu/bin/coralnpu_sim && \
      chmod +x /opt/coralnpu/bin/coralnpu_sim ; \
    fi

# ============================================================================
# Stage 2: the shipped lab image.
# ============================================================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:1 \
    LAB_HOME=/labs

# Renode runtime + cross toolchains + headless desktop bits.
#
# gtk-sharp2 / libgtk2.0-cil / libgdiplus are the C# bindings + graphics
# runtime Mono needs to actually use GTK. Without them Renode starts
# fine in headless mode but any GUI analyzer (`showAnalyzer ... LEDAnalyzer`,
# `showAnalyzer sysbus.uart4`, GPIO widgets, ...) fails with:
#   "Received 'No backend found for ...' error while initializing analyzer"
# even when LAB_GUI=1 and Xvfb/x11vnc are alive. libgtk2.0-0 alone is
# the native library and is *not* enough - the CIL bindings are separate.
#
# IMPORTANT: never put `# comments` mid-line inside a `\`-continued RUN.
# A `#` consumes the trailing `\` and silently breaks line continuation;
# the next "line" then runs as a separate shell command.
RUN apt-get update && apt-get install -y --no-install-recommends \
        mono-complete libgtk2.0-0 screen policykit-1 \
        gtk-sharp2 libgtk2.0-cil libgdiplus \
        python3 python3-pip \
        gcc-riscv64-unknown-elf  picolibc-riscv64-unknown-elf  binutils-riscv64-unknown-elf \
        gcc-arm-none-eabi        libnewlib-arm-none-eabi      binutils-arm-none-eabi \
        gcc-riscv64-linux-gnu    binutils-riscv64-linux-gnu \
        device-tree-compiler make gdb-multiarch \
        wget curl ca-certificates sudo nano less file unzip xz-utils git telnet \
        xvfb x11vnc fluxbox novnc websockify xterm x11-utils \
 && rm -rf /var/lib/apt/lists/*

# GitHub CLI (not in the Ubuntu 22.04 archive). Students need it to
# `gh repo fork --remote --remote-name=origin` from inside the codespace
# so their edits can be pushed back to their own fork.
RUN mkdir -p -m 755 /etc/apt/keyrings \
 && wget -nv -O- https://cli.github.com/packages/githubcli-archive-keyring.gpg \
    | tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null \
 && chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg \
 && echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
    > /etc/apt/sources.list.d/github-cli.list \
 && apt-get update && apt-get install -y --no-install-recommends gh \
 && rm -rf /var/lib/apt/lists/*

# Renode STABLE (portable tarball; works on amd64 and arm64).
# This is the default `renode` / `renode-test` and drives labs 00-12.
# Kept on a pinned stable release for known-good, reproducible behaviour.
ARG RENODE_VERSION=1.15.3
RUN ARCH="$(dpkg --print-architecture)" && \
    case "$ARCH" in \
      amd64) RFILE="renode-${RENODE_VERSION}.linux-portable.tar.gz" ;; \
      arm64) RFILE="renode-${RENODE_VERSION}.linux-portable-arm64.tar.gz" ;; \
      *) echo "Unsupported arch $ARCH"; exit 1 ;; \
    esac && \
    wget -q "https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/${RFILE}" \
         -O /tmp/renode.tgz && \
    mkdir -p /opt/renode && tar -xzf /tmp/renode.tgz -C /opt/renode --strip-components=1 && \
    ln -s /opt/renode/renode /usr/local/bin/renode && \
    if [ -f /opt/renode/renode-test ]; then ln -s /opt/renode/renode-test /usr/local/bin/renode-test; fi && \
    rm /tmp/renode.tgz

# Renode NIGHTLY (side-by-side, ONLY for lab 13 / Coral NPU).
#
# Lab 13 needs the `CPU.CoralNPU` component, which is only present in
# Renode nightly builds (post-1.16.1) - it is NOT in any stable release
# yet. To avoid destabilising labs 00-12, we do NOT touch the stable
# install above; instead we drop the nightly next to it at
# /opt/renode-nightly and expose it as `renode-nightly` /
# `renode-test-nightly`. Only lab 13 opts in to these; every other lab
# keeps using stable `renode`.
#
# amd64 only: builds.renode.io does not publish an arm64 mono-portable
# nightly, so on arm64 hosts (e.g. local Apple Silicon docker) lab 13 is
# unavailable while labs 00-12 still work. Codespaces and the CI image
# are amd64, where lab 13 runs. Pin a known-good dated build; if it has
# been garbage-collected, fall back to the current `latest` nightly.
ARG RENODE_NIGHTLY_VERSION=1.16.1+20260715git78dc7cf91
RUN ARCH="$(dpkg --print-architecture)" && \
    if [ "$ARCH" = "amd64" ]; then \
      ( wget -q "https://builds.renode.io/renode-${RENODE_NIGHTLY_VERSION}.linux-portable.tar.gz" -O /tmp/renode-nightly.tgz \
        || wget -q "https://builds.renode.io/renode-latest.linux-portable.tar.gz" -O /tmp/renode-nightly.tgz ) && \
      mkdir -p /opt/renode-nightly && \
      tar -xzf /tmp/renode-nightly.tgz -C /opt/renode-nightly --strip-components=1 && \
      ln -s /opt/renode-nightly/renode /usr/local/bin/renode-nightly && \
      if [ -f /opt/renode-nightly/renode-test ]; then ln -s /opt/renode-nightly/renode-test /usr/local/bin/renode-test-nightly; fi && \
      rm /tmp/renode-nightly.tgz ; \
    else \
      echo "Renode nightly (Coral NPU / lab 13) is amd64-only; skipping on $ARCH (labs 00-12 unaffected)." ; \
    fi

# Renode's test runner (renode-test / Robot Framework, used in lab 06)
# needs Python deps. Without them `lab 06` fails with
# "No module named 'psutil'". We pin the same versions Renode ships in
# tests/requirements.txt, but install them explicitly instead of via
# `-r tests/requirements.txt` because that file pulls in relative
# `-r ../tools/.../requirements.txt` includes that aren't present in the
# portable tarball (and would break the build). These four cover the
# Robot UART suite in lab 06.
RUN pip3 install --no-cache-dir \
        robotframework==6.1 \
        robotframework-retryfailed==0.2.0 \
        psutil==5.9.3 \
        "pyyaml==6.0.*"

# Prebuilt CoralNPU ISS from stage 1 (lab 13). Only the stripped binary is
# pulled in; the multi-GB build tree stays in the discarded builder stage.
# Exposed as `coralnpu_sim` so tools/run_on_coralnpu_sim.sh finds it directly.
COPY --from=coralnpu-builder /opt/coralnpu /opt/coralnpu
RUN ln -sf /opt/coralnpu/bin/coralnpu_sim /usr/local/bin/coralnpu_sim

# Non-root user with sudo (matches devcontainer.json's remoteUser).
RUN useradd -m -s /bin/bash -G sudo student && \
    echo "student ALL=(ALL) NOPASSWD:ALL" >/etc/sudoers.d/student && \
    mkdir -p /home/student/work && chown -R student:student /home/student

# X11 needs /tmp/.X11-unix to exist with sticky bit BEFORE Xvfb starts.
# Slim Ubuntu images and Codespaces both fail to provide this otherwise:
# Xvfb (running as non-root `student`) cannot create it itself, dies silently,
# and port 6080 (noVNC) ends up unbound -> "HTTP ERROR 502" in the browser.
RUN mkdir -p /tmp/.X11-unix && chmod 1777 /tmp/.X11-unix

# Lab content + helpers (extracted from heredocs into the repo root,
# so they're easy to edit and review without touching the Dockerfile).
COPY --chown=student:student labs/ /labs/
COPY lab            /usr/local/bin/lab
COPY entrypoint.sh  /usr/local/bin/entrypoint.sh
COPY sync-labs.sh   /usr/local/bin/sync-labs.sh
COPY lab.sh         /etc/profile.d/lab.sh
RUN chmod +x /usr/local/bin/lab /usr/local/bin/entrypoint.sh /usr/local/bin/sync-labs.sh && \
    chmod 0644 /etc/profile.d/lab.sh && \
    # Source the lab() wrapper in interactive (non-login) shells too, so
    # the shell lands in ~/work/<lab> after Renode quits. Login shells
    # already get /etc/profile.d/*.sh via /etc/profile.
    printf '\n# Renode labs: define lab() wrapper (cd into ~/work/<lab> after a run)\nif [ -f /etc/profile.d/lab.sh ]; then . /etc/profile.d/lab.sh; fi\n' \
      >> /home/student/.bashrc

USER student
WORKDIR /home/student/work

EXPOSE 6080 5901 1234
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["sleep", "infinity"]
