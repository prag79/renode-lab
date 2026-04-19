#!/usr/bin/env bash
# /usr/local/bin/entrypoint.sh - bring up the noVNC desktop on port 6080
# so Renode's GUI analyzers are visible in the browser.
#
# Idempotent: safe to run multiple times. Used as both the Docker
# ENTRYPOINT (for local `docker run` testing) and as the Codespaces
# `postStartCommand` (because Codespaces does not always invoke the
# Dockerfile ENTRYPOINT).

set -e

start_if_missing() {
  local name="$1"; shift
  if ! pgrep -f "$name" >/dev/null; then
    "$@" >/dev/null 2>&1 &
  fi
}

if [[ "${LAB_GUI:-0}" == "1" ]]; then
  # Xvfb needs /tmp/.X11-unix with sticky bit; create it if the
  # base image or a tmpfs reset removed it. Use sudo because student
  # cannot chmod a root-owned dir.
  if [[ ! -d /tmp/.X11-unix ]]; then
    sudo mkdir -p /tmp/.X11-unix && sudo chmod 1777 /tmp/.X11-unix
  fi

  start_if_missing "Xvfb :1"        Xvfb :1 -screen 0 1280x800x24
  sleep 0.5
  start_if_missing "fluxbox"        fluxbox
  start_if_missing "x11vnc -display :1"  x11vnc -display :1 -forever -shared -rfbport 5901 -nopw -quiet
  start_if_missing "websockify"     websockify --web=/usr/share/novnc 6080 localhost:5901
  echo "Desktop ready: open the auto-forwarded port 6080 (path /vnc.html)"
fi

# When invoked as the Docker ENTRYPOINT, exec the container CMD.
# When invoked as a Codespaces postStartCommand with `true`, exits cleanly.
exec "$@"
