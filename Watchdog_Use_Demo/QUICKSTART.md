# Quick Start Guide

This guide will help you get the ESP32 TWDT Demo up and running quickly.

## Prerequisites

### 1. Hardware
- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or compatible board
- USB cable for programming and serial communication

### 2. Software
- **ESP-IDF v4.4 or later** (v5.x recommended)
- **Python 3.7+**
- **Git**

## Installation Steps

### Step 1: Install ESP-IDF

If you haven't already installed ESP-IDF, follow the official guide:

**Linux/macOS:**
```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
. ./export.sh
```

**Windows:**
Use the [ESP-IDF Windows Installer](https://dl.espressif.com/dl/esp-idf/)

### Step 2: Clone This Repository

```bash
cd ~/esp  # or your preferred directory
git clone https://github.com/YOUR_USERNAME/esp32-twdt-demo.git
cd esp32-twdt-demo
```

### Step 3: Configure the Project

```bash
# Set up ESP-IDF environment (if not already done)
. $HOME/esp/esp-idf/export.sh

# Configure project (optional - defaults are already set)
idf.py menuconfig
```

**Important:** Ensure stack overflow checking is enabled:
- Navigate: `Component config → FreeRTOS → Kernel → Check for stack overflow`
- Select: **Method 2**

### Step 4: Build the Project

```bash
idf.py build
```

This will:
- Configure the project based on sdkconfig.defaults
- Compile all source files
- Generate firmware binaries

### Step 5: Connect Your ESP32

1. Connect your ESP32 board via USB
2. Identify the serial port:
   - **Linux**: Usually `/dev/ttyUSB0` or `/dev/ttyACM0`
   - **macOS**: Usually `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
   - **Windows**: Usually `COM3`, `COM4`, etc.

Check available ports:
```bash
# Linux/macOS
ls /dev/tty*

# Windows (in PowerShell)
Get-WmiObject -query "SELECT * FROM Win32_PnPEntity" | Where-Object {$_.Name -match "COM\d+"}
```

### Step 6: Flash and Monitor

```bash
# Replace /dev/ttyUSB0 with your actual port
idf.py -p /dev/ttyUSB0 flash monitor
```

**Tip:** You can also flash and monitor in one command as shown above.

To exit monitor mode: Press `Ctrl+]`

## What You Should See

### Initial Output (First 1-2 seconds)
```
I (xxx) DAY27_WDT: Tasks started. Expect TWDT events and a stack overflow demo soon.
I (xxx) DAY27_WDT: [Healthy] feeding TWDT
I (xxx) DAY27_WDT: [Flaky] cycle 0: feeding TWDT (1/3)
W (xxx) DAY27_WDT: [Stuck] will block forever without feeding TWDT...
I (xxx) DAY27_WDT: [TinyStack] starting with very small stack; will chew stack...
```

### Stack Overflow (Within ~1 second)
```
E (xxx) DAY27_WDT: Stack overflow detected in task: TinyStackTask
abort() was called at PC 0x400xxxxx on core 0
```

The system will reset at this point. If you want to see the watchdog timeout instead, you can comment out the tiny-stack task creation in `main.c`.

### TWDT Timeout (After ~5 seconds, if stack overflow is disabled)
```
E (xxx) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
E (xxx) task_wdt:  - StuckTask (CPU 0)
E (xxx) task_wdt: Tasks currently running:
E (xxx) task_wdt: CPU 0: IDLE0
E (xxx) task_wdt: Aborting.
```

## Customization Options

### Adjust TWDT Timeout

Edit `main.c`, line ~216:
```c
static const esp_task_wdt_config_t twdt_cfg = {
    .timeout_ms = 5000,  // Change this value (in milliseconds)
    .trigger_panic = true,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
};
```

### Disable Specific Tasks

Comment out task creation in `app_main()` (lines 235-241):
```c
// xTaskCreate(stuck_task, "StuckTask", 2048, NULL, 5, NULL);  // Disable stuck task
```

### Modify Task Behavior

Each task function is clearly documented. Modify timing, loops, or behavior as needed for your experiments.

## Troubleshooting

### Build Errors

**Error: `esp_task_wdt.h: No such file or directory`**
- Ensure ESP-IDF is properly installed and environment is sourced
- Run: `. $HOME/esp/esp-idf/export.sh`

**Error: `menuconfig not found`**
- Install required Python packages: `pip install -r $IDF_PATH/requirements.txt`

### Flash Errors

**Error: `Serial port not found`**
- Check USB connection
- Verify port name with `ls /dev/tty*`
- Install USB-to-UART drivers (CP210x or CH340 depending on your board)

**Error: `Permission denied`**
- Linux: Add user to dialout group: `sudo usermod -a -G dialout $USER`
- Log out and log back in

**Error: `Device not in download mode`**
- Hold BOOT button while pressing RESET button
- Some boards enter download mode automatically

### Runtime Issues

**No Serial Output**
- Check baud rate (should be 115200)
- Try different serial terminal or `idf.py monitor`
- Press RESET button on board

**Tasks Don't Behave As Expected**
- Verify stack overflow checking is enabled in menuconfig
- Check TWDT timeout value (should be 5 seconds)
- Review serial output for error messages

## Next Steps

- Read the full [README.md](README.md) for detailed documentation
- Experiment with different task behaviors
- Modify timeout values and observe effects
- Integrate watchdog concepts into your own projects

## Getting Help

- Check the [Troubleshooting](#troubleshooting) section
- Review [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/)
- Open an issue on GitHub
- Join ESP32 community forums

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Task Watchdog Timer API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/RTOS_book.html)
