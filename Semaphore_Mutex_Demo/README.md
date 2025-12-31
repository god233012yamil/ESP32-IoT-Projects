# ESP-IDF FreeRTOS Synchronization Demo

[![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/framework-ESP--IDF-orange.svg)](https://github.com/espressif/esp-idf)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green.svg)](https://www.freertos.org/)
[![License](https://img.shields.io/badge/license-MIT--0-lightgrey.svg)](LICENSE)

A practical demonstration of FreeRTOS synchronization primitives on ESP32 using ESP-IDF. This project showcases three essential real-world synchronization patterns commonly used in embedded systems development.

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Synchronization Patterns](#-synchronization-patterns)
- [Hardware Requirements](#-hardware-requirements)
- [Software Requirements](#-software-requirements)
- [Project Structure](#-project-structure)
- [Installation and Setup](#-installation-and-setup)
- [Configuration](#-configuration)
- [Building and Flashing](#-building-and-flashing)
- [How It Works](#-how-it-works)
- [Expected Output](#-expected-output)
- [Flowchart](#-flowchart)
- [Troubleshooting](#-troubleshooting)
- [Learning Resources](#-learning-resources)
- [Contributing](#-contributing)
- [License](#-license)

## 🎯 Overview

This project demonstrates three fundamental synchronization patterns in FreeRTOS:

1. **Mutex** - Resource protection for shared I2C bus access
2. **Binary Semaphore** - ISR-to-task event notification via GPIO interrupt
3. **Counting Semaphore** - Resource pool management with limited concurrent access

Each pattern is implemented in a realistic embedded systems context, making this an excellent learning resource for understanding when and how to use different synchronization primitives.

## ✨ Features

- ✅ Complete working example with ESP-IDF and FreeRTOS
- ✅ Three synchronization patterns in a single demonstration
- ✅ Realistic use cases (I2C communication, GPIO interrupts, resource pools)
- ✅ Well-documented code with detailed comments
- ✅ Easy to modify and extend
- ✅ Minimal hardware requirements (just an ESP32 dev board)
- ✅ Comprehensive logging for understanding execution flow

## 🔄 Synchronization Patterns

### 1. Mutex (Mutual Exclusion)

**Use Case:** Protecting the shared I2C bus

Two tasks (`i2c_task_sensor` and `i2c_task_eeprom`) compete for access to a single I2C bus. The mutex ensures that only one task can communicate on the bus at a time, preventing race conditions and data corruption.

**Key Concepts:**
- Priority inheritance to prevent priority inversion
- Timeout-based acquisition for robustness
- Clear lock/unlock pattern

**Tasks Involved:**
- `i2c_task_sensor` - Simulates reading from a temperature sensor (0x48)
- `i2c_task_eeprom` - Simulates writing to an EEPROM (0x50)

### 2. Binary Semaphore

**Use Case:** ISR-to-task event notification

When the BOOT button (GPIO0) is pressed, a hardware interrupt is triggered. The ISR gives a binary semaphore to wake up a waiting task, which then processes the event. This pattern keeps ISR execution time minimal while allowing complex processing in task context.

**Key Concepts:**
- Minimal ISR execution time
- Deferred interrupt processing in task context
- Safe ISR-to-task communication
- Basic debouncing

**Components:**
- `gpio_isr_handler` - Fast interrupt service routine
- `gpio_event_task` - Task that processes button events

### 3. Counting Semaphore

**Use Case:** Resource pool management

Multiple worker tasks compete for a limited pool of resources (simulating buffers, hardware peripherals, or communication slots). The counting semaphore ensures that at most `BUFFER_POOL_SIZE` (3) tasks can hold resources simultaneously, while others wait.

**Key Concepts:**
- Limiting concurrent access to identical resources
- Fair resource distribution
- Timeout handling for resource unavailability

**Tasks Involved:**
- Five worker tasks competing for three resource slots

## 🛠️ Hardware Requirements

- **ESP32 Development Board** (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, etc.)
- **USB Cable** for programming and power
- **Optional:** I2C devices (sensor at 0x48, EEPROM at 0x50) if you want to test actual I2C communication

**Note:** The project works without physical I2C devices; I2C transactions will fail gracefully but synchronization patterns still demonstrate correctly.

## 💻 Software Requirements

- **ESP-IDF** v5.0 or later ([Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/))
- **Python** 3.8 or later (comes with ESP-IDF)
- **Git** (for cloning the repository)

## 📁 Project Structure

```
Semaphore_Mutex_Demo/
├── main/
│   ├── CMakeLists.txt          # Build configuration for main component
│   └── main.c                  # Main source file with all demo code
├── CMakeLists.txt              # Top-level build configuration
├── sdkconfig.defaults          # Default SDK configuration
├── LICENSE                     # MIT-0 License
└── README.md                   # This file
```

## 📥 Installation and Setup

### 1. Install ESP-IDF

Follow the official [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).

For quick reference:

```bash
# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install tools
./install.sh

# Set up environment variables
. ./export.sh
```

### 2. Clone This Repository

```bash
git clone https://github.com/god233012yamil/ESP32-IoT-Projects/tree/69a6a681dabe77456c0d2356aab738dbc6d8a6fd/Semaphore_Mutex_Demo.git
cd Semaphore_Mutex_Demo
```

### 3. Set Up ESP-IDF Environment

Every time you open a new terminal, source the ESP-IDF environment:

```bash
. $HOME/esp/esp-idf/export.sh
```

Or add an alias to your `.bashrc` or `.zshrc`:

```bash
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

## ⚙️ Configuration

### GPIO Configuration

The default GPIO configuration is:

```c
#define DEMO_GPIO_INPUT          GPIO_NUM_0   // BOOT button on most dev boards
```

### I2C Configuration

Default I2C pins:

```c
#define DEMO_I2C_PORT            I2C_NUM_0
#define DEMO_I2C_SDA_GPIO        GPIO_NUM_8
#define DEMO_I2C_SCL_GPIO        GPIO_NUM_9
#define DEMO_I2C_FREQ_HZ         100000
```

**⚠️ Important:** Many ESP32 dev boards use different default I2C pins. Common alternatives:
- SDA: GPIO21, GPIO8
- SCL: GPIO22, GPIO9

Modify these values in `main/main.c` to match your board's pinout.

### Resource Pool Configuration

```c
#define BUFFER_POOL_SIZE         3    // Number of available resource slots
#define WORKER_TASK_COUNT        5    // Number of worker tasks competing for slots
```

Adjust these values to experiment with different resource contention scenarios.

## 🔨 Building and Flashing

### Set Target Device

```bash
idf.py set-target esp32
```

For other ESP32 variants:
```bash
idf.py set-target esp32s2
idf.py set-target esp32s3
idf.py set-target esp32c3
```

### Build the Project

```bash
idf.py build
```

### Flash to Device

```bash
idf.py flash
```

### Monitor Serial Output

```bash
idf.py monitor
```

Or combine all steps:

```bash
idf.py build flash monitor
```

**Exit monitor:** Press `Ctrl+]`

## 🔍 How It Works

### Application Flow

1. **Initialization** (`app_main`)
   - Creates mutex for I2C bus protection
   - Creates binary semaphore for GPIO event signaling
   - Creates counting semaphore (initial count = 3) for resource pool
   - Initializes I2C driver in master mode
   - Configures GPIO0 with falling-edge interrupt
   - Creates and starts all tasks

2. **Mutex-Protected I2C Tasks**
   - Both tasks run in infinite loops with different periods
   - Each attempts to acquire the I2C mutex with a 500ms timeout
   - On success: performs I2C transaction, releases mutex
   - On timeout: logs warning and continues
   - **Sensor task:** Runs every 1 second, attempts to read from 0x48
   - **EEPROM task:** Runs every 2 seconds, attempts to write to 0x50

3. **GPIO Interrupt Handling**
   - When BOOT button pressed → GPIO interrupt fires
   - ISR executes (in IRAM for speed):
     - Gives binary semaphore
     - Optionally yields to higher-priority task
   - `gpio_event_task` wakes up:
     - Delays 40ms for debouncing
     - Reads and logs GPIO state
     - Blocks again waiting for next interrupt

4. **Resource Pool Management**
   - Five worker tasks attempt to acquire resource slots
   - Counting semaphore limits concurrent access to 3 tasks
   - Each worker:
     - Tries to take semaphore (1 second timeout)
     - If successful: simulates work for 600ms, releases slot
     - If timeout: logs warning
     - Delays before next attempt (staggered timing)

### Task Priorities

```c
I2C Sensor Task:    Priority 5
I2C EEPROM Task:    Priority 5
GPIO Event Task:    Priority 10 (highest - for responsive interrupt handling)
Worker Tasks:       Priority 4
```

### Synchronization Primitive Comparison

| Primitive | Purpose | Max Count | Priority Inheritance | ISR Safe (Give) |
|-----------|---------|-----------|---------------------|-----------------|
| **Mutex** | Resource protection | 1 (binary) | ✅ Yes | ❌ No |
| **Binary Semaphore** | Event signaling | 1 (binary) | ❌ No | ✅ Yes |
| **Counting Semaphore** | Resource pool | N (configurable) | ❌ No | ✅ Yes |

## 📊 Expected Output

### Sample Serial Monitor Output

```
I (xxx) sync_demo: Starting Semaphore vs Mutex demo
I (xxx) sync_demo: Tasks started. Press BOOT (GPIO0) to trigger GPIO semaphore.
I (xxx) sync_demo: I2C SENSOR: bus locked
I (xxx) sync_demo: I2C SENSOR: bus released
I (xxx) sync_demo: WORKER 0: acquired pool slot
I (xxx) sync_demo: WORKER 1: acquired pool slot
I (xxx) sync_demo: WORKER 2: acquired pool slot
W (xxx) sync_demo: WORKER 3: timed out waiting for pool slot
W (xxx) sync_demo: WORKER 4: timed out waiting for pool slot
I (xxx) sync_demo: I2C EEPROM: bus locked
I (xxx) sync_demo: I2C EEPROM: bus released
I (xxx) sync_demo: WORKER 0: releasing pool slot
I (xxx) sync_demo: WORKER 3: acquired pool slot

[Press BOOT button]
I (xxx) sync_demo: GPIO EVENT: ISR signaled (gpio=0 level=0)
```

### What to Observe

1. **I2C Mutex:** Sensor and EEPROM tasks never access the bus simultaneously
2. **GPIO Semaphore:** Pressing BOOT button immediately triggers the event task
3. **Counting Semaphore:** Maximum 3 workers holding slots at any time, others waiting or timing out

## 📈 Flowchart

See [FLOWCHART.md](FLOWCHART.md) for a detailed Mermaid diagram showing the complete program flow including initialization, task execution, and synchronization patterns.

## 🐛 Troubleshooting

### I2C Initialization Fails

**Problem:** Warning message: `I2C init failed (ESP_FAIL)`

**Solution:** 
- This is expected if no I2C devices are connected
- The mutex demo still works; only I2C transactions fail
- Connect actual I2C devices or ignore the warning
- Verify GPIO pins match your board's I2C pins

### GPIO Interrupt Not Triggering

**Problem:** No log messages when pressing BOOT button

**Solution:**
- Verify GPIO0 is the BOOT button on your board (check schematic)
- Some boards may use different pins
- Check that the button isn't being held during reset
- Modify `DEMO_GPIO_INPUT` to match your board

### Workers Always Timing Out

**Problem:** All workers show timeout messages

**Solution:**
- Increase timeout in `worker_task`: `pdMS_TO_TICKS(2000)` or higher
- Decrease work duration: change the 600ms delay
- Increase `BUFFER_POOL_SIZE` to reduce contention

### Build Errors

**Problem:** Compilation fails with missing headers or functions

**Solution:**
- Ensure ESP-IDF v5.0 or later: `idf.py --version`
- Clean and rebuild: `idf.py fullclean && idf.py build`
- Source ESP-IDF environment: `. $HOME/esp/esp-idf/export.sh`

## 📚 Learning Resources

### FreeRTOS Documentation
- [FreeRTOS Kernel Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
- [Mastering the FreeRTOS Real Time Kernel](https://www.freertos.org/fr-content-src/uploads/2018/07/161204_Mastering_the_FreeRTOS_Real_Time_Kernel-A_Hands-On_Tutorial_Guide.pdf)

### ESP-IDF Resources
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [ESP-IDF API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/)
- [I2C Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)

### Synchronization Primitives
- [When to Use Mutex vs Semaphore](https://www.freertos.org/FAQMutex.html)
- [Binary Semaphores](https://www.freertos.org/Embedded-RTOS-Binary-Semaphores.html)
- [Counting Semaphores](https://www.freertos.org/Real-time-embedded-RTOS-Counting-Semaphores.html)

## 🤝 Contributing

Contributions are welcome! Here are some ways you can contribute:

- 🐛 Report bugs or issues
- 💡 Suggest new synchronization patterns to demonstrate
- 📝 Improve documentation
- ✨ Add features (e.g., more realistic I2C communication, additional peripherals)
- 🧪 Add unit tests

Please feel free to submit issues or pull requests.

## 📄 License

This project is licensed under the **MIT No Attribution License (MIT-0)** - see the [LICENSE](LICENSE) file for details.

This is a permissive license that allows you to use, modify, and distribute this code without any attribution requirements. Perfect for learning and experimentation!

## 🙏 Acknowledgments

- Espressif Systems for ESP-IDF framework
- FreeRTOS community for excellent documentation
- The embedded systems community for best practices

---

**Happy coding! 🚀**

If you found this project helpful, please consider giving it a ⭐ on GitHub!
