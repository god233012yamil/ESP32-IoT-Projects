# ESP32-S3 Loose Coupling Demo

This ESP-IDF project demonstrates loose coupling in embedded systems using interfaces, callbacks, events, dependency injection, and RTOS queues.

## Target

- MCU: ESP32-S3
- Framework: ESP-IDF 5.x
- Default serial baud rate: 115200

## What the demo shows

- A fake temperature source exposed through a generic `temperature_source_t` interface.
- A PWM fan output exposed through a generic `fan_output_t` interface.
- A climate controller that depends only on interfaces, not hardware drivers.
- A button input module that reports events through a callback.
- An application event queue that decouples producers from consumers.
- A short ISR that only reports a hardware event.

## Default GPIOs

- User button: GPIO0, active low, internal pull-up enabled.
- Fan PWM output: GPIO2.

You can connect an LED with a resistor to GPIO2 to observe the fan output. Many ESP32-S3 development boards expose GPIO2, but check your board schematic before wiring.

## Build and flash

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

## Expected behavior

The fake temperature source ramps between 25 C and 50 C. The climate controller enables the PWM fan output when the temperature reaches 40 C and disables it when the temperature falls to 35 C. Pressing the button injects a user button event into the application event queue.
