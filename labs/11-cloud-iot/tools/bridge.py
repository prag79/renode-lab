#!/usr/bin/env python3
"""Cloud IoT bridge for the Renode simulated sensor node (lab 11).

Reads newline-delimited JSON telemetry from the device UART (exposed by
Renode on a TCP socket, default tcp://localhost:3456), and:

  * always serves a live LOCAL dashboard (Chart.js) on
    http://localhost:8000  -- works with zero cloud setup, and
  * optionally forwards each message to a real cloud IoT broker:
        --cloud aws     AWS IoT Core   (MQTT over mutual TLS, port 8883)
        --cloud azure   Azure IoT Hub  (azure-iot-device SDK)

This is the "gateway" half of an edge -> gateway -> cloud pipeline: the
device stays a dumb, deterministic sensor; connectivity + credentials
live here on the host, out of the firmware.

Credentials are NEVER hard-coded. AWS uses X.509 certs + an endpoint
from env/flags; Azure uses a device connection string from the
AZURE_IOT_CONNECTION_STRING env var. See README section 4/5 and
.env.example. `certs/` and `.env` are git-ignored.

Examples:
    python3 tools/bridge.py                      # local dashboard only
    python3 tools/bridge.py --cloud aws          # + AWS IoT Core
    python3 tools/bridge.py --cloud azure        # + Azure IoT Hub
"""
import argparse
import errno
import json
import os
import socket
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
DASH_DIR = os.path.join(HERE, "dashboard")

# Shared rolling buffer of recent samples for the dashboard.
_samples = deque(maxlen=500)
_lock = threading.Lock()


# --------------------------------------------------------------------------
# Local dashboard (always on)
# --------------------------------------------------------------------------
class DashboardHandler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass  # keep the console clean; telemetry is printed by the reader

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self._send_file(os.path.join(DASH_DIR, "index.html"), "text/html")
        elif self.path.startswith("/data.json"):
            with _lock:
                body = json.dumps(list(_samples)).encode()
            self._send_bytes(body, "application/json")
        else:
            self.send_error(404)

    def _send_file(self, path, ctype):
        try:
            with open(path, "rb") as f:
                self._send_bytes(f.read(), ctype)
        except OSError:
            self.send_error(404)

    def _send_bytes(self, body, ctype):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)


def start_dashboard(port, attempts=10):
    """Serve the dashboard, auto-falling forward if the port is taken.

    A leftover bridge (or anything else) holding the port raises
    EADDRINUSE. Rather than crash the whole tool, try the next few ports,
    and if none are free, warn and carry on WITHOUT the dashboard so the
    telemetry reader + cloud forwarding still run.
    """
    for p in range(port, port + attempts):
        try:
            srv = ThreadingHTTPServer(("0.0.0.0", p), DashboardHandler)
        except OSError as e:
            if e.errno == errno.EADDRINUSE:
                continue
            raise
        t = threading.Thread(target=srv.serve_forever, daemon=True)
        t.start()
        if p != port:
            print(f"[dashboard] port {port} busy; using {p} instead")
        print(f"[dashboard] live chart at http://localhost:{p}")
        return srv

    print(f"[dashboard] ports {port}-{port + attempts - 1} all in use "
          f"(a previous bridge may still be running: 'pkill -f tools/bridge.py'). "
          f"Continuing without the dashboard.")
    return None


# --------------------------------------------------------------------------
# Cloud publishers (optional; imported lazily so the deps are only needed
# for the backend you actually use)
# --------------------------------------------------------------------------
class AwsPublisher:
    """AWS IoT Core via MQTT over mutual TLS (paho-mqtt)."""

    def __init__(self):
        try:
            import paho.mqtt.client as mqtt
        except ImportError:
            sys.exit("AWS backend needs paho-mqtt: pip install -r "
                     "tools/requirements-aws.txt")
        self.topic = os.environ.get("AWS_IOT_TOPIC", "renode/telemetry")
        endpoint = os.environ.get("AWS_IOT_ENDPOINT")
        if not endpoint:
            sys.exit("Set AWS_IOT_ENDPOINT (see README section 4).")
        certs = os.path.join(os.path.dirname(HERE), "certs")
        ca = os.environ.get("AWS_IOT_CA", os.path.join(certs, "AmazonRootCA1.pem"))
        cert = os.environ.get("AWS_IOT_CERT", os.path.join(certs, "device.pem.crt"))
        key = os.environ.get("AWS_IOT_KEY", os.path.join(certs, "private.pem.key"))
        for f in (ca, cert, key):
            if not os.path.isfile(f):
                sys.exit(f"Missing credential file: {f} (see README section 4).")
        self.client = mqtt.Client(client_id=os.environ.get("AWS_IOT_CLIENT_ID",
                                                            "renode-sim-01"))
        self.client.tls_set(ca_certs=ca, certfile=cert, keyfile=key)
        self.client.connect(endpoint, 8883, keepalive=60)
        self.client.loop_start()
        print(f"[aws] connected to {endpoint}, publishing to '{self.topic}'")

    def publish(self, msg: dict):
        self.client.publish(self.topic, json.dumps(msg), qos=0)


