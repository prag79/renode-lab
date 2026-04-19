# syntax=docker/dockerfile:1.7
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:1 \
    LAB_HOME=/labs

# Renode runtime + cross toolchains + headless desktop bits.
# IMPORTANT: never put `# comments` mid-line inside a `\`-continued RUN.
# A `#` consumes the trailing `\` and silently breaks line continuation;
# the next "line" then runs as a separate shell command.
RUN apt-get update && apt-get install -y --no-install-recommends \
        mono-complete libgtk2.0-0 screen policykit-1 \
        gcc-riscv64-unknown-elf  picolibc-riscv64-unknown-elf  binutils-riscv64-unknown-elf \
        gcc-arm-none-eabi        libnewlib-arm-none-eabi      binutils-arm-none-eabi \
        gcc-riscv64-linux-gnu    binutils-riscv64-linux-gnu \
        device-tree-compiler make gdb-multiarch \
        wget curl ca-certificates sudo nano less file unzip xz-utils git telnet \
        xvfb x11vnc fluxbox novnc websockify \
 && rm -rf /var/lib/apt/lists/*

# Renode (portable tarball; works on amd64 and arm64).
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
    rm /tmp/renode.tgz

# Non-root user with sudo (matches devcontainer.json's remoteUser).
RUN useradd -m -s /bin/bash -G sudo student && \
    echo "student ALL=(ALL) NOPASSWD:ALL" >/etc/sudoers.d/student && \
    mkdir -p /home/student/work && chown -R student:student /home/student

# Lab content + helpers (extracted from heredocs into the repo root,
# so they're easy to edit and review without touching the Dockerfile).
COPY --chown=student:student labs/ /labs/
COPY lab            /usr/local/bin/lab
COPY entrypoint.sh  /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/lab /usr/local/bin/entrypoint.sh

USER student
WORKDIR /home/student/work

EXPOSE 6080 5901 1234
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["sleep", "infinity"]
