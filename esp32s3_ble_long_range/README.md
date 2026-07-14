# ESP32-S3 BLE Long-Range GATT Server

Production-oriented ESP-IDF example for demonstrating Bluetooth LE long-range behavior on the ESP32-S3 using documented APIs only.

The firmware exposes a small GATT server over LE Coded PHY extended advertising, publishes RSSI and PHY telemetry, and allows a connected central to request manual PHY or transmit-power changes. It is intended as a safe baseline for range experiments, enclosure validation, and BLE 5 coded-link testing.

## Key Features

- Connectable extended advertising on Bluetooth LE Coded PHY.
- Default connection preference for LE Coded PHY.
- Runtime PHY requests for LE 1M, LE Coded S=2, and LE Coded S=8.
- Documented transmit-power control for default, advertising, and connection contexts.
- Adaptive link policy based on filtered RSSI.
- Readable and notifiable telemetry characteristic.
- Writable control characteristic for manual experiments.
- No undocumented RF register writes or private controller patches.

## Target Platform

| Item | Requirement |
| --- | --- |
| MCU | ESP32-S3 |
| Framework | ESP-IDF 5.4.x or newer |
| Bluetooth host | Bluedroid |
| Bluetooth mode | BLE only |
| Central device | BLE 5 central with LE Coded PHY support |

Many phones support Bluetooth 5 but do not expose LE Coded PHY scanning or connection controls to normal applications. For reliable testing, use another ESP32-S3, ESP32-C6, nRF52840, or a BLE development kit that can scan and connect on Coded PHY.

## Project Layout

```text
.
|-- CMakeLists.txt
|-- README.md
|-- FLOWCHART.md
|-- TESTING.md
|-- sdkconfig.defaults
`-- main
    |-- CMakeLists.txt
    |-- main.c
    |-- ble_long_range.c
    `-- ble_long_range.h
```

## Build and Flash

