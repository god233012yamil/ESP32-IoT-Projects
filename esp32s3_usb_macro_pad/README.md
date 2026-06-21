# ESP32-S3 USB Macro Pad

A four-button USB HID macro pad built with ESP-IDF and the ESP32-S3 native USB peripheral.

## Tested Target

- Target: ESP32-S3
- Recommended ESP-IDF: v5.4.1 or newer 5.x release
- USB component: `espressif/esp_tinyusb` v1.x

## Button Wiring

Connect each normally open push button between its GPIO and GND.
The firmware enables the internal pull-up resistors.

| Button | GPIO | Macro |
|---|---:|---|
| 1 | GPIO4 | Ctrl+C - Copy |
| 2 | GPIO5 | Ctrl+V - Paste |
| 3 | GPIO6 | Ctrl+S - Save |
| 4 | GPIO7 | Ctrl+Shift+Esc - Windows Task Manager |

The ESP32-S3 native USB peripheral uses GPIO19 for D- and GPIO20 for D+.
Do not use those pins for the buttons.

## Build

Open an ESP-IDF terminal in the project directory and run:

```text
idf.py set-target esp32s3
idf.py build
```

The first build downloads the managed TinyUSB components.

## Flash

Flash through the board's USB-to-UART connector or another programming interface:

```text
idf.py -p COM_PORT flash monitor
```

Replace `COM_PORT` with the serial port used by the programming interface.

After flashing, connect the computer to the board connector wired to the ESP32-S3 native USB peripheral. Some development boards provide separate USB-to-UART and USB-OTG connectors.

## Operation

1. Open a text editor or another safe application.
2. Press a macro-pad button.
3. The ESP32-S3 sends the matching HID keyboard shortcut.
4. Each shortcut includes a press report followed by an all-keys-released report.

## Project Structure

```text
esp32s3_usb_macro_pad/
|-- CMakeLists.txt
|-- sdkconfig.defaults
|-- README.md
`-- main/
    |-- CMakeLists.txt
    |-- idf_component.yml
    |-- app_main.c
    |-- button_driver.c
    |-- button_driver.h
    |-- macro_engine.c
    |-- macro_engine.h
    |-- usb_hid_keyboard.c
    `-- usb_hid_keyboard.h
```

## Notes

- Button inputs are active low.
- The scanner runs every 5 ms and uses a 20 ms debounce interval.
- A button generates one macro event only on the released-to-pressed transition.
- USB reports are serialized by the macro task.
- HID endpoint waits include timeouts to prevent permanent blocking.
- The Task Manager macro is Windows-specific. Replace it for Linux or macOS as required.
