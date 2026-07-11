# ESP-NOW Low-Power Mesh for ESP32-S3

Professional ESP-IDF reference project for an application-layer, low-power mesh built on ESP-NOW. The same firmware image source supports root gateways, always-awake routers, and deep-sleep leaf sensor nodes through `menuconfig`.

The project demonstrates a practical architecture for battery-powered ESP32-S3 telemetry nodes where leaf devices wake, synchronize briefly, transmit one sensor packet with application-level retry and acknowledgement, then return to deep sleep.

## Key Features

- ESP-NOW transport with fixed-channel Wi-Fi station mode.
- Three configurable node roles: root, router, and leaf.
- Deep-sleep duty cycle for leaf sensor nodes.
- Application-layer packet format with versioning, TTL, sequence number, payload CRC, and typed packets.
- Root synchronization beacons for leaf wake windows.
- Application-level ACK and retry loop for sensor telemetry.
- Router forwarding with TTL decrement and duplicate suppression.
- Optional ESP-NOW peer encryption using PMK and LMK keys.
- RTC-retained boot count and packet sequence for leaf wake cycles.
- Deterministic demo sensor data that can be replaced with real sensor drivers.

## Architecture Overview

```text
Leaf node(s)               Router node(s)                Root gateway
deep sleep most of time    always awake                  always awake

Wake -> listen beacon
Sample sensor
Send SENSOR packet ------> ACK child packet
Wait for ACK               Decrement TTL
Sleep                      Forward SENSOR packet ------> ACK router packet
                                                        Log telemetry
Broadcast BEACON <-------------------------------------- periodic beacon
```

The mesh is implemented above ESP-NOW. ESP-NOW provides fast peer-to-peer frame delivery, while this project adds packet typing, CRC validation, deduplication, hop limiting, forwarding, and application-level acknowledgement.

## Supported Roles

| Role | Power mode | Main responsibilities |
| --- | --- | --- |
| Root gateway | Always awake | Broadcast synchronization beacons, receive sensor packets, ACK immediate senders, deduplicate packets, and log telemetry. |
| Router node | Always awake | Receive child packets, ACK immediate senders, deduplicate packets, decrement TTL, and forward valid sensor packets to its configured parent. |
| Leaf sensor | Deep sleep | Wake on RTC timer, wait briefly for beacons, send one sensor packet with retries, process ACKs, stop radio, and return to deep sleep. |

Important: sleeping nodes cannot forward packets. Root and router nodes intentionally remain awake so they can receive and relay traffic at any time.

## Project Structure

```text
esp32s3_espnow_low_power_mesh/
|-- CMakeLists.txt
|-- partitions.csv
|-- sdkconfig.defaults
|-- README.md
|-- FLOWCHART.md
`-- main/
    |-- CMakeLists.txt
    |-- Kconfig.projbuild
    |-- main.c
    |-- mesh_protocol.c
    |-- mesh_protocol.h
    |-- power_manager.c
    `-- power_manager.h
```

### Important Files

| File | Purpose |
| --- | --- |
| `main/main.c` | Application entry point, role selection, Wi-Fi and ESP-NOW setup, callbacks, ACK/retry logic, routing, and deep-sleep leaf cycle. |
| `main/mesh_protocol.h` | Defines packet types, packet layout, protocol version, broadcast node ID, and public protocol helpers. |
| `main/mesh_protocol.c` | Implements CRC-16/CCITT-FALSE, packet finalization, packet validation, and MAC address parsing. |
| `main/power_manager.c` | Enables timer wake-up, flushes log output, enters deep sleep, and reports wake-up cause. |
| `main/Kconfig.projbuild` | Project-specific `menuconfig` options for role, node ID, channel, parent MAC, timing, retry, TTL, and encryption. |
| `sdkconfig.defaults` | Default ESP32-S3 target, 8 MB flash, 1 kHz FreeRTOS tick, and info-level logging. |
| `partitions.csv` | NVS, PHY, and factory app partition layout. |

## Requirements

- ESP32-S3 development boards.
- ESP-IDF 5.4 or later.
- ESP-IDF Python environment and Xtensa toolchain.
- USB cables for flashing and serial monitoring.
- At least one root node and one leaf node for a minimal demo.
- Optional additional ESP32-S3 boards for router hops.

## Quick Start

From the project directory:

```powershell
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Use the `ESP-NOW Low-Power Mesh Configuration` menu to configure each board before building and flashing it.

If you are using PowerShell and `idf.py` is not available, load your ESP-IDF environment first:

```powershell
& "$HOME\esp\v5.4.1\esp-idf\export.ps1"
```

Adjust the ESP-IDF path for your local installation.

## Configuration Reference

Open:

```text
idf.py menuconfig
  -> ESP-NOW Low-Power Mesh Configuration
