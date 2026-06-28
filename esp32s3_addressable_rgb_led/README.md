# ESP32-S3 Addressable RGB LED Demo

This ESP-IDF project demonstrates how to control a WS2812-compatible addressable RGB LED on an ESP32-S3 board using the Espressif `led_strip` component and the RMT backend.

The project includes:

- A reusable RGB LED driver module
- Brightness scaling
- Predefined colors
- Status LED patterns
- A color cycle demonstration
- A brightness ramp demonstration

## Target Hardware

- ESP32-S3 development board
- Onboard WS2812-compatible RGB LED or external addressable RGB LED

Many ESP32-S3 boards use GPIO48 for the onboard RGB LED. If your board uses another GPIO, edit this line in `main/main.c`:

```c
#define RGB_LED_GPIO 48
```

## Build and Flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the serial port used by your board.

## External LED Wiring

```text
ESP32-S3 GPIO -> 330 ohm resistor -> LED DIN
ESP32-S3 GND  -> LED GND
LED VCC       -> 5 V or 3.3 V supply, depending on the LED module
```

For external LED strips, connect the ESP32-S3 ground and LED power supply ground together.

## Notes

- The default color format is GRB, which is common for WS2812-compatible LEDs.
- The default brightness is intentionally low to avoid harsh onboard LED brightness.
- For long LED strips, use a dedicated power supply and proper power injection.