Activate the ESP-IDF environment, then build and flash:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COM_PORT flash monitor
```

Replace `COM_PORT` with the board serial port, for example `COM5` on Windows or `/dev/ttyUSB0` on Linux.

The included `sdkconfig.defaults` enables BLE, Bluedroid, BLE 5 features, BLE-only controller mode, one BLE connection, performance optimization, and a single-app partition table.

## Runtime Behavior

1. `app_main()` calls `ble_long_range_init()`.
2. NVS, the BLE controller, and the Bluedroid host stack are initialized.
3. The firmware registers GAP and GATTS callbacks.
4. A GATT attribute table is created for telemetry and control.
5. Extended advertising is configured and started on LE Coded PHY.
6. When a central connects, the firmware requests maximum documented connection power and LE Coded PHY with S=8 preference.
7. A FreeRTOS task periodically reads peer RSSI.
8. GAP RSSI events update a low-pass filtered RSSI value, publish telemetry, and apply the adaptive link policy.
9. Control-characteristic writes can override PHY or transmit-power settings for testing.
10. On disconnect, connection state is cleared and extended advertising restarts.

See [FLOWCHART.md](FLOWCHART.md) for Mermaid diagrams of the firmware flow.

## GATT API

### Device Name

```text
ESP32S3-LR-BLE
```

### Service

| Role | UUID |
| --- | --- |
| Service | `7a2e1000-5d9b-4d6f-a621-bd2c5b7a9011` |
| Telemetry characteristic | `7a2e1001-5d9b-4d6f-a621-bd2c5b7a9011` |
| Control characteristic | `7a2e1002-5d9b-4d6f-a621-bd2c5b7a9011` |

### Characteristics

| Characteristic | Properties | Purpose |
| --- | --- | --- |
| Telemetry | Read, Notify | Reports RSSI, filtered RSSI, negotiated PHY, TX power level, and sample count. |
| Control | Read, Write | Accepts ASCII commands for manual PHY and power testing. |

## Telemetry Format

Telemetry is formatted as compact ASCII:

```text
rssi=-84,filtered=-81,phy=CODED,pwr=15,samples=42
```

| Field | Description |
| --- | --- |
| `rssi` | Most recent raw RSSI sample in dBm. |
| `filtered` | Low-pass filtered RSSI value used by the adaptive policy. |
| `phy` | Last reported TX PHY: `1M`, `2M`, `CODED`, or `UNKNOWN`. |
| `pwr` | Current ESP-IDF `esp_power_level_t` value tracked by the application. |
| `samples` | Number of RSSI samples processed since the current connection started. |

## Control Commands

Write one ASCII command to the control characteristic:

| Command | Action |
| --- | --- |
| `AUTO` | Apply the adaptive PHY and transmit-power policy immediately. |
| `1M` | Request LE 1M PHY. |
| `S2` | Request LE Coded PHY with S=2 coding preference. |
| `S8` | Request LE Coded PHY with S=8 coding preference. |
| `LOW` | Set connection transmit power to `ESP_PWR_LVL_P3`. |
| `MEDIUM` | Set connection transmit power to `ESP_PWR_LVL_P9`. |
| `HIGH` | Set connection transmit power to `ESP_PWR_LVL_P15`. |
| `MAX` | Set connection transmit power to `ESP_PWR_LVL_P20`. |

PHY selection is negotiated. A successful ESP-IDF API call means the request was accepted by the local stack; the peer and controller may still choose a different PHY.

## Adaptive Link Policy

The policy uses filtered RSSI thresholds to balance throughput, range, and power:

| Filtered RSSI | Connection Power | PHY Request |
| --- | --- | --- |
| `>= -67 dBm` | `ESP_PWR_LVL_P3` | LE 1M |
| `>= -82 dBm` and `< -67 dBm` | `ESP_PWR_LVL_P9` | LE 1M |
| `>= -92 dBm` and `< -82 dBm` | `ESP_PWR_LVL_P15` | LE Coded S=2 |
| `< -92 dBm` | `ESP_PWR_LVL_P20` | LE Coded S=8 |

The filter uses a simple integer low-pass update with an alpha of `1/8`, which reduces PHY and power churn caused by short RSSI fades.

## Production-Safe BLE Range Guidance

Long range is not achieved by PHY selection alone. For production hardware, validate the complete RF path:

- Use a module or antenna design with a correct keepout area.
- Keep copper, batteries, displays, cables, and metal away from the antenna.
- Validate custom RF layouts with a controlled 50-ohm path.
- Measure antenna matching in the final enclosure with a VNA.
- Confirm that the 3.3 V rail remains stable during maximum-power radio bursts.
- Check regional EIRP, antenna gain, and certification requirements before shipment.
- Test coexistence impact if Wi-Fi is enabled in a derived product.

## Range-Test Workflow

Use [TESTING.md](TESTING.md) as the step-by-step range-test checklist. Recommended measurements include:

- Maximum stable connection distance.
- Packet delivery rate over a fixed sample count.
- Reconnect count and time to reconnect.
- Raw and filtered RSSI over distance.
- PHY update events.
- Orientation sensitivity at 0, 90, 180, and 270 degrees.
- Behavior inside the final enclosure and at minimum supply voltage.

## Limitations

- LE Coded PHY increases on-air time and may reduce throughput.
- The peer device must support the requested PHY.
- Central applications on phones may not expose Coded PHY scanning.
- Transmit-power APIs request controller settings; actual radiated power depends on module, board layout, antenna, enclosure, and regulatory constraints.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Central cannot find the device | Confirm the scanner supports extended advertising and LE Coded PHY. |
| Connects but remains on 1M PHY | Confirm the central supports Coded PHY connections and allows PHY updates. |
| Telemetry does not notify | Subscribe to the telemetry CCCD and confirm notifications are enabled. |
| RSSI is noisy | Use filtered RSSI for policy decisions and collect enough samples. |
| Build fails on PHY constants | Confirm the project is using ESP-IDF 5.4.x or newer and the documented GAP PHY constants. |

