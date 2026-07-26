# Lab 08 — Multi-node IoT network (optional)

> Every lab so far ran a *single* machine. Real
> IoT systems are **many** small nodes talking over a shared medium —
> a mesh of sensors reporting to a gateway. This lab boots **three**
> SiFive FE310 machines *in one Renode emulation*, wires their radios
> onto a shared bus, and watches sensor traffic converge on a gateway.
> No new hardware, no external firmware images — it reuses the RV32
> bare-metal toolchain and SiFive UART from labs 04/05.

The one genuinely new idea is **machine-to-machine communication**:
Renode can host any number of machines in a single emulation and
connect their peripherals with a `connector`. Here that connector is a
**UART hub** — a shared broadcast medium that relays every byte one
node sends to the receive side of all the others. That is exactly the
shape of a multi-drop bus (RS-485) or a single shared radio channel.

> **Note — other media.** The UART hub is the simplest of several
> inter-machine media. Renode also has an Ethernet `Switch`, a `CANHub`,
> and wireless media (`CreateWirelessMedium`, `CreateIEEE802_15_4Medium`,
> `CreateBLEMedium`) for Zigbee/Thread/BLE meshes with real positioning
> and range models. They all use the same `connector Connect …` pattern
> — only the medium and the peripheral (`radio`, `ethernet`, `can`)
> change. We use the UART hub here because it needs no external firmware
> and reuses the FE310 you already know. See §7.

## A 5-minute IoT primer (what this models in the real world)

"IoT" (the Internet of Things) is, at heart, **lots of small computers
sensing or acting on the physical world and shipping that data
somewhere useful**. Almost every deployment — smart buildings,
agriculture, industrial monitoring, asset tracking — is built in three
tiers:

Read it left to right — data flows from the edge, up through the
gateway, to the cloud; commands flow back the other way.

```
   TIER 1 · EDGE NODES          TIER 2 · GATEWAY          TIER 3 · CLOUD / BACKEND
   tiny MCUs, each with a        collects all nodes,       servers, storage, apps
   sensor or actuator           bridges to the internet

   [ sensor 1 ] --->|
                    |
   [ sensor 2 ] --->|--->  [ GATEWAY ]  ===>  [ Cloud IoT platform ]
                    |           |             ( MQTT broker · database ·
   [ actuator ] --->|           |               dashboards · analytics/ML )
                    |           |                         |
                    |           +--- commands/config <----+
                    |
        shared local link  (one radio channel or multi-drop bus:
                            BLE / Zigbee / Thread / LoRa / RS-485)
```

- The **`--->|`** on the left is each node tapping the **one shared
  link** (the vertical bar). That bar is what this lab's UART hub stands
  in for.
- The **gateway** is the only box touching *both* the local link and
  the **internet uplink** (`===>`) — that is its whole reason to exist.
- The **`<----`** return path shows commands/config travelling back down
  the same chain to the actuators (closing the control loop).

**Tier 1 — edge nodes.** Cheap, low-power microcontrollers (like the
FE310 in this lab) attached to a sensor or actuator. They are
*constrained*: little RAM, no OS or a tiny RTOS, often battery-powered,
and they speak a **short-range, low-power** link — not the internet
directly. In this lab, `sensor1` and `sensor2` are exactly this: they
wake up, read a value, broadcast it, and go back to sleep.

**Tier 2 — the gateway.** The crucial middle box. Edge radios (Zigbee,
Thread, BLE, LoRa, RS-485) **cannot reach the internet on their own** —
different physical layer, no IP stack, no route. The gateway is the
node that has *both* worlds: it sits on the local sensor network **and**
has an internet uplink (Wi-Fi, Ethernet, or cellular). Its jobs:

- **Aggregate / fan-in** — collect from many nodes onto one link.
- **Translate protocols** — e.g. 802.15.4 frames ⇄ MQTT-over-TCP/IP.
- **Buffer & batch** — hold readings when the uplink is down, forward
  when it returns.
- **Filter / pre-process** — drop noise, compute averages, run small
  ML at the "edge" so the cloud sees less traffic.
