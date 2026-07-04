# ESP32-S3 PID Controller Demo Flowchart

This document describes the project flow using Mermaid diagrams. It is intended to help beginners understand how the ESP-IDF application, PID component, simulated plant, and PWM output work together.

## High-Level Project Flow

```mermaid
flowchart TD
    A["Power on / Reset ESP32-S3"] --> B["ESP-IDF starts firmware"]
    B --> C["app_main() runs"]
    C --> D["Initialize LEDC PWM output"]
    D --> E{"PWM init successful?"}
    E -- "No" --> F["ESP_ERROR_CHECK stops application"]
    E -- "Yes" --> G["Create FreeRTOS PID control task"]
    G --> H{"Task created?"}
    H -- "No" --> I["Log task creation error"]
    H -- "Yes" --> J["pid_control_task() runs every 10 ms"]
    J --> K["Read current setpoint"]
    K --> L["Compute PID output"]
    L --> M["Update simulated plant"]
    M --> N["Write PID output to PWM duty"]
    N --> O["Log SP, PV, ERR, OUT, P, I, D, DUTY"]
    O --> P["Wait until next 10 ms period"]
    P --> J
```

## Application Startup Flow

```mermaid
flowchart TD
    A["app_main()"] --> B["Call pwm_output_init()"]
    B --> C["Configure LEDC timer"]
    C --> D{"Timer configured?"}
    D -- "No" --> E["Return ESP-IDF error"]
    D -- "Yes" --> F["Configure LEDC channel"]
    F --> G{"Channel configured?"}
    G -- "No" --> E
    G -- "Yes" --> H["Return ESP_OK"]
    H --> I["Log demo startup messages"]
    I --> J["Create pid_control FreeRTOS task"]
    J --> K{"xTaskCreatePinnedToCore() == pdPASS?"}
    K -- "No" --> L["Log failed task creation"]
    K -- "Yes" --> M["Control task begins running"]
```

## Periodic Control Task Flow

```mermaid
flowchart TD
    A["pid_control_task() starts"] --> B["Create PID controller instance"]
    B --> C["Fill pid_config_t values"]
    C --> D["Create simulated first-order plant"]
    D --> E["Call pid_init()"]
    E --> F["Initialize timing variables"]
    F --> G["Start infinite control loop"]
    G --> H["Get setpoint from demo_get_setpoint()"]
    H --> I["Call pid_set_setpoint()"]
    I --> J["Call pid_compute() with plant output"]
    J --> K["Update simulated plant using PID output"]
    K --> L["Call pwm_output_write()"]
    L --> M{"Is it time to log?"}
    M -- "Yes" --> N["Print setpoint, process variable, error, PID terms, and PWM duty"]
    M -- "No" --> O["Skip logging this cycle"]
    N --> P["Increment control tick counter"]
    O --> P
    P --> Q["vTaskDelayUntil() waits for next period"]
    Q --> G
```

## PID Compute Flow

```mermaid
flowchart TD
    A["pid_compute(pid, measurement, output)"] --> B{"Are arguments valid and initialized?"}
    B -- "No" --> C["Return PID_STATUS_INVALID_ARG"]
    B -- "Yes" --> D["Calculate error = setpoint - measurement"]
    D --> E["Calculate proportional term"]
    E --> F["Calculate raw derivative from measurement change"]
    F --> G{"Derivative filter enabled?"}
    G -- "Yes" --> H["Calculate derivative filter alpha"]
    G -- "No" --> I["Use alpha = 1.0"]
    H --> J["Update filtered derivative"]
    I --> J
    J --> K["Calculate derivative term"]
    K --> L["Calculate candidate integral"]
    L --> M["Calculate candidate output"]
    M --> N{"Would output saturate?"}
    N -- "No" --> O["Allow integration"]
    N -- "High saturation and error pushes down" --> O
    N -- "Low saturation and error pushes up" --> O
    N -- "Saturation gets worse" --> P["Hold previous integral"]
    O --> Q["Store candidate integral"]
    P --> R["Use existing integral"]
    Q --> S["Calculate final integral term"]
    R --> S
    S --> T["Calculate final PID output"]
    T --> U["Clamp output to min/max limits"]
    U --> V["Store previous measurement"]
    V --> W["Write output value"]
    W --> X["Return PID_STATUS_OK"]
```

## Setpoint Schedule

The demo changes the target setpoint every five seconds. This creates visible step responses in the serial monitor.

```mermaid
flowchart LR
    A["0 to 5 seconds"] --> B["Setpoint = 30.0"]
    B --> C["5 to 10 seconds"]
    C --> D["Setpoint = 70.0"]
    D --> E["10 to 15 seconds"]
    E --> F["Setpoint = 45.0"]
    F --> G["15 to 20 seconds"]
    G --> H["Setpoint = 85.0"]
    H --> I["Repeat 20 second cycle"]
    I --> A
```

## Data Flow Between Modules

```mermaid
flowchart TD
    A["main/main.c"] --> B["Creates PID config"]
    B --> C["pid_controller component"]
    C --> D["PID output 0.0 to 100.0 percent"]
    D --> E["Simulated first-order plant"]
    E --> F["Process variable"]
    F --> C
    D --> G["LEDC PWM driver"]
    G --> H["PWM duty on DEMO_PWM_GPIO"]
    D --> I["ESP_LOGI runtime logs"]
    F --> I
```

## Component Relationship

```mermaid
flowchart TD
    A["ESP-IDF Project"] --> B["main component"]
    A --> C["pid_controller component"]
    A --> D["ESP-IDF components"]
    B --> E["main.c"]
    B --> F["main/CMakeLists.txt"]
    C --> G["pid_controller.c"]
    C --> H["pid_controller.h"]
    D --> I["FreeRTOS"]
    D --> J["LEDC driver"]
    D --> K["GPIO driver"]
    D --> L["ESP logging"]
    E --> C
    E --> I
    E --> J
    E --> K
    E --> L
```

## What to Watch in the Serial Monitor

```mermaid
flowchart TD
    A["Serial log line"] --> B["SP: target setpoint"]
    A --> C["PV: simulated process value"]
    A --> D["ERR: SP - PV"]
    A --> E["OUT: final PID output"]
    A --> F["P: proportional term"]
    A --> G["I: integral term"]
    A --> H["D: derivative term"]
    A --> I["DUTY: LEDC PWM duty"]
```

If the controller is behaving correctly, the `PV` value should move toward the `SP` value after each setpoint change.
