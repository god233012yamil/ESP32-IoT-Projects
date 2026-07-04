# ESP32-S3 PID Controller Demo

This ESP-IDF project accompanies the article **Mastering PID Controllers - Part 2: Implementing a PID Controller in Embedded C**.

The goal of this project is to show how a PID controller can be written as a reusable Embedded C module and then used inside a real ESP32-S3 firmware application. It is designed for beginners who already know basic C syntax and want to understand how control-loop code is organized in an embedded project.

The firmware runs a periodic PID control task on the ESP32-S3. Instead of requiring a motor, heater, fan, pump, or sensor, the project includes a small simulated plant model. This lets you build, flash, and observe the PID behavior with only an ESP32-S3 board.

## What You Will Learn

By studying this project, you can learn how to:

- Split reusable control logic into an ESP-IDF component.
- Configure and run a fixed-period FreeRTOS control task.
- Compute proportional, integral, and derivative terms in C.
- Limit controller output to a safe range.
- Reduce integral windup when the output saturates.
- Use derivative-on-measurement to avoid large derivative spikes when the setpoint changes.
- Smooth the derivative term with a simple low-pass filter.
- Map a controller output percentage to an LEDC PWM duty cycle.
- Use ESP-IDF logging to inspect controller behavior at runtime.

## What Is a PID Controller?

A PID controller is a feedback controller. It compares a desired value with a measured value and calculates an output that should push the system toward the desired value.

In this project:

- The desired value is called the **setpoint**.
- The measured or simulated value is called the **process variable**.
- The difference between them is called the **error**.
- The controller output is a value from `0.0` to `100.0`, treated like actuator effort in percent.

The PID output is made from three terms:

- **P - Proportional:** reacts to the current error. Larger error produces a larger correction.
- **I - Integral:** reacts to accumulated error over time. This helps remove steady-state error.
- **D - Derivative:** reacts to how quickly the process variable is changing. This can reduce overshoot and improve damping.

The controller equation used by the code is conceptually:

```text
output = proportional_term + integral_term + derivative_term
```

The implementation then clamps the result to the configured output limits.

## Features

- Reusable PID component written in C.
- Fixed sample-time control loop.
- Output limiting.
- Conditional integration anti-windup.
- Derivative-on-measurement to reduce derivative kick.
- First-order low-pass filtering on the derivative term.
- LEDC PWM output for observing actuator command.
- Runtime logging of setpoint, plant output, error, PID terms, and PWM duty.

## Hardware

Required:

- ESP32-S3 development board.
- USB cable for flashing and serial monitor output.

Optional:

- LED and resistor connected to GPIO 48.
- Or use a different PWM-capable GPIO by changing `DEMO_PWM_GPIO` in `main/main.c`.

Many ESP32-S3 boards use GPIO 48 for an onboard LED or RGB LED control path, but this varies by board. The demo still runs correctly even if no LED is connected because the plant is simulated in software.

## Software Requirements

- ESP-IDF v5.x.
- ESP-IDF command prompt, terminal, or VS Code ESP-IDF extension.
- A configured ESP32-S3 toolchain.

This project was checked with ESP-IDF v5.4.1.

## Build and Flash

Open a terminal in the project folder and run:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the serial port used by your board.

Examples:

```bash
idf.py -p COM3 flash monitor
```

On Linux or macOS, the port name may look more like this:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

To exit the serial monitor, press:

```text
Ctrl+]
```

## Project Structure

```text
esp32s3_pid_controller_demo/
|-- CMakeLists.txt
|-- sdkconfig.defaults
|-- README.md
|-- components/
|   `-- pid_controller/
|       |-- CMakeLists.txt
|       |-- include/
|       |   `-- pid_controller.h
|       `-- pid_controller.c
`-- main/
    |-- CMakeLists.txt
    `-- main.c
