# ESP32 PWM LEDC Demo

This ESP-IDF project accompanies the article "Mastering ESP32 PWM: Controlling LEDs, Motors, Buzzers, and Servo Signals with ESP-IDF".

The project demonstrates how to use the ESP32 LEDC peripheral to generate PWM signals for:

- LED dimming
- DC motor speed control through an external driver
- Passive buzzer tone generation
- Servo-style 50 Hz pulse generation

## Target Framework

- ESP-IDF v5.x
- Tested project structure for ESP32, ESP32-S3, ESP32-C3, ESP32-C6, and related ESP-IDF targets that support LEDC

## Default GPIO Mapping

| Function | GPIO | Notes |
|---|---:|---|
| LED PWM | GPIO2 | Connect to LED through a current-limiting resistor |
| Motor PWM | GPIO18 | Drive an external MOSFET, motor driver, or H-bridge |
| Passive buzzer PWM | GPIO19 | Use a passive buzzer or a suitable transistor driver |
| Servo signal | GPIO21 | Signal only; power the servo from an external supply |

You can change the GPIO assignments with:

```bash
idf.py menuconfig
```

Then open:

```text
ESP32 PWM LEDC Demo Configuration
```

## Hardware Notes

Do not connect motors, servos, relays, or high-current loads directly to ESP32 GPIO pins.

Use proper driver circuits:

- LED: GPIO -> resistor -> LED -> GND
- DC motor: GPIO -> MOSFET or motor driver input
- Servo: GPIO -> servo signal input, external 5 V servo supply, common ground
- Buzzer: GPIO -> passive buzzer if current is safe, otherwise use a transistor driver

When using an external supply, connect the external supply ground to ESP32 ground.

## Build and Flash

Set your ESP-IDF target. For example, for ESP32-S3:

```bash
idf.py set-target esp32s3
```

Build the project:

```bash
idf.py build
```

Flash and monitor:

```bash
idf.py -p COMx flash monitor
```

On Linux or macOS, replace `COMx` with the correct serial device, such as `/dev/ttyUSB0`.

## Demo Behavior

The application repeats the following sequence:

1. Fades the LED up and down using a 5 kHz PWM signal.
2. Changes motor PWM duty cycle from 0% to 100% and back down.
3. Generates several passive buzzer tones by changing PWM frequency.
4. Generates servo-style 1 ms, 1.5 ms, and 2 ms pulses at 50 Hz.

## Source Organization

```text
esp32_pwm_ledc_demo/
  CMakeLists.txt
  README.md
  sdkconfig.defaults
  main/
    CMakeLists.txt
    Kconfig.projbuild
    app_main.c
    pwm_ledc_demo.c
    pwm_ledc_demo.h
```

## Teaching Notes

The source code includes Google-style function docstrings and line comments where extra clarity is useful. The implementation intentionally separates PWM initialization, raw duty updates, percentage-based duty updates, frequency changes, and servo pulse calculations so beginner engineers can study each concept independently.
