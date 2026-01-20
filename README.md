# ESP32 IoT Projects

A comprehensive collection of ESP32 IoT projects showcasing connectivity, peripherals, RTOS concepts, and real-world applications. Explore examples from networking to hardware control, designed to help developers learn, prototype, and build innovative embedded systems.

## Table of Contents

- [Overview](#overview)
- [Projects by Category](#projects-by-category)
  - [Wireless Communication](#wireless-communication)
  - [Networking & Time Synchronization](#networking--time-synchronization)
  - [Hardware Interfaces & Control](#hardware-interfaces--control)
  - [Storage & File Systems](#storage--file-systems)
  - [RTOS & Synchronization](#rtos--synchronization)
  - [Security & Configuration](#security--configuration)
- [Quick Reference Table](#quick-reference-table)
- [Getting Started](#getting-started)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)

## Overview

This repository contains **13 production-ready ESP32 projects** covering essential IoT development topics:

- **Wireless protocols** (WiFi, ESP-NOW)
- **Time synchronization** (NTP/SNTP)
- **Storage solutions** (SPI Flash, LittleFS)
- **Hardware interfaces** (UART, SPI, I2C, Touch, PWM)
- **RTOS concepts** (Tasks, Queues, Semaphores, Mutexes, Scheduling)
- **Security** (eFuse programming)
- **Real-world applications** (Internet clocks, WiFi analysis, touch displays)

Each project includes comprehensive documentation, error handling, and production-ready code patterns.

## Projects by Category

### Wireless Communication

#### ESP-NOW Demo
**Path:** `ESP-NOW_Demo`

Minimal, beginner-friendly demonstration of ESP-NOW peer-to-peer wireless communication protocol on ESP32-S3.

**Features:**
- Dual-role firmware (Sender/Receiver modes)
- FreeRTOS task-based architecture with callback-to-task handoff
- Broadcast and unicast addressing support
- Packet structure with version and sequence numbering
- Comprehensive logging and error handling

**Hardware:** 2x ESP32-S3 Development Boards

**Technology:** ESP-IDF v5.0+, FreeRTOS, ESP-NOW Protocol

---

### Networking & Time Synchronization

#### ESP32 NTP Client
**Path:** `ESP32_NTP_Client`

Simple and robust NTP (Network Time Protocol) client demonstrating WiFi connection, time synchronization, and display with timezone support.

**Features:**
- Automatic WiFi connection with retry logic
- SNTP protocol for accurate timekeeping
- Configurable POSIX timezone strings with DST handling
- Real-time formatted local time display (updates every second)
- Multiple NTP server support (pool.ntp.org, time.google.com, time.cloudflare.com)

**Hardware:** ESP32 development board, WiFi network with internet access

**Technology:** ESP-IDF v4.4+, WiFi, SNTP, FreeRTOS

#### ESP32 WiFi Scanner Application
**Path:** `ESP32_WiFi_Scanner_Application`

Comprehensive WiFi scanning application with both basic and advanced network analysis capabilities.

**Features:**

**Basic Scanner:**
- Periodic scanning with configurable intervals
- Comprehensive network display (SSID, BSSID, signal strength, channel, security)
- Signal quality indicators and security analysis

**Advanced Scanner:**
- 2.4GHz spectrum usage statistics
- OUI-based manufacturer/vendor identification
- Quality scoring (0-100 scale) with signal tracking
- Channel congestion and interference detection
- JSON output for machine-readable results
- Channel recommendations based on analysis

**Hardware:** ESP32 development board

**Technology:** ESP-IDF v4.4+, WiFi API, FreeRTOS

#### ESP32-C6 Touch LCD WiFi Internet Clock
**Path:** `ESP32C6 Touch LCD 1.47 WiFi Internet Clock`

Minimalist internet-connected clock with WiFi, NTP synchronization, and real-time display with automatic timezone and DST adjustment.

**Features:**
- Automatic WiFi connection with retry mechanism
- NTP time synchronization with multiple servers
- Auto-updating date/time display (refreshes every second)
- Configurable timezone with DST adjustment
- Custom scalable bitmap fonts (5×8 and 8×12)
- Landscape mode optimization (320×172)
- Connection and sync status display

**Hardware:** WaveShare ESP32-C6-Touch-LCD-1.47 board, WiFi network

**Technology:** ESP-IDF v5.4+, WiFi, SNTP, NTP, Custom fonts

---

### Hardware Interfaces & Control

#### ESP32-C6 Touch LCD Touch Demo
**Path:** `ESP32C6 Touch LCD 1.47 Touch Demo`

Clean, minimal touch screen demonstration for the WaveShare ESP32-C6-Touch-LCD-1.47 board featuring AXS5106 capacitive touch controller.

**Features:**
- Full AXS5106 capacitive touch controller integration with I2C (400 kHz)
- Real-time X/Y coordinate display with visual feedback
- Touch event logging to serial monitor
- Fast performance (<55ms touch response time)
- Low memory footprint (218 KB flash, 59 KB RAM)
- Supports up to 5 simultaneous touch points

**Hardware:** WaveShare ESP32-C6-Touch-LCD-1.47 board (1.47" IPS LCD, 172×320 pixels)

**Display Pins (SPI):** GPIO 1 (SCLK), GPIO 2 (MOSI), GPIO 14 (CS), GPIO 15 (DC), GPIO 22 (RST), GPIO 23 (BL)

**Touch Pins (I2C):** GPIO 18 (SDA), GPIO 19 (SCL), GPIO 21 (INT), GPIO 20 (RST)

**Technology:** ESP-IDF v5.1+, SPI, I2C, JD9853, AXS5106

#### LEDC PWM Demo
**Path:** `LEDC_PWM_Demo`

Demonstration of generating PWM (Pulse Width Modulation) signals using the LEDC (LED Control) peripheral for LED control, motor control, and servo control.

**Features:**
- Configurable PWM frequency (default: 5 kHz)
- Adjustable duty cycle with percentage input (0-100%)
- 13-bit resolution for precise control (8192 steps)
- Input validation with safety limits
- Comprehensive error handling
- Support for all ESP32 variants (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)

**Hardware:** ESP32-based development board, optional LED with resistor

**PWM Configuration:** 1 Hz to 40 MHz frequency, 1-20 bits resolution

**Technology:** ESP-IDF v4.4+, LEDC Peripheral, FreeRTOS

#### UART Demo
**Path:** `UART_Demo`

Production-ready, event-driven UART communication framework demonstrating best practices for handling UART communication in IoT applications.

**Features:**
- Event-driven architecture using ESP-IDF UART driver event queue
- Burst absorption with fast RX task and StreamBuffer
- Decoupled parsing (separate parser task)
- Serialized TX (single TX task prevents output interleaving)
- Overflow protection with graceful degradation
- Newline-delimited ASCII command protocol (easily customizable)

**Architecture:**
- **RX Event Task (Priority 12):** Monitors UART events, reads bytes, forwards to StreamBuffer
- **Parser Task (Priority 10):** Assembles commands, dispatches handlers
- **TX Task (Priority 10):** Exclusive UART writer, prevents interleaving

**Command Protocol:** `PING`, `VERSION`, `UPTIME` commands included

**Hardware:** ESP32-S3 (primary) or ESP32/ESP32-S2/ESP32-C3, optional USB-UART adapter

**Default Pins:** UART_NUM_1, TX: GPIO 17, RX: GPIO 18, Baud: 115200

**Technology:** ESP-IDF v4.4+, UART Driver, FreeRTOS Queues/StreamBuffers

---

### Storage & File Systems

#### ESP32 SPI Flash Demo
**Path:** `ESP32_SPI_Flash_Demo`

Comprehensive demonstration of interfacing with external SPI flash memory devices using ESP-IDF SPI Master driver.

**Features:**
- JEDEC ID reading for device identification
- Slow read (0x03) and fast read (0x0B) with dummy cycles
- DMA-friendly bulk reads for large data transfers
- Page programming (0x02) with automatic page boundary handling
- Sector erase (0x20) functionality
- Write enable/status polling for proper write/erase flow
- Memory safety with DMA-capable buffer allocation

**Hardware:** ESP32 board, SPI Flash Memory (W25Q32 or compatible W25Qxx series)

**Pin Configuration (Default):** GPIO23 (MOSI), GPIO19 (MISO), GPIO18 (SCLK), GPIO5 (CS)

**Technology:** ESP-IDF v5.0+, SPI Master Driver, DMA

#### ESP32-S3 LittleFS Demo
**Path:** `ESP32S3_LittleFS_Demo`

Comprehensive ESP-IDF project demonstrating LittleFS filesystem operations on ESP32-S3.

**Features:**
- Auto-format on mount failure
- Directory management with nested structure creation
- File operations (write, append, read)
- Directory listing with size information
- Real-time filesystem usage statistics
- Comprehensive error handling with detailed logging
- Periodic logging with timestamp append operations
- Wear leveling and power-loss resilience

**Hardware:** ESP32-S3 development board

**Partition Configuration:**
- NVS: 20KB
- PHY: 4KB
- Factory: 1MB
- LittleFS: 512KB

**Technology:** ESP-IDF v5.0+, LittleFS v1.20.0+, FreeRTOS

---

### RTOS & Synchronization

#### Preemptive vs Non-Preemptive Scheduling Demo
**Path:** `Preemptive_vs_Non_Preemptive_Scheduling_Demo`

Educational demonstration comparing FreeRTOS preemptive multitasking with cooperative run-to-completion event loop scheduling models.

**Features:**

**Preemptive Mode:**
- Multiple FreeRTOS tasks running concurrently with different priorities
- Task priority-based preemption
- Mutex-protected shared resources
- Demonstrates race condition prevention

**Cooperative Mode:**
- Single-threaded event loop
- Run-to-completion event handlers
- Queue-based event management
- Impact of blocking handlers demonstrated

**Configuration:** Three FreeRTOS tasks (sensor, network, high-priority), selectable via menuconfig

**Hardware:** ESP32 development board

**Technology:** ESP-IDF v5.0+, FreeRTOS, Event Queues, Mutexes

#### Queue Producer-Consumer Demo
**Path:** `QUEUE_Producer_Consumer_Demo`

Clean, beginner-friendly demonstration of the producer-consumer pattern using FreeRTOS queues for inter-task communication.

**Features:**
- FreeRTOS queue-based inter-task communication
- Structured message passing with sequence numbers and timestamps
- Timeout handling for send and receive operations
- Queue overflow detection and logging
- Well-documented code with descriptive comments
- Producer generates messages periodically
- Consumer processes messages with age tracking

**Configuration:** 8-message queue, 1000ms producer period, configurable timeouts

**Hardware:** ESP32 development board

**Technology:** ESP-IDF v4.4+, FreeRTOS Queues

#### Semaphore & Mutex Demo
**Path:** `Semaphore_Mutex_Demo`

Practical demonstration of FreeRTOS synchronization primitives including mutex, binary semaphore, and counting semaphore with real-world use cases.

**Features:**

**Mutex (Mutual Exclusion):**
- Protects shared I2C bus access
- Two tasks (sensor and EEPROM) competing for bus
- Priority inheritance to prevent priority inversion

**Binary Semaphore:**
- ISR-to-task event notification via GPIO interrupt
- BOOT button (GPIO0) triggers interrupt
- Minimal ISR execution time
- Safe interrupt-to-task communication

**Counting Semaphore:**
- Resource pool management
- Limited concurrent access (3 resource slots)
- Five worker tasks competing for slots
- Fair resource distribution

**Hardware:** ESP32 Development Board, optional I2C devices at 0x48 and 0x50

**GPIO Configuration:** GPIO0 (BOOT button), GPIO8/GPIO9 (I2C SDA/SCL)

**Technology:** ESP-IDF v5.0+, FreeRTOS Synchronization Primitives, I2C, GPIO Interrupts

---

### Security & Configuration

#### ESP32-S3 Custom eFuse Generation Demo
**Path:** `ESP32S3_Custom_Efuse_Gen_Demo`

Comprehensive demonstration of defining, generating, and using custom eFuse fields on ESP32-S3 with build-time code generation and safe virtual testing.

**Features:**
- Build-time eFuse table code generation from CSV definitions
- Custom field definitions (serial number, hardware revision, feature flags)
- CRC-16/CCITT-FALSE validation for data integrity
- Batch write mode for Reed-Solomon encoded user blocks
- Virtual eFuse support for safe testing without permanent hardware changes
- Complete API example for reading and writing custom fields
- Production-ready pattern for manufacturing workflows

**Custom eFuse Fields:**
- `USER_DATA.SERIAL_NUMBER` (128 bits - device serial)
- `USER_DATA.HW_REV` (16 bits - hardware revision)
- `USER_DATA.FEATURE_FLAGS` (32 bits - feature flags)
- `USER_DATA.PROVISIONING_CRC16` (16 bits - CRC validation)

**⚠️ Warning:** eFuses are one-time programmable and irreversible! Always test with virtual eFuses first.

**Hardware:** ESP32-S3 Development Board

**Technology:** ESP-IDF v5.0+, eFuse API, CRC-16

---

## Quick Reference Table

| Project | Category | ESP32 Variant | Key Technology | Difficulty |
|---------|----------|---------------|----------------|------------|
| [ESP-NOW Demo](#esp-now-demo) | Wireless | ESP32-S3 | ESP-NOW, FreeRTOS | Beginner |
| [NTP Client](#esp32-ntp-client) | Networking | ESP32 | WiFi, SNTP | Beginner |
| [WiFi Scanner](#esp32-wifi-scanner-application) | Networking | ESP32 | WiFi API, Advanced | Intermediate |
| [WiFi Internet Clock](#esp32-c6-touch-lcd-wifi-internet-clock) | Networking/Display | ESP32-C6 | WiFi, NTP, Display | Intermediate |
| [Touch LCD Demo](#esp32-c6-touch-lcd-touch-demo) | Hardware | ESP32-C6 | I2C, SPI, Touch | Beginner |
| [LEDC PWM Demo](#ledc-pwm-demo) | Hardware | All ESP32 | LEDC, PWM | Beginner |
| [UART Demo](#uart-demo) | Communication | ESP32-S3 | UART, Event-driven | Advanced |
| [SPI Flash Demo](#esp32-spi-flash-demo) | Storage | ESP32 | SPI Master, DMA | Intermediate |
| [LittleFS Demo](#esp32-s3-littlefs-demo) | Storage | ESP32-S3 | LittleFS, File I/O | Intermediate |
| [Scheduling Demo](#preemptive-vs-non-preemptive-scheduling-demo) | RTOS | ESP32 | FreeRTOS, Scheduling | Advanced |
| [Queue Demo](#queue-producer-consumer-demo) | RTOS | ESP32 | FreeRTOS Queues | Beginner |
| [Semaphore/Mutex Demo](#semaphore--mutex-demo) | RTOS | ESP32 | Synchronization | Intermediate |
| [Custom eFuse Demo](#esp32-s3-custom-efuse-generation-demo) | Security | ESP32-S3 | eFuse, CRC | Advanced |

## Getting Started

### Prerequisites

Before you begin, ensure you have the following installed:

1. **ESP-IDF**: Version 4.4 or higher (specific projects may require v5.0+)
   - [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. **Python**: Version 3.8 or higher
3. **Git**: For cloning the repository
4. **USB Drivers**: CP210x or CH340 drivers for your ESP32 board

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/god233012yamil/ESP32-IoT-Projects.git
   cd ESP32-IoT-Projects
   ```

2. **Set up ESP-IDF environment:**
   ```bash
   # For bash/zsh
   . $HOME/esp/esp-idf/export.sh

   # For fish shell
   . $HOME/esp/esp-idf/export.fish
   ```

3. **Navigate to a project:**
   ```bash
   cd ESP32_NTP_Client
   ```

4. **Configure the project (if needed):**
   ```bash
   idf.py menuconfig
   ```
   - Set WiFi credentials (for networking projects)
   - Configure GPIO pins
   - Adjust project-specific settings

5. **Build the project:**
   ```bash
   idf.py build
   ```

6. **Flash to your ESP32:**
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   - Replace `/dev/ttyUSB0` with your serial port (e.g., `COM3` on Windows)
   - Use `Ctrl+]` to exit the monitor

### Quick Start Example (NTP Client)

```bash
cd ESP32_NTP_Client
idf.py menuconfig  # Set WiFi SSID and password
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Hardware Requirements

### Common Requirements
- **ESP32 Development Board** (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6 depending on project)
- **USB Cable** (Type-C or Micro-USB depending on board)
- **Computer** with USB port running Windows, macOS, or Linux

### Project-Specific Hardware

| Project | Additional Hardware |
|---------|---------------------|
| ESP-NOW Demo | 2x ESP32-S3 boards |
| Touch LCD Demo | WaveShare ESP32-C6-Touch-LCD-1.47 board |
| WiFi Internet Clock | WaveShare ESP32-C6-Touch-LCD-1.47 board |
| SPI Flash Demo | W25Q32 or compatible SPI flash memory, breadboard, jumper wires |
| LEDC PWM Demo | Optional: LED with current-limiting resistor, oscilloscope |
| UART Demo | Optional: USB-UART adapter for external testing |
| Semaphore/Mutex Demo | Optional: I2C devices at addresses 0x48 and 0x50 |

### Power Supply
Most projects work with USB power (5V). Ensure your USB cable supports both data and power.

## Software Requirements

### Required
- **ESP-IDF**: v4.4 or higher (check individual projects for specific versions)
- **Python**: v3.8 or higher
- **CMake**: v3.16 or higher (included with ESP-IDF)
- **Ninja Build**: Included with ESP-IDF

### Recommended
- **Serial Terminal**: PuTTY, minicom, or use built-in `idf.py monitor`
- **Git**: For version control
- **VS Code**: With ESP-IDF extension for improved development experience
- **Logic Analyzer**: For debugging hardware interfaces (optional)

### For Windows Users
- Install ESP-IDF using the [Windows Installer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html)

### For macOS/Linux Users
- Follow the [Standard Toolchain Setup](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)

## Project Structure

```
ESP32-IoT-Projects/
├── ESP-NOW_Demo/                           # Wireless P2P communication
├── ESP32C6 Touch LCD 1.47 Touch Demo/     # Touch input demonstration
├── ESP32C6 Touch LCD 1.47 WiFi Internet Clock/  # Internet-connected clock
├── ESP32S3_Custom_Efuse_Gen_Demo/         # eFuse programming
├── ESP32S3_LittleFS_Demo/                 # Filesystem operations
├── ESP32_NTP_Client/                       # Time synchronization
├── ESP32_SPI_Flash_Demo/                   # External flash memory
├── ESP32_WiFi_Scanner_Application/         # Network analysis
├── LEDC_PWM_Demo/                          # PWM signal generation
├── Preemptive_vs_Non_Preemptive_Scheduling_Demo/  # Scheduling models
├── QUEUE_Producer_Consumer_Demo/           # Inter-task messaging
├── Semaphore_Mutex_Demo/                   # Synchronization primitives
├── UART_Demo/                              # Serial communication
├── LICENSE                                 # MIT License
└── README.md                               # This file
```

Each project contains:
- `main/` - Source code
- `CMakeLists.txt` - Build configuration
- `README.md` or documentation (in most projects)
- `sdkconfig.defaults` - Default configuration (where applicable)

## Contributing

Contributions are welcome! Whether you want to add new projects, improve existing ones, or fix bugs:

1. **Fork** this repository
2. **Create** a feature branch (`git checkout -b feature/AmazingProject`)
3. **Commit** your changes (`git commit -m 'Add some AmazingProject'`)
4. **Push** to the branch (`git push origin feature/AmazingProject`)
5. **Open** a Pull Request

### Contribution Guidelines
- Follow ESP-IDF coding style
- Include comprehensive comments
- Add README documentation for new projects
- Test on actual hardware before submitting
- Include pin configurations and hardware requirements
- Add error handling and logging

### Ideas for New Projects
- BLE communication examples
- MQTT client implementations
- Web server with RESTful API
- OTA (Over-The-Air) update examples
- Sensor integration projects
- Low-power and deep sleep examples

## License

This repository is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

---

## Resources

### Official Documentation
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)

### Community
- [ESP32 Forum](https://www.esp32.com/)
- [ESP-IDF GitHub](https://github.com/espressif/esp-idf)
- [Espressif Systems](https://www.espressif.com/)

### Related Repositories
- [Espressif IoT Solution](https://github.com/espressif/esp-iot-solution)
- [ESP-IDF Examples](https://github.com/espressif/esp-idf/tree/master/examples)

---

**⭐ If you find this repository helpful, please consider giving it a star!**

**📧 Questions or suggestions?** Feel free to open an issue or start a discussion.

**🔧 Built with ESP-IDF | Powered by ESP32**
