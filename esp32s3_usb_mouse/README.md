# ESP32-S3 USB Mouse with ESP-IDF

This project accompanies the article:

Mastering USB on the ESP32-S3 with ESP-IDF: Part 11 - Build a USB Mouse

The firmware turns an ESP32-S3 into a native USB HID mouse using ESP-IDF and TinyUSB. After the host enumerates the device, the firmware moves the cursor in a square pattern and performs a left click. The demo runs once after USB enumeration and can be triggered again with the BOOT button on GPIO0.

## Hardware Required

- ESP32-S3 development board with native USB connected to the USB-OTG peripheral
- USB data cable
- Windows, Linux, or macOS computer
- ESP-IDF installed

## Important USB Pins

On the ESP32-S3 native USB peripheral:

```text
USB D+ -> GPIO20
USB D- -> GPIO19
```

Use the native USB connector on your ESP32-S3 board. Do not use the USB-to-UART connector unless that connector is also wired to the native USB peripheral.

## Build and Flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your serial port, for example `COM5` on Windows or `/dev/ttyACM0` on Linux.

## Behavior

1. The firmware initializes the BOOT button.
2. The firmware installs the TinyUSB device stack.
3. The ESP32-S3 enumerates as a USB HID mouse.
4. After a 3-second safety delay, the cursor moves in a square pattern.
5. The firmware sends a left-click sequence.
6. Press the BOOT button to run the demo again.

## Safety Notes

This project makes the ESP32-S3 behave as a real USB mouse. The cursor will move and click on the connected computer. Test it on your own machine only, and avoid running it while important work is open.

## Project Structure

```text
esp32s3_usb_mouse/
|-- CMakeLists.txt
|-- README.md
|-- sdkconfig.defaults
|-- main/
|   |-- CMakeLists.txt
|   |-- main.c
|   |-- usb_descriptors.c
|   |-- usb_descriptors.h
|   |-- usb_mouse_config.h
```
