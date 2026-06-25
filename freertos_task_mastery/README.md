# FreeRTOS Task Mastery - ESP-IDF Example

This ESP-IDF project demonstrates practical FreeRTOS task design patterns for embedded applications.

## Features

- Periodic task execution with `vTaskDelayUntil()`
- Event-driven task execution with a queue
- Direct-to-task notifications
- Mutex-protected console access
- Event groups for system state
- Task priorities and task handles
- Task stack high-water-mark monitoring
- Clean task shutdown structure
- Optional status LED activity

## Target

The project is written for ESP-IDF 5.x and can be built for common ESP32 targets.

The default status LED GPIO is GPIO2. Change `STATUS_LED_GPIO` in `main/main.c` if your board uses a different LED pin.

## Build and Flash

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

For another target, replace `esp32` with the required device, for example:

```bash
idf.py set-target esp32s3
```

## Expected Behavior

- The sensor task generates one simulated temperature sample per second.
- The logger task receives and prints queued samples.
- The notification task toggles the status LED whenever a new sample is queued.
- The monitor task reports task state, priority, stack high-water mark, queue depth, and free heap every five seconds.