```

| Option | Kconfig symbol | Default | Description |
| --- | --- | --- | --- |
| Node role | `APP_ROLE_ROOT`, `APP_ROLE_ROUTER`, `APP_ROLE_LEAF` | Leaf | Selects root gateway, router, or deep-sleep leaf behavior. |
| Logical node ID | `APP_NODE_ID` | `1` | Application-level node identifier, range `1..65535`. |
| Parent MAC address | `APP_PARENT_MAC` | `FF:FF:FF:FF:FF:FF` | Upstream router or root station MAC for router and leaf roles. |
| ESP-NOW Wi-Fi channel | `APP_WIFI_CHANNEL` | `1` | Fixed ESP-NOW channel. Every node in the mesh must use the same channel. |
| Leaf wake interval | `APP_WAKE_INTERVAL_SEC` | `60` | RTC deep-sleep interval for leaf nodes. |
| Root beacon interval | `APP_BEACON_INTERVAL_MS` | `1000` | Period between root synchronization beacons. |
| Leaf beacon wait window | `APP_BEACON_WAIT_MS` | `600` | Maximum time a leaf waits for a beacon before transmitting anyway. |
| ACK timeout | `APP_ACK_TIMEOUT_MS` | `120` | Per-attempt wait time for an application ACK. |
| Maximum packet retries | `APP_MAX_RETRIES` | `3` | Number of retries after the first send attempt. |
| Initial packet TTL | `APP_FORWARD_TTL` | `6` | Hop limit for sensor packets. Routers decrement this before forwarding. |
| ESP-NOW encryption | `APP_ENABLE_ENCRYPTION` | Disabled | Enables PMK/LMK configuration for parent peers. |
| ESP-NOW PMK | `APP_PMK` | `pmk1234567890123` | Primary master key. Must be exactly 16 ASCII characters. |
| ESP-NOW LMK | `APP_LMK` | `lmk1234567890123` | Local master key. Must be exactly 16 ASCII characters. |

## Building a Root Gateway

1. Run `idf.py menuconfig`.
2. Select `Node role = Root gateway (always awake)`.
3. Set a unique logical node ID, for example `1`.
4. Set the ESP-NOW channel, for example `1`.
5. Build and flash:

```powershell
idf.py build
idf.py -p COM5 flash monitor
```

Record the station MAC printed at startup. Router and leaf nodes use this MAC as their `Parent MAC address` when the root is their direct upstream parent.

## Building a Router Node

1. Run `idf.py menuconfig`.
2. Select `Node role = Router node (always awake)`.
3. Set a unique logical node ID, for example `10`.
4. Set `Parent MAC address` to the station MAC of the root or an upstream router.
5. Set the same ESP-NOW channel as the root.
6. Build and flash:

```powershell
idf.py build
idf.py -p COM6 flash monitor
```

Record the router station MAC. Leaf nodes can use the router MAC as their parent to create a multi-hop topology.

## Building a Leaf Sensor Node

1. Run `idf.py menuconfig`.
2. Select `Node role = Leaf sensor node (deep sleep)`.
3. Set a unique logical node ID, for example `100`.
4. Set `Parent MAC address` to the station MAC of the root or router.
5. Set the same ESP-NOW channel as the parent.
6. Set the wake interval and retry timing for your power and latency target.
7. Build and flash:

```powershell
idf.py build
idf.py -p COM7 flash monitor
```

After flashing, the leaf node wakes, initializes ESP-NOW, listens briefly for a beacon, sends one sensor packet, waits for an ACK, shuts down ESP-NOW/Wi-Fi, and enters deep sleep.

## Packet Protocol

All ESP-NOW payloads use the packed `mesh_packet_t` format:

| Field | Type | Description |
| --- | --- | --- |
| `version` | `uint8_t` | Protocol version. Current value is `1`. |
| `type` | `uint8_t` | Packet type: beacon, sensor, or ACK. |
| `source_id` | `uint16_t` | Logical sender node ID. |
| `destination_id` | `uint16_t` | Logical destination node ID or `0xFFFF` for broadcast. |
| `sequence` | `uint16_t` | Per-node packet sequence. Leaf sequence is retained across deep sleep using RTC memory. |
| `ttl` | `uint8_t` | Hop limit. Routers drop packets when TTL is expired. |
| `flags` | `uint8_t` | Reserved for future protocol flags. |
| `uptime_ms` | `uint32_t` | Sender uptime in milliseconds. |
| `temperature_centi_c` | `int16_t` | Temperature in hundredths of a degree Celsius. |
| `humidity_centi_pct` | `uint16_t` | Relative humidity in hundredths of a percent. |
| `battery_mv` | `uint16_t` | Battery voltage in millivolts. |
| `payload_crc` | `uint16_t` | CRC-16/CCITT-FALSE over the packet with this field zeroed. |

### Packet Types

| Type | Value | Producer | Consumer | Purpose |
| --- | --- | --- | --- | --- |
| `MESH_PACKET_BEACON` | `1` | Root | Leaf/router receive path | Synchronization signal for leaf wake windows. |
| `MESH_PACKET_SENSOR` | `2` | Leaf, forwarded by routers | Router/root | Telemetry payload. |
| `MESH_PACKET_ACK` | `3` | Immediate receiver | Previous hop sender | Application-level acknowledgement for a received sensor packet. |

## Data Flow

1. Root starts two tasks: a beacon task and a receive task.
2. Router starts a receive task and registers its configured parent as an ESP-NOW peer.
3. Leaf wakes from timer deep sleep and initializes the radio stack.
4. Leaf waits up to `APP_BEACON_WAIT_MS` for a root beacon.
5. Leaf sends a finalized sensor packet to its parent.
6. Receiver validates version, type, size, and CRC.
7. Receiver sends an ACK to the immediate sender.
8. Root logs unique sensor packets.
9. Router drops duplicates, decrements TTL, and forwards valid packets upstream.
10. Leaf retries until an ACK is received or retry budget is exhausted.
11. Leaf deinitializes ESP-NOW, stops Wi-Fi, configures timer wake, and enters deep sleep.

See `FLOWCHART.md` for Mermaid diagrams of the startup, role, receive, retry, and deep-sleep flows.

## Sensor Integration

The project intentionally runs without external hardware. `populate_demo_sensor_data()` generates deterministic values from the retained boot count:

- Temperature starts around `22.00 C`.
- Humidity starts around `48.00 %`.
- Battery voltage decreases from `4200 mV` in a repeating demo pattern.

Replace `populate_demo_sensor_data()` with real sensor reads. Keep these points in mind:

- Initialize sensors only when needed on leaf nodes.
- Prefer one-shot conversions and low-power sensor modes.
- Account for sensor warm-up and conversion time in the wake energy budget.
- Avoid long blocking delays inside the ESP-NOW receive callback.
- Do not perform heavy work in callbacks; queue data and process it in tasks.

## Encryption Notes

ESP-NOW unicast peer encryption can be enabled with `APP_ENABLE_ENCRYPTION`.

Requirements:

- PMK and LMK must each be exactly 16 ASCII characters.
- All encrypted peers must be configured with compatible keys.
- Broadcast beacons remain unencrypted because ESP-NOW broadcast peers do not use per-peer LMK encryption.
- The code only enables encryption for the configured parent peer when encryption is enabled.

## Power Behavior

Leaf nodes:

- Retain `s_boot_count` and `s_sequence` in RTC memory.
- Wake through the RTC timer.
- Keep radio-on time short by waiting only a bounded beacon window.
- Stop ESP-NOW and Wi-Fi before deep sleep.
- Flush log output before entering deep sleep.

Root and router nodes:

- Stay awake permanently.
- Must remain powered so they can receive child traffic and forward packets.
- Are not optimized for deep-sleep current in this reference design.

## Power Measurement Guidance

For realistic current measurements:

- Power the board from a measurement instrument instead of USB when possible.
- Remove or disable development-board power LEDs.
- Avoid USB-UART bridges on the measured battery rail.
- Use a low-quiescent-current regulator.
- Measure both peak transmit current and average sleep-cycle current.
- Increase `APP_WAKE_INTERVAL_SEC` to reduce average current.
- Keep `APP_BEACON_WAIT_MS`, `APP_ACK_TIMEOUT_MS`, and `APP_MAX_RETRIES` as low as your deployment reliability allows.

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Leaf never receives ACK | Wrong parent MAC, channel mismatch, parent not powered, or low RSSI. | Confirm station MACs in monitor output and use the same `APP_WIFI_CHANNEL` on all nodes. |
| Root receives duplicates | Leaf retries after missing an ACK, or router path duplicates traffic. | Duplicate suppression should ignore repeated source ID and sequence pairs; inspect ACK timing and signal quality. |
| Router drops packets | TTL expired. | Increase `APP_FORWARD_TTL` or reduce hop count. |
| Encrypted peer does not communicate | PMK/LMK mismatch or invalid key length. | Configure matching 16-character keys on every encrypted unicast peer. |
| Leaf current is too high | Board peripherals, LEDs, USB bridge, long wake window, or sensor delays. | Measure bare module or low-power board and tune wake/retry timing. |
| Build uses stale role settings | Existing `sdkconfig` still contains previous choices. | Re-run `idf.py menuconfig`, or remove `sdkconfig` and regenerate from defaults. |

## Current Limitations

- There is no dynamic route discovery. Each router or leaf has one configured parent MAC.
- Sleeping nodes cannot act as routers.
- The root is the telemetry sink; the example does not implement cloud upload or persistent storage.
- The duplicate cache is in RAM and sized for a small demonstration network.
- Sensor data is demo-generated until `populate_demo_sensor_data()` is replaced.
- Broadcast beacons are not encrypted.
- Network-wide time synchronization and scheduled forwarding slots are outside this example.

## Suggested Extensions

- Add real sensor drivers and calibration.
- Persist node configuration in NVS.
- Add route advertisements or parent selection based on RSSI.
- Add signed packets or application-layer encryption for broadcast data.
- Forward root telemetry to MQTT, HTTPS, UART, SD card, or BLE.
- Add a commissioning mode to exchange parent MACs and keys.
- Add packet counters and link-quality metrics.

## Build Verification

This project has been verified to build with ESP-IDF `v5.4.1` for target `esp32s3`.

```powershell
ninja -C build
```

Expected output includes generation of:

```text
build/espnow_low_power_mesh.bin
```