```

Important files:

- `main/main.c` contains the ESP-IDF application, FreeRTOS task, simulated plant, PWM setup, and logging.
- `components/pid_controller/pid_controller.c` contains the reusable PID algorithm.
- `components/pid_controller/include/pid_controller.h` defines the public PID API and data structures.
- `main/CMakeLists.txt` declares the components required by the main application.
- `sdkconfig.defaults` stores default ESP-IDF configuration values for this demo.

## How the Demo Works

The firmware starts in `app_main()`.

First, it configures the LEDC peripheral so the PID output can be represented as a PWM signal. Then it creates a FreeRTOS task called `pid_control`.

The control task runs every 10 ms:

```c
#define CONTROL_PERIOD_MS (10)
#define CONTROL_PERIOD_S  (0.010f)
```

Each loop does the following:

1. Selects the current setpoint.
2. Sends the setpoint to the PID controller.
3. Computes the next PID output.
4. Updates the simulated plant.
5. Writes the controller output to PWM.
6. Prints diagnostic values to the serial monitor.

The setpoint changes automatically every few seconds. This creates step changes so you can watch how the controller responds.

## Simulated Plant

A real PID controller usually controls a physical system, such as:

- Motor speed.
- Heater temperature.
- Fan airflow.
- Tank level.
- Power converter voltage.
- Servo position.

To keep this project easy to run, `main.c` includes a simple first-order plant simulation:

```c
typedef struct
{
    float output;
    float time_constant_s;
    float gain;
} first_order_plant_t;
```

The plant does not jump instantly to the controller output. Instead, it moves gradually, like a real system with inertia or thermal mass.

The main tuning value is:

```c
.time_constant_s = 1.20f
```

A larger time constant makes the simulated system respond more slowly. A smaller time constant makes it respond faster.

## PID Configuration

The PID controller is configured in `pid_control_task()`:

```c
const pid_config_t pid_config = {
    .kp = 1.20f,
    .ki = 0.80f,
    .kd = 0.08f,
    .sample_time_s = CONTROL_PERIOD_S,
    .output_min = 0.0f,
    .output_max = 100.0f,
    .integral_min = -80.0f,
    .integral_max = 80.0f,
    .derivative_filter_tau_s = 0.050f,
};
```

Beginner-friendly meaning of each value:

- `kp`: How strongly the controller reacts to present error.
- `ki`: How strongly the controller reacts to accumulated past error.
- `kd`: How strongly the controller reacts to the rate of change.
- `sample_time_s`: Time between PID calculations, in seconds.
- `output_min`: Lowest allowed controller output.
- `output_max`: Highest allowed controller output.
- `integral_min`: Lower limit for the stored integral value.
- `integral_max`: Upper limit for the stored integral value.
- `derivative_filter_tau_s`: Smoothing amount for the derivative term.

## Runtime Log Output

When the firmware runs, the serial monitor prints lines like this:

```text
SP=70.00 PV=42.11 ERR=27.89 OUT=85.33 P=33.47 I=49.18 D=2.68 DUTY=6990
```

Field meanings:

- `SP`: Setpoint, the target value.
- `PV`: Process variable, the simulated measured value.
- `ERR`: Error, calculated as `SP - PV`.
- `OUT`: Final PID output after limiting.
- `P`: Proportional term.
- `I`: Integral term.
- `D`: Derivative term.
- `DUTY`: LEDC PWM duty value written to the output pin.

If the controller is working, `PV` should move toward `SP` after each setpoint change.

## PWM Output

The PID output is expressed as a percentage from `0.0` to `100.0`. The demo converts that percentage into a PWM duty cycle:

```c
const uint32_t duty = (uint32_t)((limited_output / 100.0f) * (float)DEMO_PWM_MAX_DUTY);
```

The PWM configuration is:

```c
#define DEMO_PWM_FREQUENCY_HZ (5000)
#define DEMO_PWM_RESOLUTION   LEDC_TIMER_13_BIT
```

With 13-bit resolution, the duty value ranges from `0` to `8191`.

## Things to Try

After the project builds and runs, try changing one value at a time.

Good beginner experiments:

- Change `kp` and observe how strongly the output reacts to error.
- Set `ki` to `0.0f` and observe whether the process variable settles below the setpoint.
- Increase `kd` and observe whether the response becomes more damped.
- Change `time_constant_s` to make the simulated plant faster or slower.
- Change the setpoint values in `demo_get_setpoint()`.
- Change `LOG_PERIOD_TICKS` to print more or fewer log messages.
- Move `DEMO_PWM_GPIO` to a pin available on your board.

When tuning, change only one parameter at a time. This makes it much easier to understand what each term does.

## Common Build Notes

If `idf.py` is not recognized, open an ESP-IDF terminal or run the ESP-IDF export script for your installation.

If flashing fails, check:

- The board is connected over USB.
- The correct serial port is selected.
- No other serial monitor is using the port.
- The board is in download mode if your development board does not auto-reset.

If you see an internal compiler error inside an unused ESP-IDF driver, make sure `main/CMakeLists.txt` depends only on the drivers this demo needs:

```cmake
REQUIRES pid_controller esp_driver_gpio esp_driver_ledc
```

Using the broad legacy `driver` component can pull in many unused ESP-IDF drivers and increase build time.

## Reusing the PID Component

The PID logic is intentionally separated from the ESP32-specific code. This makes it easier to reuse in another project.

The basic usage pattern is:

```c
pid_controller_t pid;
pid_config_t config = {
    .kp = 1.0f,
    .ki = 0.5f,
    .kd = 0.1f,
    .sample_time_s = 0.01f,
    .output_min = 0.0f,
    .output_max = 100.0f,
    .integral_min = -50.0f,
    .integral_max = 50.0f,
    .derivative_filter_tau_s = 0.05f,
};

pid_init(&pid, &config);
pid_set_setpoint(&pid, 50.0f);
pid_compute(&pid, measurement, &output);
```

For real hardware, replace the simulated plant with an actual sensor reading, then send the PID output to a real actuator such as PWM, DAC, motor driver, or heater driver.

## Safety Note

This demo is safe because it controls a simulated plant. When using PID control with real hardware, always add safety limits outside the PID controller too. Examples include maximum duty cycle limits, temperature cutoffs, current limits, timeout handling, and emergency stop behavior.