class AzurePublisher:
    """Azure IoT Hub via the device SDK + a connection string."""

    def __init__(self):
        try:
            from azure.iot.device import IoTHubDeviceClient
        except ImportError:
            sys.exit("Azure backend needs azure-iot-device: pip install -r "
                     "tools/requirements-azure.txt")
        cs = os.environ.get("AZURE_IOT_CONNECTION_STRING")
        if not cs:
            sys.exit("Set AZURE_IOT_CONNECTION_STRING (see README section 5).")
        from azure.iot.device import Message  # noqa: F401  (used in publish)
        self._Message = Message
        self.client = IoTHubDeviceClient.create_from_connection_string(cs)
        self.client.connect()
        print("[azure] connected to IoT Hub")

    def publish(self, msg: dict):
        m = self._Message(json.dumps(msg))
        m.content_type = "application/json"
        m.content_encoding = "utf-8"
        self.client.send_message(m)


def make_publisher(kind):
    if kind == "aws":
        return AwsPublisher()
    if kind == "azure":
        return AzurePublisher()
    return None


# --------------------------------------------------------------------------
# Telemetry reader
# --------------------------------------------------------------------------
def read_stream(host, port, publisher):
    """Connect to the Renode UART socket and process JSON lines forever."""
    while True:
        try:
            sock = socket.create_connection((host, port), timeout=5)
        except OSError:
            print(f"[reader] waiting for device at {host}:{port} ...")
            time.sleep(1.0)
            continue
        print(f"[reader] connected to device at {host}:{port}")
        buf = b""
        try:
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    raise ConnectionError("device closed the connection")
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    handle_line(line, publisher)
        except (OSError, ConnectionError) as e:
            print(f"[reader] lost device connection ({e}); retrying")
            try:
                sock.close()
            except OSError:
                pass
            time.sleep(1.0)


def handle_line(raw, publisher):
    text = raw.decode("utf-8", "replace").strip().rstrip("\r")
    if not text or not text.startswith("{"):
        return  # skip banners / partial noise
    try:
        msg = json.loads(text)
    except json.JSONDecodeError:
        return
    msg["ts"] = int(time.time() * 1000)
    with _lock:
        _samples.append(msg)
    seq = msg.get("seq", "?")
    dev = msg.get("device", "?")
    print(f"  telemetry device={dev} seq={seq} temp_c={msg.get('temp_c')} "
          f"humidity={msg.get('humidity')}")
    if publisher is not None:
        try:
            publisher.publish(msg)
        except Exception as e:  # keep bridging even if one publish fails
            print(f"  [cloud] publish failed: {e}")


def main():
    ap = argparse.ArgumentParser(description="Renode -> cloud IoT bridge")
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--port", type=int, default=3456,
                    help="Renode UART socket port (default 3456)")
    ap.add_argument("--cloud", choices=["none", "aws", "azure"], default="none",
                    help="forward telemetry to this cloud (default: none)")
    ap.add_argument("--dashboard-port", type=int, default=8000)
    ap.add_argument("--no-dashboard", action="store_true")
    args = ap.parse_args()

    # Line-buffer stdout so telemetry shows live even when piped/tee'd.
    try:
        sys.stdout.reconfigure(line_buffering=True)
    except (AttributeError, ValueError):
        pass

    if not args.no_dashboard:
        start_dashboard(args.dashboard_port)

    publisher = make_publisher(args.cloud)
    if publisher is None:
        print("[cloud] no cloud backend (local dashboard only). "
              "Add --cloud aws|azure to forward.")

    try:
        read_stream(args.host, args.port, publisher)
    except KeyboardInterrupt:
        print("\n[bridge] stopped.")


if __name__ == "__main__":
    main()
