#!/usr/bin/env bash
# /usr/local/bin/entrypoint.sh - conditionally bring up a noVNC desktop
# on port 6080 so Renode's GUI analyzers are visible in the browser.

set -e

if [[ "${LAB_GUI:-0}" == "1" ]]; then
  Xvfb :1 -screen 0 1280x800x24 &
  sleep 0.5
  fluxbox >/dev/null 2>&1 &
  x11vnc -display :1 -forever -shared -rfbport 5901 -nopw -quiet >/dev/null 2>&1 &
  websockify --web=/usr/share/novnc 6080 localhost:5901 >/dev/null 2>&1 &
  echo "Desktop ready: open the auto-forwarded port 6080"
fi

exec "$@"
