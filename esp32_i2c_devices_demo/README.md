# ESP32 I2C Sensors, Displays, and EEPROM Demo

This ESP-IDF project accompanies the article "ESP32 I2C: Connecting Sensors, Displays, and EEPROMs with ESP-IDF."

It demonstrates:

- Creating an I2C master bus with the modern ESP-IDF driver
- Scanning all usable 7-bit I2C addresses
- Reading a TMP102 temperature sensor
- Initializing and updating a 128x64 SSD1306 OLED display
- Reading and writing a 24LC256-compatible EEPROM
- Handling absent devices without stopping the application
- Organizing reusable device drivers with documented APIs

## ESP-IDF Version

The project is designed for ESP-IDF 5.4 or later and uses:

```c
#include "driver/i2c_master.h"
```

## Default Wiring

| ESP32 | I2C Bus |
|---|---|
| GPIO21 | SDA |
| GPIO22 | SCL |
| 3.3 V | Device VCC |
| GND | Device GND |

Use one pull-up resistor from SDA to 3.3 V and one pull-up resistor from SCL to 3.3 V. A common starting value is 4.7 kohm.

The GPIO assignments can be changed with `idf.py menuconfig`.

## Default Device Addresses

| Device | Address |
|---|---|
| TMP102 | 0x48 |
| SSD1306 | 0x3C |
| 24LC256 | 0x50 |

Each demo can be disabled or assigned a different address in menuconfig.

## Build and Flash

```bash
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the serial port used by the ESP32 board.

For another ESP32 family member, replace `esp32` with the appropriate target, such as `esp32s3` or `esp32c6`, and update the GPIO assignments.

## Menu Configuration

Open:

```text
Component config
  ESP32 I2C Devices Demo Configuration
```

The project provides options for:

- I2C controller number
- SDA GPIO
- SCL GPIO
- Bus clock frequency
- Internal pull-up enable
- Device demo enable or disable
- Device addresses
- EEPROM test memory location

## Application Behavior

At startup, the application:

1. Initializes the I2C master bus.
2. Scans addresses 0x08 through 0x77.
3. Logs every detected target address.
4. Reads the TMP102 when detected.
5. Draws a test pattern on the SSD1306 when detected.
6. Performs a non-destructive EEPROM test when detected.
7. Reads and logs the TMP102 temperature every two seconds.

The EEPROM demo first saves the original bytes, writes a test pattern, verifies the readback, and restores the original bytes.

## Expected Serial Output

```text
I (...) i2c_bus: I2C bus initialized: port=0 SDA=21 SCL=22
I (...) i2c_scanner: Found device at 0x3C
I (...) i2c_scanner: Found device at 0x48
I (...) i2c_scanner: Found device at 0x50
I (...) app: TMP102 temperature: 24.81 C
I (...) app: SSD1306 demonstration pattern displayed
I (...) app: 24LC256 write and readback test passed
I (...) app: Original EEPROM bytes restored
```

## Hardware Notes

- ESP32 GPIO is normally 3.3 V. Do not allow external pull-ups to raise SDA or SCL to 5 V.
- Some breakout boards already include pull-up resistors. Parallel pull-ups reduce the effective resistance.
- The SSD1306 driver in this project targets common 128x64 modules using the internal charge pump.
- The EEPROM driver assumes a 24LC256-compatible device with 32 KiB capacity, 16-bit memory addressing, and 64-byte pages.
- The TMP102 driver reads the standard 12-bit temperature format.
