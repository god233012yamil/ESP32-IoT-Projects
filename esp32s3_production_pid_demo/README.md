# ESP32-S3 Production-Grade PID Controller Demo

This ESP-IDF project accompanies the article section "Production-Grade PID Library for ESP32" from Part 3 of the PID controller series.

The project demonstrates a reusable PID controller library with:

- Floating-point PID implementation
- Q16.16 fixed-point PID implementation
- Derivative-on-measurement
- Conditional integration anti-windup
- Derivative low-pass filtering
- Bumpless transfer support
- Output clamping
- Output slew-rate limiting
- FreeRTOS periodic control task
- LEDC PWM actuator output
- Simulated motor plant for safe testing without external hardware

## Target

- MCU: ESP32-S3
- Framework: ESP-IDF 5.x
- Default PWM output: GPIO2
- Serial baud rate: 115200

## Project Layout

```text
esp32s3_production_pid_demo/
  CMakeLists.txt
  sdkconfig.defaults
  README.md
  main/
    CMakeLists.txt
    main.c
    simulated_motor.c
    simulated_motor.h
  components/
    pid_controller/
      CMakeLists.txt
      pid_f32.c
      pid_q16.c
      include/
        pid_f32.h
        pid_q16.h
```

## How to Build

```bash
idf.py set-target esp32s3
idf.py build
```

## How to Flash and Monitor

```bash
idf.py -p COMx flash monitor
```

Replace `COMx` with your ESP32-S3 serial port.

## What the Demo Does

The firmware controls a simulated DC motor speed plant. The PID controller computes a PWM duty-cycle command from 0 to 100 percent. The output is also written to GPIO2 using the ESP32-S3 LEDC peripheral, so it can be observed with an oscilloscope or logic analyzer.

The demo periodically applies setpoint and load changes so you can observe the controller response through the serial monitor.

Example log output:

```text
I pid_demo: setpoint=1800.0 rpm, speed=1764.5 rpm, pwm=58.7 %, load=15.0 %
I pid_demo: setpoint=2400.0 rpm, speed=2210.3 rpm, pwm=79.1 %, load=15.0 %
I pid_demo: setpoint=2400.0 rpm, speed=2360.8 rpm, pwm=86.4 %, load=45.0 %
```

## Notes

This project is designed as a safe software demonstration. It does not require a real motor, encoder, or power stage. To adapt it to real hardware, replace the simulated motor functions with actual sensor and actuator drivers.

Typical hardware replacements:

- `simulated_motor_get_speed_rpm()` -> encoder or tachometer speed measurement
- `pwm_set_duty_percent()` -> motor driver, heater MOSFET, valve driver, or power stage command
- `PID_SAMPLE_TIME_MS` -> control-loop period selected for the real plant

## Recommended First Experiments

1. Change `kp`, `ki`, and `kd` in `main.c`.
2. Increase or decrease `pid_f32_set_derivative_filter()` alpha.
3. Disable slew-rate limiting by setting it to `0.0f`.
4. Change the setpoint and load step values inside `pid_control_task()`.
5. Plot the serial output using a Python script, Serial Plotter, or another visualization tool.
