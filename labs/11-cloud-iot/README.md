# Lab 11 — Cloud IoT: telemetry to AWS IoT Core / Azure IoT Hub (optional)

> **Optional capstone.** Lab 08 built a multi-node network that stopped
> at the **gateway**; labs 09–10 kept intelligence **on the chip**. This
> lab crosses the last boundary in the IoT picture: **off the device,
> through a host gateway, to a real cloud IoT broker**. A simulated
> RISC-V sensor node emits JSON telemetry on its UART; Renode exposes
> that UART on a TCP socket; and a host-side **bridge** reads it, shows a
> live dashboard, and *optionally* publishes each message to **AWS IoT
> Core** or **Azure IoT Hub**.

The one genuinely new idea is the **edge → gateway → cloud split done
for real**. The firmware stays a dumb, deterministic sensor that knows
*nothing* about TCP/IP, MQTT, TLS or credentials — it just prints a line
of JSON, forever. Everything cloud-facing (connectivity, certificates,
retries) lives on the host, in `tools/bridge.py`. That is exactly how
production fleets are built: constrained devices talk a simple local
link; a gateway owns the internet uplink and the secrets.

```
   TIER 1 · EDGE (Renode)          TIER 2 · GATEWAY (host)         TIER 3 · CLOUD
   simulated RISC-V node           tools/bridge.py                 AWS IoT Core /
   prints JSON on its UART         reads socket, forwards          Azure IoT Hub
                                                                   + local dashboard
   [ node ] --UART--> tcp://localhost:3456 --> [ bridge ] ==MQTT/TLS==> [ broker ]
                                                     |
                                                     +--> http://localhost:8000
                                                          (live Chart.js dashboard)
```

- The **device** half runs in Renode and needs zero cloud setup.
- The **bridge** always serves a local dashboard, so the whole lab works
  **offline** with no account. Add `--cloud aws|azure` only when you want
  to push to a real broker.

## 1. Run it

```bash
lab 11
```

This mirrors `/labs/11-cloud-iot/` into `~/work/11-cloud-iot/`, runs
`make` to cross-compile `telemetry.elf`, then launches
`renode/iot-cloud.resc`. That script loads the firmware, exposes the
device UART on **`tcp://localhost:3456`**, and `start`s the node. You'll
see:

```
IoT node streaming JSON telemetry on tcp://localhost:3456
In a SECOND terminal run the bridge, e.g.:
    cd ~/work/11-cloud-iot && python3 tools/bridge.py
```

Now open a **second terminal** and run the bridge:

```bash
cd ~/work/11-cloud-iot
python3 tools/bridge.py            # local dashboard only (no cloud needed)
```

The bridge connects to the socket, prints each telemetry line, and
serves a live chart:

```
[dashboard] live chart at http://localhost:8000
[cloud] no cloud backend (local dashboard only). Add --cloud aws|azure to forward.
[reader] connected to device at localhost:3456
  telemetry device=renode-sim-01 seq=0 temp_c=24.7 humidity=51
  telemetry device=renode-sim-01 seq=1 temp_c=25.3 humidity=49
  ...
```

Open **http://localhost:8000** (in Codespaces, the port is
auto-forwarded — click the browser prompt or the Ports tab) to watch
temperature and humidity update live.

> If port 8000 is already taken (usually a previous `bridge.py` still
> running — `pkill -f tools/bridge.py`), the bridge automatically picks
> the next free port and prints the real URL in its `[dashboard]` line.
> You can also force one with `--dashboard-port <N>` or skip it with
> `--no-dashboard`.

## 2. What just happened

The device and the cloud are deliberately **decoupled by a socket**:

