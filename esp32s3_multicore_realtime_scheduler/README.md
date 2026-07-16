# ESP32-S3 Multicore Real-Time Scheduler

An ESP-IDF demonstration project for deterministic task scheduling on the dual-core ESP32-S3. The application models a 1 kHz real-time control loop pinned to one core while telemetry and background service load run on the other core.

The goal is to show practical embedded scheduling techniques that reduce jitter, protect the real-time path from avoidable blocking, and expose timing behavior through logs and GPIO instrumentation.

## What This Project Demonstrates

- Core-affinity control using `xTaskCreatePinnedToCore()`
- A high-priority 1 kHz control task isolated on core 0
- Lower-priority telemetry and synthetic service load isolated on core 1
- Periodic release using `esp_timer_start_periodic()`
- Direct-to-task notifications for low-overhead task release
- CPU-cycle execution-time measurement with `esp_cpu_get_cycle_count()`
- Release-jitter, deadline-miss, overrun, and dropped-sample tracking
- A lock-free single-producer, single-consumer timing ring buffer
- Short critical sections for shared state protected by `portMUX_TYPE`
- GPIO instrumentation for oscilloscope or logic-analyzer validation

## Target Platform

- MCU: ESP32-S3
- Framework: ESP-IDF
- Recommended ESP-IDF version: 5.4.x or newer 5.x release
- FreeRTOS tick rate: 1000 Hz through `CONFIG_FREERTOS_HZ=1000`

## Hardware

Required:

- ESP32-S3 development board
- USB cable for flashing and monitoring

Optional but recommended:

- Oscilloscope or logic analyzer
- Probes connected to GPIO 2 and GPIO 3

Default instrumentation pins:

| GPIO | Purpose |
| --- | --- |
| GPIO 2 | Goes high while the control algorithm is executing |
| GPIO 3 | Pulses when the control response time exceeds the configured deadline |

If either pin conflicts with your board, change `PROFILE_GPIO` and `DEADLINE_MISS_GPIO` in `main/realtime_scheduler.c`.

## Project Structure

```text
esp32s3_multicore_realtime_scheduler/
|-- CMakeLists.txt
|-- README.md
|-- FLOWCHART.md
|-- sdkconfig
|-- sdkconfig.defaults
`-- main/
    |-- CMakeLists.txt
    |-- main.c
    |-- realtime_scheduler.c
    |-- realtime_scheduler.h
    |-- lockfree_ring.c
    `-- lockfree_ring.h
```

### Source Files

| File | Responsibility |
| --- | --- |
| `main/main.c` | ESP-IDF entry point. Calls `realtime_scheduler_start()`. |
| `main/realtime_scheduler.c` | Creates tasks, starts the periodic release timer, runs the control loop, collects timing stats, and reports telemetry. |
| `main/realtime_scheduler.h` | Public scheduler start API. |
| `main/lockfree_ring.c` | Lock-free single-producer, single-consumer timing sample queue. |
| `main/lockfree_ring.h` | Ring buffer types and function declarations. |
| `sdkconfig.defaults` | Default ESP-IDF configuration for target, FreeRTOS stats, watchdog, and logging. |

## Runtime Architecture

The application separates real-time and non-real-time work by core:

| Component | Core | Priority | Role |
| --- | ---: | ---: | --- |
| `control_task` | 0 | 22 | Executes the 1 kHz control loop and records timing samples. |
| `telemetry_task` | 1 | 8 | Drains timing samples and logs aggregate statistics once per second. |
| `service_stress` | 1 | 5 | Generates background CPU load and exercises a short protected shared-state update. |
| `esp_timer` callback | ESP timer task | ESP-IDF managed | Releases the control task every 1000 us using a task notification. |

The control task is intentionally kept free of logging, dynamic allocation, long critical sections, and blocking service calls. It performs deterministic synthetic arithmetic to model a repeatable control workload.

## Timing Model

Default timing constants are defined in `main/realtime_scheduler.c`:

| Constant | Value | Meaning |
| --- | ---: | --- |
| `CONTROL_PERIOD_US` | 1000 us | Control-loop release period, equivalent to 1 kHz. |
| `CONTROL_DEADLINE_US` | 800 us | Maximum allowed response time from release to completion. |
| `CONTROL_WORK_ITERATIONS` | 220 | Synthetic workload size. Increase to force overload. |
| `TELEMETRY_REPORT_MS` | 1000 ms | Log reporting interval. |
| `LF_RING_CAPACITY` | 128 samples | Timing ring capacity. |

Each control release measures:

- Release timestamp using `esp_timer_get_time()`
- Absolute release jitter relative to the expected release time
- Execution time in CPU cycles
- Response time in microseconds
- Deadline status
- Sequence number

## Data Flow

1. `app_main()` calls `realtime_scheduler_start()`.
2. The scheduler initializes atomics, timing stats, the ring buffer, and GPIO outputs.
3. The control task is pinned to core 0.
4. Telemetry and service stress tasks are pinned to core 1.
5. A periodic `esp_timer` releases the control task every 1000 us.
6. The control task runs deterministic work, measures timing, drives GPIO 2 during execution, and pulses GPIO 3 on deadline misses.
7. Timing samples are pushed into the lock-free ring buffer.
8. The telemetry task drains the ring buffer and logs aggregate statistics once per second.
9. The service stress task creates background load on core 1 while keeping cross-core critical sections short.

For a visual version of this flow, see [FLOWCHART.md](FLOWCHART.md).

## Build, Flash, and Monitor

From this project directory:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the serial port for your board, such as `COM5` on Windows or `/dev/ttyUSB0` on Linux.

