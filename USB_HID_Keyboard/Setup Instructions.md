## Follow These Steps

### Step 1: Create the idf_component.yml file

Create a file named `idf_component.yml` in your `main` directory with this content:

```yaml
dependencies:
  espressif/esp_tinyusb:
    version: "~1.4.4"
    
  idf:
    version: ">=5.0.0"
```

### Step 2: Update main/CMakeLists.txt

Your `main/CMakeLists.txt` should look like this:

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_system
        driver
        freertos
        log
        esp_tinyusb
)
```

### Step 3: Project Structure

Your project should have this structure:

```
USB_HID_Keyboard/
├── CMakeLists.txt
├── sdkconfig.defaults
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml          ← ADD THIS FILE
    └── main.c
```

### Step 4: Clean and Rebuild

```bash
# Clean the build
idf.py fullclean

# Set target
idf.py set-target esp32s3

# Build (this will download esp_tinyusb automatically)
idf.py build
```

### Step 5: First build will download dependencies

When you run `idf.py build` for the first time, you'll see:

```
Processing 2 dependencies:
[1/2] espressif/esp_tinyusb (1.4.4)
[2/2] idf (5.0.0)
```

This is normal! ESP-IDF is downloading the TinyUSB component from the registry.

## Alternative: Manual Component Installation

If the automatic download doesn't work, you can add the component manually:

```bash
# Navigate to your project directory
cd USB_HID_Keyboard

# Add the component
idf.py add-dependency "espressif/esp_tinyusb~1.4.4"

# This will create the idf_component.yml file automatically
```

## Complete File Contents

### File 1: `CMakeLists.txt` (root directory)

```cmake
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(usb_hid_keyboard)
```

### File 2: `main/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_system
        driver
        freertos
        log
        esp_tinyusb
)
```

### File 3: `main/idf_component.yml`

```yaml
dependencies:
  espressif/esp_tinyusb:
    version: "~1.4.4"
    
  idf:
    version: ">=5.0.0"
```

### File 4: `sdkconfig.defaults`

```ini
# USB Configuration
CONFIG_TINYUSB_ENABLED=y

# USB Device Configuration
CONFIG_TINYUSB_DESC_USE_ESPRESSIF_VID=y
CONFIG_TINYUSB_DESC_CUSTOM_PID=0x4001

# FreeRTOS Configuration
CONFIG_FREERTOS_HZ=1000

# ESP32-S3 Specific - USB Console
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

## Verify Setup

After following these steps, run:

```bash
idf.py reconfigure
idf.py build
```

You should see a successful build with output like:

```
Project build complete. To flash, run:
idf.py -p PORT flash
```

## Troubleshooting

### Error: "Could not find component espressif/esp_tinyusb"

**Solution:** Check your internet connection. ESP-IDF needs to download the component from the registry.

### Error: "Failed to resolve component 'esp_tinyusb'"

**Solution:** 
1. Delete the `managed_components` folder if it exists
2. Run `idf.py fullclean`
3. Run `idf.py reconfigure`

### Download is slow or times out

**Solution:** 
1. Check firewall settings
2. Try: `idf.py --no-ccache reconfigure`
3. Or manually download from: https://components.espressif.com/components/espressif/esp_tinyusb

## Build Commands Summary

```bash
# Complete clean build sequence
idf.py fullclean
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
idf.py -p COM3 flash monitor  # Replace COM3 with your port
```

## What Changed in ESP-IDF v5.x?

1. **TinyUSB is now a managed component** - Must be declared in `idf_component.yml`
2. **Component name changed** - `tinyusb` → `esp_tinyusb`
3. **API changes** - Some function names have changed
4. **USB console** - New options for USB Serial JTAG

## Next Steps

After successful build:
1. Flash the firmware: `idf.py -p PORT flash`
2. Monitor output: `idf.py -p PORT monitor`
3. Press the boot button to trigger automation

## Need Help?

If you still have issues:
1. Check ESP-IDF version: `idf.py --version` (should be 5.x)
2. Verify internet connection
3. Try: `idf.py add-dependency "espressif/esp_tinyusb"`
4. Check component registry: https://components.espressif.com/