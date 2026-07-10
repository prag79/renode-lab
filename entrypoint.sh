#!/usr/bin/env bash
# /usr/local/bin/entrypoint.sh - bring up the noVNC desktop on port 6080
# so Renode's GUI analyzers are visible in the browser.
#
# Idempotent: safe to run multiple times. Used as both the Docker
# ENTRYPOINT (for local `docker run` testing) and as the Codespaces
# `postStartCommand` (because Codespaces does not always invoke the
# Dockerfile ENTRYPOINT).

set -e

LOG_DIR="${LAB_GUI_LOG_DIR:-/tmp}"

# Background a service if it isn't already running. Use `nohup` +
# `setsid` so the child survives even when the postStartCommand
# shell exits (Codespaces sometimes reaps non-detached children
# of postStartCommand). Logs go to $LOG_DIR for debugging instead
# of /dev/null, since we hit "everything died silently" twice.
start_if_missing() {
  local name="$1"; shift
  local log="$LOG_DIR/${name// /_}.log"
  if ! pgrep -f "$name" >/dev/null; then
    nohup setsid "$@" >"$log" 2>&1 < /dev/null &
    disown || true
  fi
}

# Block until X display :1 is actually accepting connections.
# A fixed `sleep 0.5` is not enough on Codespaces -- fluxbox /
# x11vnc race Xvfb, fail to connect, and exit immediately.
wait_for_display() {
  local socket="/tmp/.X11-unix/X${1#:}"
  for _ in $(seq 1 100); do
    if [[ -S "$socket" ]] && DISPLAY="$1" xdpyinfo >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done
  echo "entrypoint: timed out waiting for X display $1" >&2
  return 1
}

if [[ "${LAB_GUI:-0}" == "1" ]]; then
  # Xvfb needs /tmp/.X11-unix with sticky bit; create it if the
  # base image or a tmpfs reset removed it. Use sudo because student
  # cannot chmod a root-owned dir.
  if [[ ! -d /tmp/.X11-unix ]]; then
    sudo mkdir -p /tmp/.X11-unix && sudo chmod 1777 /tmp/.X11-unix
  fi

  start_if_missing "Xvfb :1"             Xvfb :1 -screen 0 1280x800x24

  wait_for_display ":1" || true
  start_if_missing "fluxbox"             fluxbox

  # x11vnc flags for a stable Codespaces tunnel:
  #   -forever -shared  survive client disconnects / allow reconnects
  #   -noxdamage        poll the framebuffer instead of relying on the
  #                     XDAMAGE extension, which under Xvfb can miss or
  #                     mis-report updates and wedge the session
  #   -wait/-defer 50   batch framebuffer updates (~20 fps cap) so a busy
  #                     analyzer can't flood the VNC pipe and stall it
  # (-forever already keeps the server alive across client disconnects,
  #  and with no -timeout x11vnc waits indefinitely for the first client.)
  start_if_missing "x11vnc -display :1"  x11vnc -display :1 -forever -shared \
      -noxdamage -wait 50 -defer 50 -rfbport 5901 -nopw -quiet

  # --heartbeat=30 makes websockify send a WebSocket ping every 30 s.
  # Without it, GitHub Codespaces' port-forwarding proxy treats a quiet
  # tunnel as idle and closes it, so noVNC "loses the connection" even
  # though x11vnc/Xvfb are perfectly healthy. The ping keeps it warm.
  start_if_missing "websockify"          websockify --web=/usr/share/novnc \
      --heartbeat=30 6080 localhost:5901

  echo "Desktop ready: open the auto-forwarded port 6080 (path /vnc.html)"
  echo "Tip: for auto-reconnect use  /vnc.html?autoconnect=1&reconnect=1&reconnect_delay=2000"
  echo "Service logs: $LOG_DIR/{Xvfb_:1,fluxbox,x11vnc_-display_:1,websockify}.log"
fi

# When invoked as the Docker ENTRYPOINT, exec the container CMD.
# When invoked as a Codespaces postStartCommand with `true`, exits cleanly.
exec "$@"