To exit the ESP-IDF monitor, press:

```text
Ctrl+]
```

## Expected Serial Output

After startup, the application prints the configured core placement and GPIO assignments:

```text
I (...) realtime_sched: Real-time scheduler demo started
I (...) realtime_sched: Control task pinned to core 0
I (...) realtime_sched: Service tasks pinned to core 1
I (...) realtime_sched: GPIO 2: execution pulse, GPIO 3: deadline-miss pulse
```

Every second, telemetry reports aggregate timing:

```text
I (...) realtime_sched: samples=1000 consumed=1000 misses=0 consumed_misses=0 min_cycles=... max_cycles=... max_jitter_us=... overruns=0 dropped=0
```

### Telemetry Fields

| Field | Meaning |
| --- | --- |
| `samples` | Total timing samples produced by the control task. |
| `consumed` | Timing samples drained by the telemetry task. |
| `misses` | Total deadline misses observed by the control task. |
| `consumed_misses` | Deadline misses observed in drained timing samples. |
| `min_cycles` | Minimum measured control execution time in CPU cycles. |
| `max_cycles` | Maximum measured control execution time in CPU cycles. |
| `max_jitter_us` | Largest absolute release jitter measured so far. |
| `overruns` | Releases that arrived before the previous release was fully handled. |
| `dropped` | Ring-buffer samples dropped because the telemetry consumer fell behind. |

## Oscilloscope or Logic-Analyzer Validation

1. Connect channel 1 to GPIO 2.
2. Connect channel 2 to GPIO 3.
3. Flash and monitor the application.
4. Confirm GPIO 2 produces a stable 1 kHz execution pulse.
5. Measure the GPIO 2 pulse width to estimate control execution time.
6. Confirm GPIO 3 remains low during normal operation.
7. Increase `CONTROL_WORK_ITERATIONS` to intentionally overload the control task.
8. Confirm deadline misses appear in the log and as GPIO 3 pulses.

## Real-Time Design Notes

### Core Isolation

The control loop is pinned to core 0 and service work is pinned to core 1. This keeps telemetry logging and synthetic background work away from the high-priority control path.

### Direct Task Notification

The periodic timer callback uses `xTaskNotifyGive()` instead of queues or semaphores. This keeps release signaling lightweight and allows the control task to detect batched notifications.

### Lock-Free Timing Queue

The timing ring is designed for exactly one producer and one consumer:

- Producer: `control_task`
- Consumer: `telemetry_task`

The ring uses C11 atomics and acquire/release ordering so the producer can publish samples without taking a mutex in the real-time path.

### Critical Sections

The project still includes a small protected shared-state update in `service_stress_task()` to demonstrate how to keep cross-core critical sections short. Long critical sections should not be added to the control path.

### GPIO Instrumentation

GPIO 2 and GPIO 3 provide external timing visibility that serial logs cannot provide. Logs are useful for aggregate trends; scope traces are better for measuring short pulses, jitter, and deadline behavior.

## Configuration

`sdkconfig.defaults` sets the project defaults:

```text
CONFIG_IDF_TARGET="esp32s3"
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y
CONFIG_ESP_TASK_WDT_EN=y
CONFIG_ESP_TASK_WDT_INIT=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=5
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=y
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

If you regenerate `sdkconfig`, keep these settings aligned with the timing experiment you want to run.

## Tuning Experiments

Try these controlled changes to study scheduler behavior:

| Experiment | Change | Expected Observation |
| --- | --- | --- |
| Increase control workload | Raise `CONTROL_WORK_ITERATIONS` | Longer GPIO 2 pulse width, possible deadline misses. |
| Tighten deadline | Lower `CONTROL_DEADLINE_US` | More deadline misses without changing execution time. |
| Increase telemetry pressure | Lower telemetry delay or add logging | Possible ring drops if telemetry cannot keep up. |
| Add service load | Add work to `service_stress_task()` | Core 1 load should not directly block the core 0 control loop unless shared resources are contended. |
| Test pin conflicts | Change GPIO constants | Confirms instrumentation can be adapted to board constraints. |

## Extending the Demo

To adapt this into a real control application:

1. Replace `execute_control_algorithm()` with sensor acquisition, control-law calculation, and actuator output.
2. Keep dynamic memory allocation, logging, and blocking I/O out of the control task.
3. Move communication, storage, diagnostics, and UI work to service tasks.
4. Use DMA, queues, or lock-free buffers for handoff where appropriate.
5. Keep shared critical sections small and measurable.
6. Validate with both logs and external GPIO timing captures.

## Troubleshooting

| Symptom | Likely Cause | Action |
| --- | --- | --- |
| Build target is not ESP32-S3 | Target not set or stale build directory | Run `idf.py set-target esp32s3`, then rebuild. |
| No serial logs | Wrong port or monitor speed | Check the board port and rerun `idf.py -p COMx monitor`. |
| GPIO 2 has no pulse | Wrong pin, boot strap conflict, or probe ground issue | Verify board pinout and update `PROFILE_GPIO` if needed. |
| Frequent `misses` | Workload exceeds deadline or system is overloaded | Reduce `CONTROL_WORK_ITERATIONS` or increase `CONTROL_DEADLINE_US`. |
| Nonzero `dropped` | Telemetry task is not draining fast enough | Increase `LF_RING_CAPACITY` or reduce telemetry/reporting overhead. |
| Nonzero `overruns` | A new release arrived before the previous release was handled | Reduce control work or increase the period. |

## License

No license file is currently included in this project. Add a license before publishing or reusing the code outside your local environment.
