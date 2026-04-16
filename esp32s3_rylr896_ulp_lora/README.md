# ESP32-S3 Ultra-Low Power LoRa Sensor with RYLR896 and ULP Scheduler

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.4.1-blue?logo=espressif)](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/)
[![Target](https://img.shields.io/badge/Target-ESP32--S3-green?logo=espressif)](https://www.espressif.com/en/products/socs/esp32-s3)
[![LoRa](https://img.shields.io/badge/LoRa-RYLR896-orange)](https://reyax.com/products/rylr896/)
[![License](https://img.shields.io/badge/License-MIT-yellow)](LICENSE)

A production-ready ESP-IDF v5.4.1 firmware project for the ESP32-S3 that periodically transmits LoRa packets using the REYAX RYLR896 module while achieving **microamp-level average current draw** through aggressive deep sleep and the ESP32-S3's ULP RISC-V coprocessor.

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Project Structure](#project-structure)
- [Prerequisites](#prerequisites)
- [Build Instructions](#build-instructions)
- [Flash and Monitor](#flash-and-monitor)
- [Configuration](#configuration)
- [Power Optimisation](#power-optimisation)
- [Serial Output](#serial-output)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

Most IoT sensor nodes waste energy by keeping the main CPU running continuously, polling timers, or using FreeRTOS task delays. This project takes a fundamentally different approach:

- The **main Xtensa LX7 CPU** is powered off completely between transmissions using deep sleep.
- The **ULP RISC-V coprocessor** runs at a fraction of the main CPU's power, acting as a lightweight hardware scheduler that counts sleep cycles and wakes the main CPU only when a LoRa transmission is due.
- The **RYLR896 LoRa module** is controlled over UART using standard AT commands and is only initialised for the duration of each transmission window, then fully released.

The result is a system that spends the vast majority of its time drawing ~25–30 µA, with brief active bursts of less than 200 ms every 60 seconds — achieving an average current well under 100 µA on bare module hardware.

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                        ESP32-S3                          │
│                                                          │
│  ┌──────────────────┐        ┌────────────────────────┐  │
│  │   Main CPU       │        │   ULP RISC-V Core      │  │
│  │  (Xtensa LX7)    │        │   (RTC domain)         │  │
│  │                  │        │                        │  │
│  │  app_main()      │◄───────│  wakeup_counter++      │  │
│  │  ├─ init UART    │  wake  │  if counter >= 60:     │  │
│  │  ├─ AT+SEND      │        │    tx_due = 1          │  │
│  │  ├─ deinit UART  │        │    wake_main_cpu()     │  │
│  │  └─ deep_sleep() │        │  ulp_riscv_halt()      │  │
│  └────────┬─────────┘        └───────────┬────────────┘  │
│           │ deep sleep                   │ RTC timer      │
│           └──────────────────────────────┘  (1 s tick)    │
│                                                          │
│  ┌───────────────────────────────────────────────────┐  │
│  │               RTC Slow Memory                     │  │
│  │   ulp_wakeup_counter  |  ulp_tx_due               │  │
│  └───────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
            │ UART1 (TX=GPIO17, RX=GPIO18)
            ▼
 ┌──────────────────────┐
 │   RYLR896 LoRa       │
 │   (SX1276 / 915 MHz) │
 └──────────────────────┘
```

**Execution flow:**

1. **Cold boot** — the main CPU loads the ULP binary into RTC memory, sets the ULP timer period to 1 second, starts the ULP core, and immediately enters deep sleep.
2. **ULP ticks every 1 second** — the RTC timer fires the ULP. It increments `wakeup_counter` and halts. The main CPU is never involved.
3. **Transmission trigger at 60 ticks** — the ULP sets `tx_due = 1` and calls `ulp_riscv_wakeup_main_processor()`.
4. **Active window (<200 ms)** — the main CPU wakes, initialises UART, sends `AT+SEND`, waits for `+OK`, deinitialises UART, clears the counters, and re-enters deep sleep.

---

## Hardware Requirements

| Component | Details |
|-----------|---------|
| ESP32-S3 module or devboard | Any variant; bare module recommended for lowest power |
| REYAX RYLR896 LoRa module | UART AT-command interface, SX1276 chipset |
| 3.3 V regulated supply | Shared between ESP32-S3 and RYLR896 |
| Jumper wires | 4 connections required (6 if using optional reset) |

> **Frequency band:** The firmware defaults to **915 MHz** (Americas / AU). Change `RYLR896_BAND_HZ` in `main/rylr896.h` to `868000000` for Europe or `433000000` for the 433 MHz band. Confirm your module hardware supports the target frequency.

---

## Wiring

| ESP32-S3 Pin | RYLR896 Pin | Notes |
|:---:|:---:|---|
| **GPIO 17** | **RX** | UART TX out of ESP32-S3 → into RYLR896 |
| **GPIO 18** | **TX** | UART RX into ESP32-S3 ← from RYLR896 |
| **3V3** | **VCC** | 3.3 V power |
| **GND** | **GND** | Common ground |
| 3V3 via 10 kΩ | **NRST** | Optional: prevents floating reset pin noise |

No level shifting is required — both devices operate at 3.3 V logic levels.

### Schematic (ASCII)

```
ESP32-S3                              RYLR896
────────                              ───────
GPIO17 (TX) ──────────────────────►  RX
GPIO18 (RX) ◄──────────────────────  TX
3V3         ──────────────────────►  VCC
GND         ──────────────────────►  GND

3V3 ──[10kΩ]──────────────────────►  NRST  (optional)
```

---

## Project Structure

```
esp32s3_rylr896_ulp_lora/
│
├── CMakeLists.txt                   Top-level ESP-IDF project file
├── sdkconfig.defaults               Pre-tuned Kconfig defaults (ULP, no BT, warn logs)
├── README.md                        This file
├── FLOWCHART.md                     Mermaid program-flow diagrams
├── RYLR896_README.md                RYLR896 module reference and AT command guide
│
└── main/
    ├── CMakeLists.txt               Component + ULP binary embedding (relative path fix)
    ├── main.c                       app_main(): cold-boot and wakeup dispatch
    ├── rylr896.h                    Driver public API and pin/radio configuration
    ├── rylr896.c                    UART driver: init, AT commands, send, deinit
    └── ulp/
        └── ulp_lora_scheduler.c     ULP RISC-V: tick counter and wakeup trigger
```

### Key files explained

| File | Purpose |
|------|---------|
| `main.c` | Top-level boot/wake dispatcher. Detects cold boot vs ULP wakeup, calls the appropriate path, always returns to deep sleep. |
| `ulp_lora_scheduler.c` | The ULP program. Increments a counter every RTC timer tick; wakes the main CPU when the transmission interval is reached. |
| `rylr896.c` | Full UART driver for the RYLR896 AT command set. Handles `AT`, `AT+ADDRESS`, `AT+NETWORKID`, `AT+BAND`, and `AT+SEND` with blocking `+OK` validation. |
| `sdkconfig.defaults` | Enables ULP RISC-V, disables Bluetooth, sets WARN log level. Applied automatically on first build. |

---

## Prerequisites

- **ESP-IDF v5.4.1** — Install via the [official getting started guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/get-started/index.html).
- `idf.py` on your `PATH` — source `export.sh` (Linux/macOS) or `export.bat` (Windows) from your IDF installation.
- USB-to-serial connection to your ESP32-S3 for flashing.

Verify your installation before building:

```bash
idf.py --version
# Expected: ESP-IDF v5.4.1
```

---

## Build Instructions

> **If you have built a previous version of this project**, you must delete the `build/` directory before building. The cached generated ULP header will contain stale symbol names and cause compile errors.

```bash
# 1. Enter the project root
cd esp32s3_rylr896_ulp_lora

# 2. Set the build target (only required once)
idf.py set-target esp32s3

# 3. (Optional) Review or customise configuration
idf.py menuconfig

# 4. Build
idf.py build
```

A successful build ends with:
```
Project build complete. To flash, run:
  idf.py flash
```

### Clean rebuild

```bash
# Windows
rmdir /s /q build
idf.py set-target esp32s3
idf.py build

# Linux / macOS
rm -rf build
idf.py set-target esp32s3
idf.py build
```

---

## Flash and Monitor

```bash
# Flash and open serial monitor
idf.py -p PORT flash monitor
```

| OS | Typical PORT value |
|----|---|
| Windows | `COM3`, `COM4`, … (check Device Manager → Ports) |
| Linux | `/dev/ttyUSB0` or `/dev/ttyACM0` |
| macOS | `/dev/cu.usbserial-XXXX` |

Press **Ctrl + ]** to exit the serial monitor.

```bash
# Flash only
idf.py -p PORT flash

# Monitor only (already flashed)
idf.py -p PORT monitor
```

---

## Configuration

### Transmission interval

Two constants control how often LoRa packets are sent:

| File | Constant | Default | Description |
|------|----------|---------|-------------|
| `main/main.c` | `ULP_WAKEUP_PERIOD_US` | `1000000` | ULP timer period in microseconds (1 s) |
| `main/ulp/ulp_lora_scheduler.c` | `TX_INTERVAL_COUNT` | `60` | ULP ticks between transmissions |

**Transmission period = `ULP_WAKEUP_PERIOD_US` × `TX_INTERVAL_COUNT`**

Examples:

| `TX_INTERVAL_COUNT` | Period |
|---|---|
| `30` | 30 seconds |
| `60` | 1 minute (default) |
| `300` | 5 minutes |
| `3600` | 1 hour |

### LoRa radio parameters (`main/rylr896.h`)

| Constant | Default | Description |
|----------|---------|-------------|
| `RYLR896_TX_PIN` | `17` | ESP32-S3 GPIO for UART TX |
| `RYLR896_RX_PIN` | `18` | ESP32-S3 GPIO for UART RX |
| `RYLR896_ADDRESS` | `1` | Device address (0–65535) |
| `RYLR896_NETWORK_ID` | `18` | Shared network ID for all nodes |
| `RYLR896_BAND_HZ` | `915000000` | RF frequency in Hz |
| `RYLR896_DEST_ADDR` | `0` | Destination address (0 = broadcast) |
| `RYLR896_CMD_TIMEOUT_MS` | `3000` | AT command timeout in ms |

---

## Power Optimisation

### Techniques used

| Technique | Mechanism | Saving |
|-----------|-----------|--------|
| Deep sleep | `esp_deep_sleep_start()` shuts down the main CPU, all RAM except RTC, and most clocks | Reduces CPU from ~80 mA to ~0 |
| ULP scheduler | 8 MHz RISC-V core handles timing; wakes for a few µs per tick then halts | Avoids waking the full CPU every second |
| UART lifecycle | Driver installed only for the <200 ms TX window, then `uart_driver_delete()` | Peripheral powered down during sleep |
| Radio disabled | `CONFIG_BT_ENABLED=n`; Wi-Fi not referenced or compiled | Removes both radio stacks |
| Minimal logging | `CONFIG_LOG_DEFAULT_LEVEL=2` (WARN) | Minimises UART active time |

### Current budget (bare ESP32-S3 module, 3.3 V LDO)

| Phase | Duration | Current |
|-------|----------|---------|
| Deep sleep + ULP running | ~59.8 s | ~25–30 µA |
| CPU active (UART + LoRa TX) | <200 ms | ~40–80 mA |
| **Average at 60 s interval** | — | **~50–100 µA** |

> Development boards add 3–20 mA from on-board regulators and USB-UART bridge chips. For accurate measurements, use a bare module with a low-IQ LDO.

### Formula

```
I_avg ≈ I_sleep + (I_active × t_active_s) / T_cycle_s

Example (default settings):
I_avg ≈ 28µA + (60mA × 0.2s) / 60s ≈ 28µA + 200µA/s ≈ 48µA
```

---

## Serial Output

### Cold boot
```
I (312) MAIN: Cold boot - loading ULP scheduler
I (318) RYLR896: Initialised (addr=1, netid=18, band=915000000 Hz)
I (524) RYLR896: Sent (12 bytes): SENSOR:PKT#0
I (526) MAIN: Entering deep sleep - wake source: ULP
```

### ULP-triggered wakeup (every ~60 s)
```
I (312) MAIN: Woke from ULP (counter=60, tx_due=1)
I (318) RYLR896: Initialised (addr=1, netid=18, band=915000000 Hz)
I (524) RYLR896: Sent (14 bytes): SENSOR:PKT#60
I (526) MAIN: Entering deep sleep - wake source: ULP
```

---

## Troubleshooting

| Symptom | Likely Cause | Resolution |
|---------|-------------|------------|
| `Module did not respond to AT ping` | Wrong GPIO pins or module not powered | Verify the wiring table; check 3.3 V supply voltage |
| `Failed to set BAND` | Unsupported frequency for your RYLR896 variant | Check the label on your module; use 868 MHz for EU variants |
| Device never wakes after sleep | ULP failed to start | Review cold-boot log for errors from `ulp_riscv_load_binary` or `ulp_riscv_run` |
| `ESP_ERR_NO_MEM` during ULP load | ULP memory reservation too small | Increase `CONFIG_ULP_COPROC_RESERVE_MEM` in `sdkconfig.defaults` |
| Compile error: `ulp_ulp_wakeup_counter` | Stale ULP header from previous build | Delete `build/` entirely and rebuild from scratch |
| IntelliSense error on `ulp_lora_scheduler.h` | File is generated at build time | Run `idf.py build` once; VS Code finds it automatically after that |
| High current draw during sleep | Dev board USB-UART bridge active | Use a bare module; disconnect or disable the bridge chip |
| `Woke from ULP but tx_due not set` | Unexpected spurious wakeup | Not harmful; device returns to sleep immediately |

---

## Contributing

Contributions are welcome — bug reports, feature requests, and pull requests.

**Before submitting a pull request:**
- Test on real ESP32-S3 hardware with a physical RYLR896 module.
- Confirm `idf.py build` succeeds on a clean `build/` directory.
- Follow the Google-style Doxygen comment format used throughout the project.
- Keep the active CPU window under 200 ms to maintain the power budget.

---

## License

This project is released under the [MIT License](LICENSE).

---

## Acknowledgements

- [Espressif Systems](https://www.espressif.com/) — ESP-IDF framework, ULP RISC-V toolchain, and documentation.
- [REYAX Technology](https://reyax.com/) — RYLR896 module hardware and AT command reference.
- The ESP-IDF community — issue discussions and examples that informed the ULP symbol naming conventions.