- **Secure the boundary** — authenticate nodes, encrypt the uplink, be
  the one hardened device exposed to the internet.

In this lab, `gateway` (node 0) is that box: every sensor report
converges on its `uart1`, and it prints a unified feed. The one piece
we *don't* model is the uplink — see below.

**Tier 3 — the cloud / backend.** Where the gateway eventually connects.
Typically a managed **IoT platform** (AWS IoT Core, Azure IoT Hub,
Google Cloud IoT, or a self-hosted MQTT broker like Mosquitto). The
gateway publishes readings there — usually via **MQTT**, sometimes CoAP
or HTTP — and the platform ingests them into a time-series database,
drives dashboards and alerts, feeds analytics/ML and "digital twin"
models, and can send **commands back down** (turn on a valve, change a
sampling rate) through the same gateway to the actuators. That
downward path closes the control loop.

**How this lab maps to the picture:**

| Real-world piece | In this lab |
|---|---|
| Edge sensor nodes | `sensor1`, `sensor2` (broadcast fake temperature) |
| Shared low-power link | the `UARTHub` "bus" (stands in for a radio channel) |
| Gateway aggregator | `gateway` (node 0) collecting every report |
| Internet uplink → cloud | **not modelled** — the natural next step |

To take the last step *in Renode*, you'd give the gateway a second
"uplink" interface and bridge it to the host: attach a socket terminal
(`emulation CreateServerSocketTerminal 3333 "uplink"` →
`connector Connect sysbus.uart0 uplink`) and point a real MQTT client
on your machine at it, or add an Ethernet model and a `CreateTap` to
put the gateway on the host network. Then the gateway is doing exactly
what a shipping IoT gateway does: local sensors in one side, cloud out
the other.

## 1. Run it

```bash
lab 08
```

This mirrors `/labs/08-multi-node-iot/` into `~/work/08-multi-node-iot/`,
runs `make` to build **three** firmware images (`node0.elf`,
`node1.elf`, `node2.elf` — same C source, different `-DNODE_ID`), then
launches `renode/iot-net.resc`. That script:

1. Creates a shared bus: `emulation CreateUARTHub "bus"`.
2. Creates three machines (`gateway`, `sensor1`, `sensor2`), loads the
   matching ELF into each, and connects each node's **`uart1`** to the
   hub with `connector Connect sysbus.uart1 bus`.
3. Opens each node's **`uart0`** console analyzer and `start`s the whole
   emulation (which also starts the hub).

In **GUI mode** (`LAB_GUI=1`, noVNC tab on port 6080, path `/vnc.html`)
three analyzer windows appear. The two sensors narrate their sends:

```
*** IoT sensor node 1 online ***
sent t=21
sent t=22
...
```

and the **gateway** window shows the reports arriving over the bus:

```
*** IoT gateway (node 0) online ***
Listening on the shared bus for sensor reports...
[gateway] report: node1 t=21 seq=0
[gateway] report: node2 t=22 seq=0
[gateway] report: node1 t=22 seq=1
[gateway] report: node2 t=23 seq=1
...
```

That interleaving of `node1` and `node2` lines in one gateway window
**is** the multi-node system working: two independent CPUs, each on its
own RAM, sharing one medium, collected by a third.

The gateway polls its network UART (`uart1`) while waiting for sensor
traffic. Renode's SiFive UART model warns on empty RX reads, so the
script intentionally raises only `gateway`'s `uart1` log threshold to
ERROR. That hides expected polling noise like `Trying to read data from
empty receive fifo` while keeping the `uart0` console analyzers visible.

If the noVNC tab is unavailable, jump to §4 to read each console from
the monitor with file backends.

## 2. What just happened

The three machines are wired to one shared bus like this:

