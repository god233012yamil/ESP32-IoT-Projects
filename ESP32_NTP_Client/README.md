# ESP32 NTP Client

A simple and robust ESP32 NTP (Network Time Protocol) client implementation using ESP-IDF and FreeRTOS. This project demonstrates how to connect to Wi-Fi, synchronize time with NTP servers, and display the current local time with timezone support.

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Configuration](#configuration)
  - [Timezone Examples](#timezone-examples)
- [Building and Flashing](#building-and-flashing)
- [Usage](#usage)
  - [Sample Output](#sample-output)
- [Code Structure](#code-structure)
  - [Key Functions](#key-functions)
- [Customization](#customization)
  - [Change Time Format](#change-time-format)
  - [Add More NTP Servers](#add-more-ntp-servers)
  - [Change Print Interval](#change-print-interval)
- [Troubleshooting](#troubleshooting)
  - [Wi-Fi Connection Issues](#wi-fi-connection-issues)
  - [Time Sync Issues](#time-sync-issues)
  - [Build Errors](#build-errors)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)
- [Additional Resources](#additional-resources)

## Features

- **Wi-Fi Station Mode**: Automatic connection with retry logic
- **NTP Time Synchronization**: Uses SNTP protocol for accurate timekeeping
- **Timezone Support**: Configurable POSIX timezone strings with DST handling
- **Real-time Display**: Prints formatted local time every second
- **Event-driven Architecture**: Uses FreeRTOS event groups for synchronization
- **Robust Error Handling**: Comprehensive timeout and retry mechanisms
- **Configurable Settings**: Wi-Fi credentials and timezone via menuconfig

## Hardware Requirements

- ESP32 development board (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, etc.)
- USB cable for programming and power
- Wi-Fi network access

## Software Requirements

- [ESP-IDF](https://github.com/espressif/esp-idf) v4.4 or later
- Python 3.6+ (for ESP-IDF toolchain)

## Installation

1. **Clone this repository:**
   ```bash
   git clone https://github.com/yourusername/esp32-ntp-client.git
   cd esp32-ntp-client
   ```

2. **Set up ESP-IDF environment:**
   ```bash
   # If ESP-IDF is not installed, follow the official installation guide
   # https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
   
   # Source the ESP-IDF environment
   . $HOME/esp/esp-idf/export.sh
   ```

## Configuration

Configure the project settings using ESP-IDF's menuconfig:

```bash
idf.py menuconfig
```

Navigate to **Example Configuration** and set:

- **WiFi SSID**: Your Wi-Fi network name
- **WiFi Password**: Your Wi-Fi network password  
- **Timezone**: POSIX timezone string (see examples below)

### Timezone Examples

| Location | POSIX TZ String |
|----------|-----------------|
| UTC | `UTC0` |
| US Eastern (with DST) | `EST5EDT,M3.2.0/2,M11.1.0/2` |
| US Pacific (with DST) | `PST8PDT,M3.2.0/2,M11.1.0/2` |
| Central Europe (with DST) | `CET-1CEST,M3.5.0,M10.5.0/3` |
| Japan Standard Time | `JST-9` |
| Australian Eastern (with DST) | `AEST-10AEDT,M10.1.0,M4.1.0/3` |

## Building and Flashing

1. **Build the project:**
   ```bash
   idf.py build
   ```

2. **Flash to ESP32:**
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```
   Replace `/dev/ttyUSB0` with your ESP32's serial port.

3. **Monitor output:**
   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   ```

## Usage

Once flashed and running, the ESP32 will:

1. **Initialize** NVS flash and create the default event loop
2. **Connect to Wi-Fi** using the configured credentials
3. **Synchronize time** with NTP servers (default: pool.ntp.org)
4. **Display time** in 12-hour format with AM/PM every second

### Sample Output

```
I (2543) NTP_APP: Wi-Fi STA started, connecting to SSID:"YourWiFiNetwork"
I (4321) NTP_APP: Got IP: 192.168.1.100
I (4325) NTP_APP: Connected to AP
I (4330) NTP_APP: SNTP started, waiting for time sync...
I (6789) NTP_APP: Time synchronization event received
I (6790) NTP_APP: Time is synchronized: 2024-01-15 14:30:25
I (6795) NTP_APP: Timezone set to: EST5EDT,M3.2.0/2,M11.1.0/2
[TIME] 2024-01-15 02:30:26 PM EST
[TIME] 2024-01-15 02:30:27 PM EST
[TIME] 2024-01-15 02:30:28 PM EST
```

## Code Structure

```
esp32-ntp-client/
├── main/
│   └── main.c              # Main application code
├── CMakeLists.txt          # ESP-IDF build configuration
├── Kconfig.projbuild       # Project configuration options
├── README.md               # This file
└── sdkconfig              # Generated configuration file
```

### Key Functions

- **`wifi_init_and_wait_ip()`**: Initializes Wi-Fi and waits for connection
- **`sntp_start_and_wait()`**: Configures SNTP and waits for time sync
- **`print_time_task()`**: FreeRTOS task that prints time every second
- **`wifi_event_handler()`**: Handles Wi-Fi connection events

## Customization

### Change Time Format
Modify the `strftime()` format string in `print_time_task()`:
```c
// 24-hour format
strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &local);

// 12-hour format with AM/PM (current)
strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S %p %Z", &local);
```

### Add More NTP Servers
In `sntp_start_and_wait()`, add additional servers:
```c
esp_sntp_setservername(0, "pool.ntp.org");
esp_sntp_setservername(1, "time.google.com");
esp_sntp_setservername(2, "time.cloudflare.com");
```

### Change Print Interval
Modify the delay in `print_time_task()`:
```c
vTaskDelay(pdMS_TO_TICKS(5000)); // Print every 5 seconds
```

## Troubleshooting

### Wi-Fi Connection Issues
- Verify SSID and password in menuconfig
- Check Wi-Fi signal strength
- Ensure WPA2/WPA3 compatibility settings

### Time Sync Issues  
- Check internet connectivity
- Try different NTP servers
- Increase sync timeout in `sntp_start_and_wait()`

### Build Errors
- Ensure ESP-IDF is properly installed and sourced
- Check ESP-IDF version compatibility
- Clean build: `idf.py fullclean && idf.py build`

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make your changes and commit: `git commit -am 'Add feature'`
4. Push to the branch: `git push origin feature-name`
5. Submit a pull request

## License

This project is licensed under the [MIT License](LICENSE).

## Acknowledgments

- [ESP-IDF](https://github.com/espressif/esp-idf) by Espressif Systems
- NTP Pool Project for providing free NTP servers
- FreeRTOS for the real-time operating system

## Additional Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [SNTP Protocol Specification (RFC 4330)](https://tools.ietf.org/html/rfc4330)
- [POSIX Timezone Specification](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)