# ESP32-S3 FreeRTOS Event Groups Demo

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/framework-ESP--IDF-orange.svg)](https://github.com/espressif/esp-idf)

A comprehensive, production-ready demonstration of **FreeRTOS Event Groups** on ESP32-S3 using ESP-IDF. This project showcases real-world patterns for multi-task synchronization in embedded systems.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Event Bits Mapping](#event-bits-mapping)
- [Task Descriptions](#task-descriptions)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Expected Output](#expected-output)
- [How It Works](#how-it-works)
- [Key Concepts](#key-concepts)
- [Extending the Project](#extending-the-project)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## 🎯 Overview

Event Groups are a powerful FreeRTOS synchronization primitive that allows tasks to wait for multiple conditions simultaneously. Unlike semaphores or mutexes, Event Groups can manage up to 24 different boolean flags (event bits) in a single object, making them ideal for:

- **Startup barriers** - Ensuring all subsystems are ready before proceeding
- **Multi-source coordination** - Waiting for data from multiple sensors/sources
- **State machine synchronization** - Coordinating complex state transitions
- **Fault detection** - Using timeouts to detect missing or delayed events

This project demonstrates these patterns in a practical, ready-to-compile application.

## ✨ Features

### Demonstrated Patterns

1. **System Startup Barrier**
   - Waits for ADC, GPIO, I2C, and Network initialization
   - Uses AND logic to ensure all subsystems are ready

2. **OR vs AND Wait Logic**
   - **Diagnostics Task**: Wakes on ANY data-ready event (OR logic)
   - **Aggregator Task**: Waits for ALL data-ready events (AND logic)

3. **Real-World Sensor Tasks**
   - ADC sampling with periodic reads
   - Simulated I2C temperature sensor
   - GPIO debouncing and stability detection

4. **Timeout-Based Fault Detection**
   - Aggregator uses timeouts to detect missing data sources
   - Provides graceful handling of partial system failures

5. **Data Aggregation**
   - Combines multi-source sensor data into JSON payloads
   - Demonstrates proper separation of signaling (Event Groups) vs data transfer

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      app_main()                              │
│  Creates Event Group and spawns all tasks                    │
└───────────┬─────────────────────────────────────────────────┘
            │
            ├──────> init_task (Priority: 8)
            │        ├─> Initialize GPIO → Set EVT_GPIO_INIT
            │        ├─> Initialize ADC  → Set EVT_ADC_INIT
            │        ├─> Simulate I2C    → Set EVT_I2C_INIT
            │        └─> Simulate Network→ Set EVT_NET_INIT
            │
            ├──────> adc_task (Priority: 6)
            │        Wait EVT_ADC_INIT → Sample ADC → Set EVT_ADC_READY
            │
            ├──────> temp_task (Priority: 6)
            │        Wait EVT_I2C_INIT → Read Temp → Set EVT_TEMP_READY
            │
            ├──────> gpio_task (Priority: 6)
            │        Wait EVT_GPIO_INIT → Monitor GPIO → Set EVT_GPIO_READY
            │
            ├──────> aggregator_task (Priority: 7)
            │        Wait ALL_INIT → Wait ALL_DATA → Aggregate & Log
            │
            └──────> diagnostics_task (Priority: 5)
                     Wait ALL_INIT → Wait ANY_DATA → Log Activity
```

See [FLOWCHART.md](FLOWCHART.md) for a detailed Mermaid diagram of the complete system flow.

## 🔢 Event Bits Mapping

| Event Bit       | Bit Position | Hex Value | Purpose                          |
|-----------------|--------------|-----------|----------------------------------|
| EVT_ADC_INIT    | 0            | 0x0001    | ADC initialization complete      |
| EVT_GPIO_INIT   | 1            | 0x0002    | GPIO initialization complete     |
| EVT_I2C_INIT    | 2            | 0x0004    | I2C initialization complete      |
| EVT_NET_INIT    | 3            | 0x0008    | Network initialization complete  |
| EVT_ADC_READY   | 8            | 0x0100    | ADC data ready                   |
| EVT_TEMP_READY  | 9            | 0x0200    | Temperature data ready           |
| EVT_GPIO_READY  | 10           | 0x0400    | GPIO state stable                |

### Combined Masks

- **EVT_ALL_INIT_MASK** = `0x000F` (all initialization bits)
- **EVT_ALL_DATA_MASK** = `0x0700` (all data-ready bits)
- **EVT_ANY_DATA_MASK** = `0x0700` (same as ALL, used for OR waiting)

## 📝 Task Descriptions

### 1. Init Task (Priority: 8)
**Purpose**: Orchestrates system startup sequence

**Operations**:
- Initializes GPIO input (BOOT button by default)
- Configures ADC oneshot driver
- Simulates I2C bus initialization
- Simulates network stack initialization
- Sets corresponding event bits after each successful init
- Self-deletes after completion

**Key Functions**: `init_gpio_input()`, `init_adc_oneshot()`

---

### 2. ADC Task (Priority: 6)
**Purpose**: Periodic analog-to-digital conversion

**Operations**:
- Waits for `EVT_ADC_INIT` before starting
- Samples ADC channel every 500ms
- Stores raw value in `g_last_adc_raw`
- Sets `EVT_ADC_READY` bit after each successful read

**Hardware**: ADC1 Channel 0 (configurable)

---

### 3. Temperature Task (Priority: 6)
**Purpose**: Simulates I2C temperature sensor

**Operations**:
- Waits for `EVT_I2C_INIT` before starting
- Generates realistic temperature variation (22°C - 28°C)
- Updates every 1000ms
- Stores value in `g_last_temp_c`
- Sets `EVT_TEMP_READY` bit after each update

**Note**: In production, replace with actual I2C sensor driver (e.g., BME280, DS18B20)

---

### 4. GPIO Task (Priority: 6)
**Purpose**: Debounced GPIO monitoring

**Operations**:
- Waits for `EVT_GPIO_INIT` before starting
- Samples GPIO every 100ms
- Implements stability detection (3 consecutive identical reads)
- Stores level in `g_last_gpio_lvl`
- Sets `EVT_GPIO_READY` only when signal is stable

**Hardware**: GPIO0 (BOOT button) by default

---

### 5. Aggregator Task (Priority: 7)
**Purpose**: Multi-source data aggregation using AND logic

**Operations**:
- Waits for ALL initialization events (`EVT_ALL_INIT_MASK`)
- Blocks until ALL data sources are ready (`EVT_ALL_DATA_MASK`)
- Uses 3-second timeout for fault detection
- Clears event bits after successful read (`pdTRUE` auto-clear)
- Assembles JSON payload with timestamp
- Logs aggregated data

**Output Format**:
```json
{"ts_ms":123456,"adc":1876,"temp":24.50,"gpio":1}
```

---

### 6. Diagnostics Task (Priority: 5)
**Purpose**: System activity monitoring using OR logic

**Operations**:
- Waits for ALL initialization events
- Wakes when ANY data-ready bit is set (`pdFALSE` for OR logic)
- Does NOT clear event bits (allows other tasks to see them)
- Logs which subsystem(s) reported activity
- Uses 5-second timeout to detect system inactivity

**Use Case**: Real-time debugging, activity logging, watchdog feeding

## 🔧 Hardware Requirements

### Minimum Requirements
- **MCU**: ESP32-S3 development board (any variant)
- **USB Cable**: For programming and serial monitoring

### Optional Components
- **External ADC source**: Connect to GPIO1 (ADC1_CH0) for real voltage measurements
- **Temperature sensor**: I2C sensor (BME280, SHT31, etc.) for real temperature data
- **Additional GPIO**: For actual button/switch monitoring beyond BOOT button

### Default Pin Configuration
| Function | GPIO | Notes |
|----------|------|-------|
| GPIO Input | GPIO0 | BOOT button on most devkits |
| ADC Input | GPIO1 | ADC1 Channel 0 |

## 💻 Software Requirements

- **ESP-IDF**: v5.0 or newer (tested with v5.1+)
- **CMake**: v3.16 or newer (bundled with ESP-IDF)
- **Python**: v3.8+ (for ESP-IDF tools)

### Installation

If you haven't set up ESP-IDF yet:

```bash
# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install toolchain (Linux/macOS)
./install.sh esp32s3

# Install toolchain (Windows)
install.bat esp32s3

# Set up environment (run in each terminal session)
. ./export.sh  # Linux/macOS
export.bat     # Windows
```

For detailed instructions, see [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/).

## 🚀 Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/esp32s3-event-groups-demo.git
cd esp32s3-event-groups-demo
```

### 2. Set Target

```bash
idf.py set-target esp32s3
```

### 3. Configure (Optional)

```bash
idf.py menuconfig
```

Common configurations:
- Serial flasher config → Flash size
- Component config → FreeRTOS → Tick rate
- Component config → Log output → Default log level

### 4. Build

```bash
idf.py build
```

### 5. Flash and Monitor

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your serial port:
- Linux: `/dev/ttyUSB0`, `/dev/ttyACM0`
- macOS: `/dev/cu.usbserial-*`
- Windows: `COM3`, `COM4`, etc.

**Exit Monitor**: Press `Ctrl+]`

## ⚙️ Configuration

### Changing GPIO Pin

Edit `main/main.c`:

```c
#define DEMO_GPIO_INPUT GPIO_NUM_4  // Change from GPIO_NUM_0
```

### Changing ADC Channel

```c
#define DEMO_ADC_UNIT    ADC_UNIT_1
#define DEMO_ADC_CHANNEL ADC_CHANNEL_2  // GPIO3 on ESP32-S3
```

### Adjusting Task Priorities

```c
#define PRIO_INIT        8  // Highest priority for initialization
#define PRIO_ADC         6  // Data producer tasks
#define PRIO_TEMP        6
#define PRIO_GPIO        6
#define PRIO_AGGREGATOR  7  // Higher priority for aggregation
#define PRIO_DIAG        5  // Lower priority for diagnostics
```

### Modifying Timing

```c
// In adc_task:
vTaskDelay(pdMS_TO_TICKS(500));  // Change ADC sample rate

// In temp_task:
vTaskDelay(pdMS_TO_TICKS(1000));  // Change temp update rate

// In aggregator_task:
pdMS_TO_TICKS(3000)  // Change timeout duration
```

## 📊 Expected Output

### Successful Startup

```
I (XXX) EVT_GRP_DEMO: ADC ready
I (XXX) EVT_GRP_DEMO: Temp ready
I (XXX) EVT_GRP_DEMO: GPIO ready
I (XXX) EVT_GRP_DEMO: Payload: {"ts_ms":1234,"adc":1876,"temp":24.50,"gpio":1}
I (XXX) EVT_GRP_DEMO: ADC ready
I (XXX) EVT_GRP_DEMO: Payload: {"ts_ms":1734,"adc":1892,"temp":24.60,"gpio":1}
```

### Timeout Scenario

If a data source fails:

```
W (XXX) EVT_GRP_DEMO: Aggregator timeout waiting for data
```

This indicates not all event bits were set within the 3-second window.

## 🔍 How It Works

### Initialization Phase

1. `app_main()` creates the Event Group (`g_evt`)
2. All tasks are spawned simultaneously
3. `init_task` begins initialization sequence:
   - GPIO init → Sets `EVT_GPIO_INIT`
   - ADC init → Sets `EVT_ADC_INIT`
   - I2C init (simulated) → Sets `EVT_I2C_INIT`
   - Network init (simulated) → Sets `EVT_NET_INIT`
4. Producer tasks (`adc_task`, `temp_task`, `gpio_task`) block on their respective INIT bits
5. Consumer tasks (`aggregator_task`, `diagnostics_task`) block on `EVT_ALL_INIT_MASK`

### Operational Phase

**Producer Tasks**:
```c
// Wait for initialization
xEventGroupWaitBits(g_evt, EVT_ADC_INIT, pdFALSE, pdTRUE, portMAX_DELAY);

// Periodic operation
while (1) {
    // Acquire data
    g_last_adc_raw = read_adc();
    
    // Signal readiness
    xEventGroupSetBits(g_evt, EVT_ADC_READY);
    
    vTaskDelay(pdMS_TO_TICKS(500));
}
```

**Aggregator (AND Logic)**:
```c
// Wait for ALL data-ready bits, auto-clear them, with timeout
EventBits_t bits = xEventGroupWaitBits(
    g_evt,
    EVT_ALL_DATA_MASK,   // Wait for these bits
    pdTRUE,              // Clear bits on exit
    pdTRUE,              // Wait for ALL bits (AND)
    pdMS_TO_TICKS(3000)  // Timeout
);

if ((bits & EVT_ALL_DATA_MASK) == EVT_ALL_DATA_MASK) {
    // All data sources ready - process data
    aggregate_and_log();
}
```

**Diagnostics (OR Logic)**:
```c
// Wake on ANY data-ready bit, don't clear, with timeout
EventBits_t bits = xEventGroupWaitBits(
    g_evt,
    EVT_ANY_DATA_MASK,   // Wait for these bits
    pdFALSE,             // Don't clear bits
    pdFALSE,             // Wait for ANY bit (OR)
    pdMS_TO_TICKS(5000)  // Timeout
);

if (bits & EVT_ADC_READY)  log("ADC ready");
if (bits & EVT_TEMP_READY) log("Temp ready");
if (bits & EVT_GPIO_READY) log("GPIO ready");
```

## 💡 Key Concepts

### Event Groups vs Other Primitives

| Primitive | Best For | Event Groups Advantage |
|-----------|----------|----------------------|
| **Semaphores** | Binary signaling, resource counting | Multiple conditions simultaneously |
| **Mutexes** | Protecting shared resources | Non-blocking status checks |
| **Queues** | Data transfer | Lightweight state signaling |
| **Task Notifications** | Single task signaling | Multiple tasks can wait on same events |

### Important Design Principles

1. **Signaling, Not Data Transfer**
   - Event Groups signal *state* or *readiness*
   - Actual data lives in separate variables (protected if needed)
   - Use queues for data transfer between tasks

2. **Auto-Clear Behavior**
   - `pdTRUE` in `xClearOnExit`: Bits cleared automatically (prevents re-wake)
   - `pdFALSE` in `xClearOnExit`: Bits remain set (allows multiple readers)

3. **AND vs OR Logic**
   - `pdTRUE` in `xWaitForAllBits`: All specified bits must be set (AND)
   - `pdFALSE` in `xWaitForAllBits`: Any specified bit triggers wake (OR)

4. **Timeout Strategies**
   - `portMAX_DELAY`: Block forever (use for initialization barriers)
   - Finite timeout: Detect missing/delayed events (use for runtime monitoring)

5. **Bit Organization**
   - Group related events logically
   - Leave gaps for future expansion
   - Document bit meanings clearly

### Common Pitfalls

❌ **Mistake**: Using Event Groups for data transfer
```c
// DON'T DO THIS
xEventGroupSetBits(g_evt, temperature_value);  // Wrong!
```

✅ **Correct**: Separate signaling from data
```c
g_last_temp_c = temperature_value;  // Store data
xEventGroupSetBits(g_evt, EVT_TEMP_READY);  // Signal ready
```

---

❌ **Mistake**: Forgetting to clear bits in AND-wait scenarios
```c
// If auto-clear is pdFALSE, bits stay set forever
xEventGroupWaitBits(g_evt, EVT_ALL_DATA_MASK, pdFALSE, pdTRUE, timeout);
// Aggregator will wake immediately on next loop!
```

✅ **Correct**: Use auto-clear for repeating AND-waits
```c
xEventGroupWaitBits(g_evt, EVT_ALL_DATA_MASK, pdTRUE, pdTRUE, timeout);
```

---

❌ **Mistake**: Using Event Groups from ISRs
```c
void IRAM_ATTR gpio_isr_handler(void* arg) {
    xEventGroupSetBits(g_evt, EVT_GPIO_READY);  // NOT ISR-safe!
}
```

✅ **Correct**: Use `xEventGroupSetBitsFromISR()`
```c
void IRAM_ATTR gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(g_evt, EVT_GPIO_READY, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## 🛠️ Extending the Project

### Add Real I2C Temperature Sensor

Replace simulated `temp_task` with actual I2C driver:

```c
#include "driver/i2c_master.h"
#include "bme280.h"  // Example sensor driver

static void temp_task(void *arg) {
    xEventGroupWaitBits(g_evt, EVT_I2C_INIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    bme280_handle_t sensor = bme280_init();
    
    while (1) {
        float temp = bme280_read_temperature(sensor);
        g_last_temp_c = temp;
        xEventGroupSetBits(g_evt, EVT_TEMP_READY);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Add WiFi Connectivity

Replace simulated network initialization:

```c
#include "esp_wifi.h"
#include "esp_event.h"

static void init_task(void *arg) {
    // ... existing GPIO/ADC init ...
    
    // Real WiFi initialization
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_start();
    
    xEventGroupSetBits(g_evt, EVT_NET_INIT);
    vTaskDelete(NULL);
}
```

### Add MQTT Publishing

Use aggregated data in MQTT publish task:

```c
static void mqtt_task(void *arg) {
    xEventGroupWaitBits(g_evt, EVT_NET_INIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    esp_mqtt_client_handle_t client = mqtt_init();
    
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            g_evt, EVT_ALL_DATA_MASK, pdTRUE, pdTRUE, pdMS_TO_TICKS(3000));
        
        if ((bits & EVT_ALL_DATA_MASK) == EVT_ALL_DATA_MASK) {
            char payload[160];
            snprintf(payload, sizeof(payload),
                     "{\"adc\":%d,\"temp\":%.2f,\"gpio\":%d}",
                     g_last_adc_raw, g_last_temp_c, g_last_gpio_lvl);
            
            esp_mqtt_client_publish(client, "sensor/data", payload, 0, 1, 0);
        }
    }
}
```

### Add New Event Bits

Add battery monitoring:

```c
// In event bits section
#define EVT_BATTERY_INIT  (1U << 4)
#define EVT_BATTERY_READY (1U << 11)

// Update masks
#define EVT_ALL_INIT_MASK (EVT_ADC_INIT | EVT_GPIO_INIT | EVT_I2C_INIT | \
                           EVT_NET_INIT | EVT_BATTERY_INIT)
#define EVT_ALL_DATA_MASK (EVT_ADC_READY | EVT_TEMP_READY | EVT_GPIO_READY | \
                           EVT_BATTERY_READY)

// Add battery task
static void battery_task(void *arg) {
    xEventGroupWaitBits(g_evt, EVT_BATTERY_INIT, pdFALSE, pdTRUE, portMAX_DELAY);
    
    while (1) {
        float battery_voltage = read_battery_voltage();
        g_last_battery_v = battery_voltage;
        xEventGroupSetBits(g_evt, EVT_BATTERY_READY);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## 🐛 Troubleshooting

### Build Errors

**Error**: `fatal error: freertos/event_groups.h: No such file or directory`

**Solution**: Ensure ESP-IDF is properly installed and environment is sourced:
```bash
. $HOME/esp/esp-idf/export.sh
```

---

**Error**: `undefined reference to xEventGroupCreate`

**Solution**: Add FreeRTOS to component dependencies in `main/CMakeLists.txt`:
```cmake
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES freertos)
```

### Runtime Issues

**Issue**: Aggregator always times out

**Diagnosis**: Check which event bits are missing:
```c
EventBits_t current = xEventGroupGetBits(g_evt);
ESP_LOGI(APP_TAG, "Current bits: 0x%04X", current);
```

**Solution**: Ensure producer tasks are running and setting their bits

---

**Issue**: Task doesn't start after initialization

**Diagnosis**: Verify initialization bits are set:
```c
ESP_LOGI(APP_TAG, "Waiting for init bits: 0x%04X", EVT_ALL_INIT_MASK);
```

**Solution**: Check `init_task` completes successfully without errors

---

**Issue**: High CPU usage

**Diagnosis**: Tasks may be busy-looping

**Solution**: Ensure all tasks use `vTaskDelay()` and not tight loops

### Serial Monitor Issues

**Issue**: No output in monitor

**Solution**:
1. Check baud rate (default: 115200)
2. Verify correct serial port: `ls /dev/ttyUSB*` or `ls /dev/cu.*`
3. Check USB cable supports data (not just power)

**Issue**: Garbage characters

**Solution**: Reset board after flashing or adjust baud rate in `idf.py menuconfig` → Component config → ESP System Settings → UART console baud rate

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style

- Follow ESP-IDF coding conventions
- Use descriptive variable and function names
- Add comments for complex logic
- Update documentation for new features

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Espressif Systems for ESP-IDF framework
- FreeRTOS team for the excellent RTOS
- ESP32 community for documentation and support

## 📚 Additional Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [Event Groups API Reference](https://www.freertos.org/event-groups-API.html)

## 📧 Contact

For questions or feedback, please open an issue on GitHub.

---

**Happy Coding! 🚀**
