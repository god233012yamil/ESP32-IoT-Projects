# ESP32-S3 Digital Scale

<p align="center">
  <img src="https://img.shields.io/badge/ESP--IDF-v5.x-blue?logo=espressif" />
  <img src="https://img.shields.io/badge/Target-ESP32--S3-green?logo=espressif" />
  <img src="https://img.shields.io/badge/License-MIT-yellow" />
  <img src="https://img.shields.io/badge/Language-C-lightgrey?logo=c" />
</p>

A production-ready **ESP-IDF** project that turns an **ESP32-S3** microcontroller, four **50 kg half-bridge load cells**, and an **HX711** 24-bit ADC amplifier into a networked digital scale with a live web dashboard accessible from any browser on the same Wi-Fi network.

---

## Table of Contents

1. [Features](#features)
2. [Hardware](#hardware)
   - [Bill of Materials](#bill-of-materials)
   - [Wiring the Load Cells](#wiring-the-load-cells)
   - [HX711 to ESP32-S3 Connection](#hx711-to-esp32-s3-connection)
   - [Schematic Overview](#schematic-overview)
3. [Web Dashboard](#web-dashboard)
4. [REST API](#rest-api)
5. [Project Structure](#project-structure)
6. [Getting Started](#getting-started)
   - [Prerequisites](#prerequisites)
   - [Clone & Configure](#clone--configure)
   - [Build & Flash](#build--flash)
   - [Monitor](#monitor)
7. [Calibration](#calibration)
   - [Why Calibration Is Needed](#why-calibration-is-needed)
   - [Step-by-Step Calibration](#step-by-step-calibration)
   - [Calibration via Web API](#calibration-via-web-api)
8. [Configuration Reference](#configuration-reference)
9. [Architecture](#architecture)
   - [Task Diagram](#task-diagram)
   - [Component Dependencies](#component-dependencies)
10. [Troubleshooting](#troubleshooting)
11. [Contributing](#contributing)
12. [License](#license)

---

## Features

| Feature | Detail |
|---|---|
| **24-bit resolution** | HX711 at gain 128 on Channel A → ≈ 0.5 g resolution over 200 kg |
| **Noise reduction** | Configurable multi-sample averaging (default 10 samples) |
| **Live web dashboard** | Single-page app served directly from the ESP32-S3 flash – no internet required |
| **Real-time updates** | Server-Sent Events (SSE) push weight to the browser at 500 ms intervals |
| **60-second history graph** | Chart.js rolling line chart embedded in the dashboard |
| **REST API** | JSON endpoints for weight, tare, calibration, and system status |
| **NVS persistence** | Scale factor and tare offset survive power cycles |
| **Interactive calibration** | UART wizard and HTTP API for in-field calibration |
| **Unit toggle** | Browser-side conversion between grams, kilograms, and pounds |
| **Session statistics** | Min / Max / Average tracked per browser session |
| **Power management** | HX711 software power-down API to reduce standby current |
| **Wi-Fi retry logic** | Automatic reconnection with configurable retry count |
| **Zero threshold** | Configurable dead-band to suppress tare noise |

---

## Hardware

### Bill of Materials

| Qty | Component | Notes |
|-----|-----------|-------|
| 1 | ESP32-S3 development board | Any variant with ≥ 4 MB flash; DevKitC-1 recommended |
| 4 | 50 kg Half-Bridge Load Cell / Strain Gauge | "Human body scale" type |
| 1 | HX711 Amplifier Module | 24-bit ADC, 80 Hz output rate at 3.3 V supply |
| 1 | Scale platform / frame | Aluminium or MDF, sized to mount all four cells at corners |
| — | Jumper wires | Standard Dupont wires |
| — | 5 V or 3.3 V power supply | The HX711 module runs on 3.3 V (VCC → 3V3) |

### Wiring the Load Cells

Four **half-bridge** load cells are combined into a single **full Wheatstone bridge** before connecting to the HX711. This is the standard body-scale configuration:

```
              ┌─────────────────────────────────┐
              │         Full Bridge              │
              │                                  │
  Cell 1 RED ─┤ E+                          A+ ├─ Cell 2 WHITE
              │                                  │
  Cell 1 BLK ─┤ E-                          A- ├─ Cell 2 BLACK
              │                                  │
  Cell 3 RED ─┤ (parallel E+)      (parallel A+)├─ Cell 4 WHITE
              │                                  │
  Cell 3 BLK ─┤ (parallel E-)      (parallel A-)├─ Cell 4 BLACK
              └──────────┬──────────┬────────────┘
                         │          │
                      Cell wire   Cell wire
                      colours may vary by manufacturer
```

**Practical connection summary:**

| HX711 Pin | Wire colour (typical) | Connected cells |
|-----------|----------------------|-----------------|
| E+ (EXCITATION +) | Red | Cells 1 & 3 red wires joined |
| E- (EXCITATION −) | Black | Cells 1 & 3 black wires joined |
| A+ (SIGNAL +) | White | Cells 2 & 4 white wires joined |
| A− (SIGNAL −) | Green/Yellow | Cells 2 & 4 green wires joined |

> **Note:** Load cell wire colours vary between manufacturers. Always verify with a multimeter: with no load, measure resistance between wire pairs. Red/Black is normally the excitation pair (~1 kΩ); White/Green is the signal pair (~1 kΩ at rest, changing under load).

### HX711 to ESP32-S3 Connection

| HX711 Pin | ESP32-S3 GPIO | Description |
|-----------|---------------|-------------|
| VCC | 3V3 | Supply voltage (3.3 V) |
| GND | GND | Common ground |
| DT / DOUT | GPIO 4 | Serial data output |
| SCK / PD_SCK | GPIO 5 | Serial clock / power-down |

> GPIO assignments can be changed in `main/scale_config.h` (`CONFIG_HX711_DOUT_GPIO` and `CONFIG_HX711_SCK_GPIO`).

### Schematic Overview

```
 3.3V ──┬──────────────────────────────────────┐
        │                                       │
       HX711                               ESP32-S3
     ┌──────┐                            ┌──────────┐
     │  VCC │◄──── 3.3V                  │          │
     │  GND │──── GND ───────────────────│ GND      │
     │  DT  │────────────────────────────│ GPIO 4   │
     │  SCK │────────────────────────────│ GPIO 5   │
     │  E+  │◄──── Bridge E+             │          │
     │  E-  │◄──── Bridge E−             └──────────┘
     │  A+  │◄──── Bridge A+
     │  A-  │◄──── Bridge A−
     └──────┘
```

---

## Web Dashboard

Once the ESP32-S3 connects to Wi-Fi the serial monitor prints the dashboard URL:

```
I (3241) MAIN: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
I (3242) MAIN:   Dashboard: http://192.168.1.42/
I (3243) MAIN: ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

Open that URL in any browser on the same network. The dashboard features:

- **Live weight display** — large, readable number updated every 500 ms via SSE.
- **Unit toggle** — switch between **g**, **kg**, and **lb** without reloading.
- **60-second rolling chart** — Chart.js line graph of weight history.
- **Session statistics** — Min, Max, and Average for the current browser session.
- **Tare button** — zero the scale with one click.
- **Calibration panel** — enter a scale factor to apply it immediately.
- **System info panel** — IP address, uptime, free heap, and current scale factor.
- **Connection status indicator** — green dot when SSE stream is live.

The entire dashboard is served as a single embedded HTML file from ESP32 flash — **no internet connection, cloud service, or external server is required**.

---

## REST API

All endpoints return `application/json`.

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/` | Dashboard HTML page |
| `GET` | `/api/weight` | `{"weight_g": 123.45, "unit": "g"}` |
| `GET` | `/api/status` | System info: IP, uptime, scale factor, heap |
| `POST` | `/api/tare` | Tare the scale; returns `{"status": "ok"}` |
| `POST` | `/api/calibrate` | Body: `{"scale": 430.0}`; applies new factor |
| `GET` | `/events` | SSE stream — `event: weight` every 500 ms |

### Example curl commands

```bash
# Read current weight
curl http://192.168.1.42/api/weight

# Tare
curl -X POST http://192.168.1.42/api/tare

# Set calibration scale factor
curl -X POST http://192.168.1.42/api/calibrate \
     -H "Content-Type: application/json" \
     -d '{"scale": 435.7}'

# System status
curl http://192.168.1.42/api/status

# SSE stream (Ctrl+C to stop)
curl -N http://192.168.1.42/events
```

---

## Project Structure

```
esp32s3_scale/
├── CMakeLists.txt              # Top-level ESP-IDF project CMake
├── sdkconfig.defaults          # Pre-tuned SDK configuration
├── partitions.csv              # Custom partition table (4 MB flash)
├── LICENSE
├── README.md                   # ← You are here
│
├── components/
│   └── hx711/                  # Reusable HX711 ESP-IDF component
│       ├── CMakeLists.txt
│       ├── hx711.c             # Driver implementation (IRAM-safe)
│       └── include/
│           └── hx711.h         # Public API
│
├── docs/
│   └── HX711_README.md         # HX711 technical deep-dive
│
└── main/
    ├── CMakeLists.txt
    ├── scale_config.h          # All compile-time configuration
    ├── main.c                  # app_main + measurement task
    ├── calibration.h / .c      # NVS persistence + UART wizard
    ├── wifi_manager.h / .c     # Wi-Fi STA connection manager
    └── web_server.h / .c       # HTTP server + SSE + dashboard HTML
```

---

## Getting Started

### Prerequisites

- **ESP-IDF v5.1 or later** — [Installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- **Python 3.8+** (bundled with ESP-IDF)
- A USB cable connected to the ESP32-S3 UART/USB port
- The ESP32-S3 and your computer on the **same Wi-Fi network**

Verify your ESP-IDF installation:

```bash
idf.py --version
# Expected: ESP-IDF v5.x.x
```

### Clone & Configure

```bash
# 1. Clone the repository
git clone https://github.com/your-username/esp32s3-digital-scale.git
cd esp32s3-digital-scale

# 2. Set your Wi-Fi credentials in scale_config.h
#    Edit the two lines below (do NOT commit real passwords):
#      #define CONFIG_WIFI_SSID      "YOUR_WIFI_SSID"
#      #define CONFIG_WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
nano main/scale_config.h

# 3. (Optional) Change GPIO pins if your board layout differs
#      #define CONFIG_HX711_DOUT_GPIO  4
#      #define CONFIG_HX711_SCK_GPIO   5
```

### Build & Flash

```bash
# Set the IDF target (only needed once per clone)
idf.py set-target esp32s3

# Build
idf.py build

# Flash (replace PORT with your serial port, e.g. /dev/ttyUSB0 or COM3)
idf.py -p PORT flash
```

### Monitor

```bash
idf.py -p PORT monitor
```

Look for the dashboard URL in the output:

```
I (3242) MAIN:   Dashboard: http://192.168.1.42/
```

Open that URL in your browser. Press `Ctrl+]` to exit the monitor.

---

## Calibration

### Why Calibration Is Needed

The HX711 outputs a raw integer count that is proportional to the mechanical strain on the load cells. The exact conversion factor (counts per gram) depends on:

- The mechanical properties of your load cell frame.
- The gain setting of the HX711 (default: Channel A, gain 128).
- Supply voltage variations.

Calibration maps raw counts to real-world grams using a **known reference weight**.

### Step-by-Step Calibration

**Method 1 – UART Wizard (recommended for first-time setup)**

Connect a serial monitor (115200 baud) and send the `calibrate` command, or simply reboot with an empty scale. The wizard runs automatically if no calibration data is found in NVS:

```
========================================
      Digital Scale Calibration Wizard
========================================

[1/3] Remove ALL weight from the platform.
      Press ENTER when ready...
      Capturing tare (20 samples)...
      Tare captured: -47382 raw counts

[2/3] Enter the reference weight in grams (e.g. 1000): 1000
      Place the 1000.0 g reference weight on the platform.
      Press ENTER when ready...
      Measuring (20 samples)...
      Loaded avg : 383618 raw counts
      Delta      : 431000 raw counts
      Scale factor: 431.0000 counts/g

[3/3] Saving calibration to NVS...
      Calibration saved successfully!
========================================
```

**Method 2 – HTTP API (for remote / automated calibration)**

```bash
# Step 1 – Tare with nothing on the scale
curl -X POST http://192.168.1.42/api/tare

# Step 2 – Place known weight, compute factor manually:
#   factor = (raw_loaded - raw_tare) / known_grams
#   Then apply:
curl -X POST http://192.168.1.42/api/calibrate \
     -H "Content-Type: application/json" \
     -d '{"scale": 431.0}'
```

**Method 3 – Compile-time default**

Edit `CONFIG_SCALE_FACTOR` in `main/scale_config.h` before flashing. This is used as the fallback when no NVS calibration data exists.

### Calibration Tips

- Use a certified reference weight for best accuracy.
- Calibrate at the centre of the platform where daily use loads are placed.
- Re-tare after the scale has warmed up for ~5 minutes.
- Avoid air currents and vibration during calibration.
- If the reading drifts, increase `CONFIG_SCALE_SAMPLES` to reduce noise.

---

## Configuration Reference

All options are in `main/scale_config.h`.

| Constant | Default | Description |
|----------|---------|-------------|
| `CONFIG_HX711_DOUT_GPIO` | `4` | HX711 DOUT → ESP32 GPIO |
| `CONFIG_HX711_SCK_GPIO` | `5` | HX711 SCK → ESP32 GPIO |
| `CONFIG_SCALE_SAMPLES` | `10` | ADC samples averaged per reading |
| `CONFIG_MEASURE_INTERVAL_MS` | `500` | ms between measurements |
| `CONFIG_TARE_SAMPLES` | `20` | Samples used for tare capture |
| `CONFIG_SCALE_FACTOR` | `430.0` | Default raw counts per gram |
| `CONFIG_MAX_WEIGHT_G` | `200000.0` | Full scale (4 × 50 000 g) |
| `CONFIG_ZERO_THRESHOLD_G` | `2.0` | Dead-band around zero (grams) |
| `CONFIG_WIFI_SSID` | `"YOUR_WIFI_SSID"` | Network name |
| `CONFIG_WIFI_PASSWORD` | `"YOUR_WIFI_PASSWORD"` | Network password |
| `CONFIG_WIFI_MAX_RETRIES` | `5` | Reconnection attempts |
| `CONFIG_WEBSERVER_PORT` | `80` | HTTP listen port |
| `CONFIG_SSE_PUSH_INTERVAL_MS` | `500` | SSE event interval |
| `CONFIG_NVS_NAMESPACE` | `"scale_cfg"` | NVS storage namespace |

---

## Architecture

### Task Diagram

```
app_main()
  │
  ├─ nvs_flash_init()
  ├─ wifi_manager_init() + wifi_manager_connect()
  ├─ hx711_init()
  ├─ calibration_load()   ← NVS
  ├─ hx711_tare()
  ├─ web_server_start()
  │     └─ httpd (internal tasks)
  │           ├─ GET  /              → dashboard HTML
  │           ├─ GET  /api/weight    → JSON
  │           ├─ POST /api/tare      → hx711_tare()
  │           ├─ POST /api/calibrate → hx711_set_scale()
  │           ├─ GET  /api/status    → JSON
  │           └─ GET  /events        → SSE stream (keeps socket open)
  │
  └─ xTaskCreate(task_measure)
        │
        └─ loop every 500 ms:
              hx711_get_weight()
              web_server_push_weight()  → SSE → browser
```

### Component Dependencies

```
main
 ├── hx711          (components/hx711)
 ├── calibration    (main/)
 ├── wifi_manager   (main/)
 ├── web_server     (main/)
 ├── esp_http_server
 ├── esp_wifi
 ├── esp_netif
 ├── nvs_flash
 ├── json  (cJSON)
 └── driver (GPIO)
```

---

## Troubleshooting

**Scale always reads 0 or a fixed value**
- Check DOUT and SCK wiring between the HX711 and ESP32-S3.
- Verify the HX711 VCC is at 3.3 V (not 5 V, which can damage GPIO inputs).
- Ensure a common GND between the HX711 and the ESP32-S3.
- Confirm load cells are wired as a full bridge (E+/E−/A+/A−).

**Readings are very noisy**
- Increase `CONFIG_SCALE_SAMPLES` in `scale_config.h` (try 20–30).
- Add 100 nF decoupling capacitors on the HX711 VCC pin.
- Route signal wires away from power lines and the USB cable.
- Check that the load cell frame is mechanically rigid.

**Wi-Fi does not connect**
- Double-check `CONFIG_WIFI_SSID` and `CONFIG_WIFI_PASSWORD`.
- The ESP32-S3 supports 2.4 GHz only — ensure your router broadcasts on that band.
- Increase `CONFIG_WIFI_MAX_RETRIES` if the environment is noisy.

**Dashboard does not open**
- Confirm the ESP32-S3 and your computer are on the same Wi-Fi subnet.
- Check the IP address printed in the serial monitor.
- Try port 80 explicitly: `http://<IP>:80/`.
- If port 80 is blocked, change `CONFIG_WEBSERVER_PORT` to 8080.

**SSE chart does not update**
- Modern browsers require a stable connection; reload the page once.
- Check for firewall rules blocking persistent HTTP connections.

**`calibration_load` returns `ESP_ERR_NVS_NOT_FOUND`**
- This is normal on the first boot. Run the calibration wizard to set values.
- The compile-time default `CONFIG_SCALE_FACTOR` is used until calibration runs.

**Build error: `cJSON not found`**
- Ensure `CONFIG_CJSON_ENABLE=y` is in `sdkconfig.defaults` (already set).
- Run `idf.py reconfigure` to regenerate the sdkconfig.

---

## Contributing

Contributions, bug reports, and feature requests are welcome!

1. Fork the repository.
2. Create a feature branch: `git checkout -b feature/my-improvement`.
3. Commit your changes with descriptive messages.
4. Ensure all functions have docstrings matching the existing style.
5. Open a Pull Request describing the change and test results.

---

## License

This project is released under the **MIT License**. See [LICENSE](LICENSE) for the full text.