```
   Renode (the "device")                    host (the "gateway")
  +-----------------------+   TCP 3456   +--------------------------+
  |  RV64 core            |  ==========> |  tools/bridge.py         |
  |  UART.NS16550         |  JSON lines  |   * reads JSON lines      |
  |  telemetry.elf  ------+------------->|   * serves dashboard :8000|
  |  prints one JSON line |              |   * (opt) publishes to    |
  |  per period, forever  |              |     AWS IoT / Azure IoT   |
  +-----------------------+              +-----------+--------------+
                                                     | MQTT/TLS (opt)
                                                     v
                                            AWS IoT Core / Azure IoT Hub
```

- **`emulation CreateServerSocketTerminal 3456 "cloudlink" false`** turns
  the device UART into a TCP server. Anything the firmware writes to the
  UART is streamed to whoever connects on port 3456. `false` means "don't
  prepend the terminal-config bytes" — we want raw UART text.
- **`connector Connect sysbus.uart @cloudlink`** wires the NS16550 to
  that socket, exactly like the console analyzer in other labs — only the
  backend is a socket instead of a window.
- The firmware (`src/telemetry.c`) is trivial: synthesize a reading, emit
  one compact JSON line, `delay`, repeat. No networking on the MCU.

The bridge is the **gateway**: it owns the connection, buffers/parses the
JSON, fans it out to a dashboard, and (optionally) authenticates to a
cloud broker with credentials that never touch the firmware.

## 3. Files

| File | What it is |
|---|---|
| `src/telemetry.c` | Single-node firmware: emit one JSON line per period on the UART. |
| `src/fleet.c` | **Fleet variant** firmware (see §6): `#if NODE_ID == 0` → gateway, else sensor. |
| `src/start.S`, `src/link.ld` | RV64 startup + 64 MiB linker script (as lab 03/09). |
| `renode/iot-cloud.repl` | The board: one RV64 core + 64 MiB RAM + one NS16550 UART. |
| `renode/iot-cloud.resc` | Loads `telemetry.elf`, exposes the UART on TCP 3456, starts. |
| `renode/iot-fleet.repl` | Two-UART board for the fleet variant (bus + cloud uplink). |
| `renode/iot-fleet.resc` | Creates a UART hub + gateway + 2 sensors, wires the uplink socket. |
| `Makefile` | `make` builds `telemetry.elf`; `make fleet` builds `gateway/sensor{1,2}.elf`. **Real TABs.** |
| `tools/bridge.py` | The host gateway: socket reader + dashboard + AWS/Azure publishers. |
| `tools/dashboard/index.html` | Live Chart.js dashboard (temp + humidity). |
| `tools/requirements-aws.txt` | `paho-mqtt` (only needed for `--cloud aws`). |
| `tools/requirements-azure.txt` | `azure-iot-device` (only needed for `--cloud azure`). |
| `.env.example` | Template for cloud endpoints/keys. Copy to `.env`; **never commit it.** |
| `.gitignore` | Keeps `.env`, `certs/`, `*.pem/crt/key` and build artifacts out of git. |

## 4. Forward to AWS IoT Core (`--cloud aws`)

AWS IoT Core speaks **MQTT over mutual TLS** on port 8883. You'll need a
(free-tier) "Thing" with an X.509 certificate.

1. **Create a Thing + certificate.** AWS IoT console → *Manage → Things →
   Create* → *Auto-generate a new certificate*. Download the device
   certificate, the private key, and the **Amazon Root CA 1**. Attach a
   policy that allows `iot:Connect`/`iot:Publish`.
2. **Drop the files in `certs/`** (git-ignored) using the default names
   the bridge expects:

   ```
   ~/work/11-cloud-iot/certs/AmazonRootCA1.pem
   ~/work/11-cloud-iot/certs/device.pem.crt
   ~/work/11-cloud-iot/certs/private.pem.key
   ```

3. **Set your endpoint.** Copy `.env.example` to `.env`, fill in
   `AWS_IOT_ENDPOINT` (console → *Settings → Device data endpoint*), then
   load it and run:

   ```bash
   pip install -r tools/requirements-aws.txt
   set -a; . ./.env; set +a          # export the vars for this shell
   python3 tools/bridge.py --cloud aws
   ```