```
                        UARTHub  "bus"   (shared broadcast medium)
      +--------------------------------------------------------------+
      |   a byte TX'd by any node is delivered to every OTHER node    |
      +--------^-------------------^-------------------^--------------+
               | uart1             | uart1             | uart1
               | 0x10023000        | 0x10023000        | 0x10023000
      +--------+--------+  +--------+--------+  +-------+---------+
      |    gateway      |  |    sensor1      |  |    sensor2      |
      |   node0.elf     |  |   node1.elf     |  |   node2.elf     |
      |  FE310 (RV32)   |  |  FE310 (RV32)   |  |  FE310 (RV32)   |
      |   RX  <-------  |  |  ------->  TX   |  |  ------->  TX   |
      +--------+--------+  +--------+--------+  +-------+---------+
               | uart0             | uart0             | uart0
               v                   v                   v
          console             console             console
        (own analyzer)      (own analyzer)      (own analyzer)

  data flow on the bus:  sensor1.uart1 --.
                                         +--> bus --> gateway.uart1 (RX)
                         sensor2.uart1 --'
```

Each node keeps its `uart0` as a private console (its own analyzer
window / log), and shares only `uart1` on the hub. The hub is a
broadcast medium and does **not** echo to the sender, so the two
sensors' transmissions both land on the gateway's `uart1` RX — and the
sensors never hear each other.

Three copies of the *same* platform (`renode/iot-node.repl`) run as
three isolated machines. Each is a real FE310 memory map with **two**
SiFive UARTs at their datasheet addresses:

```
uart0: UART.SiFive_UART @ sysbus 0x10013000   // local console
uart1: UART.SiFive_UART @ sysbus 0x10023000   // shared network bus
```

The only thing linking the machines is the hub:

```
emulation CreateUARTHub "bus"
...
connector Connect sysbus.uart1 bus     // run once per machine
```

A UART hub is **broadcast** and does not echo to the sender: a byte a
sensor writes to its `uart1 TXDATA` lands in the `uart1 RXDATA` of every
*other* connected node. The gateway firmware polls its `uart1`, buffers
bytes until a newline, and reprints each complete report to its own
console. The sensors never listen — they just transmit, like fire-and-
forget telemetry beacons.

**One firmware, three roles.** `src/iot.c` branches on `NODE_ID`
(baked in by the Makefile): id `0` is the gateway (RX + print), any
other id is a sensor (periodic broadcast). This mirrors real fleets,
where identical firmware is flashed to every node and behaviour is
chosen by a compile-time or provisioned identity.

## 3. Files

| File | What it is |
|---|---|
| `src/iot.c` | The node firmware. `#if NODE_ID == 0` → gateway, else sensor. |
| `src/start.S`, `src/link.ld` | RV32 startup + 16 KiB linker script (as lab 04). |
| `Makefile` | Builds `node0/1/2.elf` from one source via `-DNODE_ID`. **Real TABs.** |
| `renode/iot-node.repl` | A single FE310 node: CPU + RAM + 2 UARTs + CLINT/PLIC. |
| `renode/iot-net.resc` | Creates the hub, the three machines, wires and starts them. |

## 4. Read the consoles headless (no GUI needed)

Each machine has its own `uart0`. `Ctrl-C` once to get the monitor
prompt, then attach a file backend per node (note the `mach set` to
switch the active machine):

```text
pause
mach set "gateway"
sysbus.uart0 CreateFileBackend @/tmp/gateway.log true
mach set "sensor1"
sysbus.uart0 CreateFileBackend @/tmp/sensor1.log true
machine Reset
sysbus LoadELF @node1.elf
mach set "gateway"
machine Reset
sysbus LoadELF @node0.elf
start
```

In another terminal:

```bash
tail -f /tmp/gateway.log      # reports converging from both sensors
```

(`CreateFileBackend` only captures bytes written *after* it attaches,
which is why we reset + reload before `start`.)

## 5. Useful monitor commands to try

The multi-machine world adds `mach` navigation on top of the usual
single-machine commands.

| Command | What it does |
|---|---|
| `mach` | List all machines in the emulation (gateway, sensor1, sensor2). |
| `mach set "sensor1"` | Make `sensor1` the active machine for later commands. |
| `peripherals` | Block list of the *active* machine (cpu, uart0, uart1, …). |
| `emulation` | Show the whole emulation: every machine plus the `bus` hub. |
| `showAnalyzer sysbus.uart0` | Open the active node's console window. |
| `sysbus ReadDoubleWord 0x10023004` | Read `uart1 RXDATA` on the active node. |
| `sysbus.cpu PC` | Program counter of the active node's CPU (RV32). |
| `emulation RunFor "0.5"` | Advance **all** machines 0.5 s of shared sim-time. |
| `bus Pause` / `bus Resume` | Freeze / thaw the shared bus without stopping the CPUs. |
| `pause` / `start` | Freeze / resume the entire emulation. |
| `quit` | Exit Renode. |