4. **Verify.** In the AWS IoT console → *MQTT test client*, subscribe to
   `renode/telemetry` (or your `AWS_IOT_TOPIC`) and watch the messages
   arrive.

## 5. Forward to Azure IoT Hub (`--cloud azure`)

Azure IoT Hub authenticates each device with a **per-device connection
string** (symmetric key). The bridge uses `azure-iot-device` to connect
over MQTT and publish your telemetry as device-to-cloud messages.

You need three things from Azure, in order: **(A)** an IoT hub, **(B)** a
registered device, **(C)** that device's connection string. Two paths
below — the **Azure CLI** (fastest, copy-paste) or the **portal** (UI).
Everything fits in the **Free (F1)** tier (one free hub per subscription,
8,000 messages/day — plenty for this lab).

### 5a. Azure CLI (recommended)

Use the [Azure Cloud Shell](https://shell.azure.com) (nothing to install)
or a local `az`. Pick globally-unique lowercase names for the hub.

```bash
# One-time: add the IoT extension (auto-installs on first use anyway).
az extension add --upgrade --name azure-iot

# (A) Resource group + IoT hub in the FREE tier.
#     F1 REQUIRES --partition-count 2 (the default of 4 is rejected on free).
az group create --name renode-lab-rg --location eastus
az iot hub create \
    --resource-group renode-lab-rg \
    --name <YOUR-HUB-NAME> \
    --sku F1 --partition-count 2          # ~2-3 min to provision

# (B) Register the device (must match the DeviceId your firmware sends;
#     the lab streams device "renode-sim-01").
az iot hub device-identity create \
    --device-id renode-sim-01 --hub-name <YOUR-HUB-NAME>

# (C) Print the device connection string (this is the secret the bridge needs).
az iot hub device-identity connection-string show \
    --device-id renode-sim-01 --hub-name <YOUR-HUB-NAME> -o tsv
```

The last command prints exactly the value below — copy it verbatim:

```
HostName=<YOUR-HUB-NAME>.azure-devices.net;DeviceId=renode-sim-01;SharedAccessKey=<base64-key>
```

### 5b. Azure portal (UI alternative)

1. **Create the hub.** Portal → **Create a resource** → search **IoT Hub**
   → **Create**. Choose your subscription and resource group, a unique
   **IoT hub name**, and a region. On the **Management** (a.k.a. *Tier*)
   tab set **Pricing and scale tier → Free (F1)**. **Review + create**.
2. **Register the device.** Open the hub → **Device management → Devices**
   → **+ Add device**. Enter **Device ID** `renode-sim-01`, leave
   *Authentication type* = **Symmetric key** with *auto-generate keys*
   checked, **Save**.
3. **Copy the connection string.** **Device management → Devices** → click
   `renode-sim-01` → copy **Primary connection string** (click the eye
   icon to reveal, or the copy button). Same `HostName=…;DeviceId=…;
   SharedAccessKey=…` format as above.

### 5c. Point the bridge at your hub

```bash
pip install -r tools/requirements-azure.txt
export AZURE_IOT_CONNECTION_STRING='HostName=<YOUR-HUB-NAME>.azure-devices.net;DeviceId=renode-sim-01;SharedAccessKey=<base64-key>'
python3 tools/bridge.py --cloud azure
```

On success the bridge prints `[azure] connected to IoT Hub` and forwards
every telemetry line as a device-to-cloud message.

### 5d. Verify it's arriving

Run this in the **[Azure Cloud Shell](https://shell.azure.com)** (the `>_`
icon in the Azure portal, or [shell.azure.com](https://shell.azure.com)) —
it's already signed in to your subscription and auto-installs the
`azure-iot` extension, so no local setup is needed:

```bash
az iot hub monitor-events --output table \
    --device-id renode-sim-01 --hub-name <YOUR-HUB-NAME>
```

You'll see your simulated node's JSON (`temp_c`, `humidity`, `seq`) stream
in. (You can also watch the hub's **Metrics** blade → *Telemetry messages
sent* in the portal.)

> **Where to run `az`.** All the `az ...` commands in this section
> (§5a and §5d) are meant for the **Azure Cloud Shell**, not the
> Codespace — Cloud Shell has the CLI, the `azure-iot` extension, and
> your Azure login already set up. `monitor-events` only *observes* the
> hub, so it can run anywhere with Azure access; it does **not** need to
> be on the same machine as `bridge.py` (which stays in the Codespace).
> To run `az` from the Codespace instead, you'd first install the CLI and
> `az login --use-device-code`.

> **Device ID must match.** The connection string's `DeviceId` has to be
> the device you registered, and Azure rejects messages whose device
> doesn't exist. The single-node firmware sends `renode-sim-01`; for
> `lab 11 fleet` (which sends `renode-sim-01` **and** `renode-sim-02`
> through one gateway), register **both** device IDs, or change the
> sensor IDs in `src/fleet.c` to match what you registered.

> **Clean up to avoid surprises.** Even the free hub counts against your
> one-free-hub-per-subscription limit; delete it when done:
> `az group delete --name renode-lab-rg`.

> **Credentials are never hard-coded or committed.** The bridge reads
> them from env vars / `certs/`, and `.gitignore` blocks `.env`, `certs/`
> and all `*.pem/crt/key` files. Treat a leaked device key like any other
> secret.

## 6. Fleet variant — multi-node → gateway → cloud (`lab 11 fleet`)

This is the direct fusion of **lab 08** (many nodes on a shared bus) and
**lab 11** (a node streaming to the cloud). Instead of one node talking
straight to the socket, several **sensor** nodes broadcast on a shared
UART hub and a **gateway** node relays the merged stream out the cloud
uplink:

```
        UARTHub "bus" (shared broadcast medium)          cloud uplink
   +----------------------------------------+           (TCP 3456)
   |  every byte a node TX's -> others' RX  |
   +----^-----------------^-----------------+
        | uart1           | uart1            | uart1
   +----+-----+     +------+-----+     +------+---------+
   | sensor1  |     | sensor2    |     |  gateway       |  uart0 --> tcp://:3456
   | fleet1   |     | fleet2     |     |  fleet0        |  ==> bridge ==> cloud
   | RV64     |     | RV64       |     |  RV64          |
   +----------+     +------------+     +----------------+
      broadcast JSON on the bus          reads bus, forwards
                                         each line to the uplink
```

Run it (then start the bridge exactly as before):

```bash
lab 11 fleet
# second terminal:
cd ~/work/11-cloud-iot && python3 tools/bridge.py        # or --cloud aws|azure
```

The bridge now shows **two devices** interleaved — the multi-node fleet
converging through one gateway to one cloud connection:

```
  telemetry device=renode-sim-01 seq=0 temp_c=22.3 humidity=52
  telemetry device=renode-sim-02 seq=0 temp_c=24.8 humidity=47
  telemetry device=renode-sim-01 seq=1 temp_c=22.9 humidity=50
  ...
```

**How it maps to the code.** One source, two roles, chosen at build time
by `-DNODE_ID` (exactly the lab 08 pattern):

| Role | `NODE_ID` | UART use |
|---|---|---|
| Gateway | `0` | reads `uart1` (bus), relays each JSON line to `uart0` (cloud uplink socket) |
| Sensor | `1`, `2`, … | broadcasts JSON telemetry on `uart1` (bus); `uart0` is a local console |

`renode/iot-fleet.resc` creates the `UARTHub`, boots the three machines,
connects every node's `uart1` to the hub, and connects **only the
gateway's `uart0`** to the socket terminal on 3456. The bridge and cloud
side are unchanged — from their point of view, one gateway is publishing
telemetry for a whole fleet.

> The dashboard is **per-device**: it groups samples by the `device`
> field and draws a separate temperature line (solid, left axis) and
> humidity line (dashed, right axis) for each sensor, plus a live summary
> chip per device. So `lab 11 fleet` shows `renode-sim-01` and
> `renode-sim-02` as distinct series, while `lab 11` shows just one.

## 7. Useful monitor commands

Single-node (`lab 11`):

| Command | What it does |
|---|---|
| `peripherals` | The whole "board": `cpu`, `ram`, `uart`. |
| `sysbus.cpu PC` | Where the CPU is executing. |
| `sysbus.cpu ExecutedInstructions` | Rough cost of the telemetry loop. |
| `pause` / `start` | Freeze / resume the device (the bridge auto-reconnects). |
| `machine Reset` then `start` | Restart the stream from `seq=0`. |
| `quit` | Exit Renode (the bridge keeps waiting; `Ctrl-C` it too). |

Fleet (`lab 11 fleet`) adds `mach` navigation (as in lab 08):

| Command | What it does |
|---|---|
| `mach` | List machines: `gateway`, `sensor1`, `sensor2`. |
| `mach set "sensor1"` | Make a sensor the active machine. |
| `emulation` | Show all machines plus the `bus` hub. |
| `mach set "sensor2"; connector Disconnect sysbus.uart1 bus` | Cut a sensor off the bus — its device vanishes from the cloud stream. |

## 8. Mini-experiments (try at least one)

1. **Go cloud.** Do §4 or §5 and watch your simulated node's telemetry
   appear in the AWS MQTT test client / Azure event monitor — real device
   data from a chip that doesn't exist.

2. **Change the payload.** Add a field (e.g. `"battery_mv"`) to the JSON
   in `src/telemetry.c`, re-run `lab 11`. It flows to the dashboard and
   cloud with **zero** changes to the bridge — JSON is the contract.

3. **Change the cadence.** Lower the `delay()` bound in `telemetry.c` for
   a faster stream, or raise it to model a low-power sensor that reports
   once a minute.

4. **Pull the plug.** `pause` the device in the Renode monitor; the
   bridge prints `waiting for device …` and reconnects when you `start`
   again — the buffering/reconnect behaviour every real gateway needs.

5. **Spot a dead node.** Run `lab 11 fleet`, then in the Renode monitor
   `mach set "sensor2"; connector Disconnect sysbus.uart1 bus`. That
   sensor's dashboard chip stops advancing and its lines flatten while
   `sensor1` keeps streaming — exactly how a fleet operator notices a node
   has dropped off. Reconnect with `connector Connect sysbus.uart1 bus`.

6. **Add a third sensor.** Copy a `sensor2` block in
   `renode/iot-fleet.resc` → `sensor3`, add a `sensor3.elf` rule in the
   `Makefile` (`fleet3.o`, `NODE_ID=3`), re-run `lab 11 fleet`. A third
   device appears in the cloud stream — the fleet scales with two edits.

## 9. Stopping cleanly

The device loops forever, so in the Renode terminal:

```text
quit
```

…and `Ctrl-C` the bridge in the other terminal.

## What this lab proves

- The **edge → gateway → cloud** split is a *decoupling*: the device
  speaks a dead-simple local protocol (JSON on a UART) and a gateway owns
  connectivity, credentials, buffering and retries. Renode models the
  device; the host bridge models the gateway; the broker is real.
- Renode's **socket terminals** turn any modelled UART into a host TCP
  stream, so simulated firmware integrates with real host tooling
  (dashboards, MQTT clients, cloud SDKs) exactly like hardware would.
- Credentials belong on the **gateway**, never in firmware — the same
  security boundary real deployments rely on.
- The pattern **composes**: bolt lab 08's multi-node bus onto the front
  (`lab 11 fleet`) and a whole fleet reaches the cloud through one
  gateway, with no change to the cloud side — the shape of a real IoT
  deployment, entirely in simulation.