## 6. Mini-experiments (try at least one)

1. **Add a fourth node.** Copy a machine block in
   `renode/iot-net.resc` (`mach create "sensor3"` …
   `connector Connect sysbus.uart1 bus`), add `3` to `NODES` in the
   `Makefile`, and re-run `lab 08`. `node3` reports now appear at the
   gateway — the fleet scales with two edits.

2. **Cut a node off the bus.** At the monitor:

   ```text
   mach set "sensor2"
   connector Disconnect sysbus.uart1 bus
   ```

   `node2` reports stop reaching the gateway (its console still logs
   `sent …`, but nobody hears it) — a vivid demo of what the connector
   actually does. Reconnect with `connector Connect sysbus.uart1 bus`.

3. **Provoke a collision.** Remove the `delay(2000000U * NODE_ID);`
   stagger in `src/iot.c` so both sensors transmit in lock-step. Re-run
   and watch some gateway lines arrive **garbled** (bytes interleaved on
   the shared medium) — the real reason multi-drop buses need arbitration
   or a MAC layer.

4. **Change the report cadence or payload.** Edit the sensor `delay()`
   or the `temp` formula in `src/iot.c`, re-run `lab 08`, and the
   gateway output follows.

5. **Make the gateway a repeater.** Have the gateway echo each received
   report back onto `uart1` (add a `uart_puts(UART1_BASE, line)`), then
   add a second gateway node that only listens — one node relaying for
   another, the seed of a mesh.

## 7. Beyond the UART hub

The UART hub is deliberately the simplest inter-machine medium. Every
other Renode medium uses the identical `connector Connect <peripheral>
<medium>` pattern — only the medium and the peripheral change:

| Medium | Create command | Peripheral | Models |
|---|---|---|---|
| Ethernet switch | `emulation CreateSwitch "sw"` | `sysbus.ethernet` | LANs / TCP-IP (bridge to host via `CreateTap`) |
| CAN hub | `emulation CreateCANHub "c"` | `sysbus.can` | Automotive / industrial ECUs |
| Wireless (generic) | `emulation CreateWirelessMedium "w"` | `radio` | RF with per-node position + range/loss functions |
| IEEE 802.15.4 | `emulation CreateIEEE802_15_4Medium "w"` | 802.15.4 `radio` | Zigbee / Thread / 6LoWPAN mesh |
| BLE | `emulation CreateBLEMedium "w"` | BLE `radio` | Bluetooth Low Energy central/peripheral |

The wireless media add what a UART hub can't: physical **position**
(`wireless SetPosition <node> x y z`) and a propagation model, so range
and interference actually affect delivery. Renode ships ready-made
multi-node radio demos under `/opt/renode/scripts/multi-node/`
(e.g. nRF52840 BLE). Those need radio-capable platforms and prebuilt
Zephyr/Contiki images — out of scope here, but the wiring you learned in
this lab is exactly the same.

## 8. Stopping cleanly

The nodes loop forever, so just:

```text
quit
```

…or `Ctrl-D`.

## What this lab proves

- Renode hosts **many machines in one emulation**, each fully isolated
  (own CPU, RAM, peripherals) yet advancing on a single shared clock.
- A `connector` + `UARTHub` models a **shared broadcast medium**: the
  primitive behind multi-drop buses and single-channel radios, and the
  backbone of any multi-node IoT simulation.
- Identical firmware can take on different roles from a compile-time
  identity — exactly how real sensor fleets and gateways are built.
- Emergent, system-level behaviour (report convergence, bus collisions)
  is observable with zero hardware, entirely in simulated time.
- The same `connector` pattern scales up to Ethernet, CAN, and true
  wireless meshes — the UART hub is just the smallest step onto that path.
