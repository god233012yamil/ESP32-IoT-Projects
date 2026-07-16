# Project Flowchart

This document shows the execution flow for the ESP32-S3 multicore real-time scheduler demo.

## System Startup

```mermaid
flowchart TD
    A["ESP-IDF starts app_main()"] --> B["Call realtime_scheduler_start()"]
    B --> C["Clear timing statistics"]
    C --> D["Initialize atomic counters"]
    D --> E["Initialize lock-free timing ring"]
    E --> F["Configure GPIO 2 and GPIO 3 as outputs"]
    F --> G["Create control_task pinned to core 0"]
    G --> H["Create telemetry_task pinned to core 1"]
    H --> I["Create service_stress task pinned to core 1"]
    I --> J["Create periodic esp_timer"]
    J --> K["Start 1000 us periodic timer"]
    K --> L["Log startup configuration"]
```

## Core Allocation

```mermaid
flowchart LR
    subgraph Core0["Core 0: Real-Time Domain"]
        C0["control_task<br/>Priority 22<br/>1 kHz release"]
    end

    subgraph Core1["Core 1: Service Domain"]
        T1["telemetry_task<br/>Priority 8<br/>Reports once per second"]
        S1["service_stress<br/>Priority 5<br/>Synthetic background load"]
    end

    Timer["esp_timer<br/>1000 us period"] -->|"xTaskNotifyGive()"| C0
    C0 -->|"timing_sample_t"| Ring["Lock-free SPSC ring buffer"]
    Ring --> T1
    S1 -->|"short critical section"| Shared["Guarded shared value"]
```

## Periodic Control Release

```mermaid
flowchart TD
    A["esp_timer period expires"] --> B["control_release_timer_callback()"]
    B --> C["Increment release sequence"]
    C --> D{"Was previous release still pending?"}
    D -->|"Yes"| E["Increment notification overrun counter"]
    D -->|"No"| F["Continue"]
    E --> G["Notify control_task"]
    F --> G
    G --> H["control_task unblocks"]
```

## Control Task Loop

```mermaid
flowchart TD
    A["control_task waits for notification"] --> B["ulTaskNotifyTake() returns release count"]
    B --> C{"Multiple notifications received?"}
    C -->|"Yes"| D["Add extra notifications to overrun counter"]
    C -->|"No"| E["Read release timestamp"]
    D --> E
    E --> F["Calculate release jitter"]
    F --> G["Update expected release time"]
    G --> H["Set GPIO 2 high"]
    H --> I["Read CPU cycle counter"]
    I --> J["Run deterministic control algorithm"]
    J --> K["Read CPU cycle counter again"]
    K --> L["Calculate execution cycles and response time"]
    L --> M{"Response time > 800 us?"}
    M -->|"Yes"| N["Set deadline_missed true<br/>Pulse GPIO 3"]
    M -->|"No"| O["Set deadline_missed false"]
    N --> P["Set GPIO 2 low"]
    O --> P
    P --> Q["Create timing_sample_t"]
    Q --> R["Push sample to lock-free ring"]
    R --> S["Update aggregate timing statistics"]
    S --> A
```

## Telemetry Task Loop

```mermaid
flowchart TD
    A["telemetry_task starts on core 1"] --> B["Try to pop timing samples"]
    B --> C{"Sample available?"}
    C -->|"Yes"| D["Increment consumed counters"]
    D --> B
    C -->|"No"| E["Check elapsed time since last report"]
    E --> F{"At least 1000 ms elapsed?"}
    F -->|"Yes"| G["Copy timing stats under short critical section"]
    G --> H["Read overrun and dropped-sample counters"]
    H --> I["Log aggregate telemetry"]
    I --> J["Delay 10 ms"]
    F -->|"No"| J
    J --> B
```

## Lock-Free Ring Buffer

```mermaid
flowchart LR
    subgraph Producer["Producer: control_task"]
        P1["Prepare timing sample"]
        P2["Read write_index"]
        P3["Read read_index with acquire ordering"]
        P4{"Ring full?"}
        P5["Store sample"]
        P6["Publish write_index with release ordering"]
        P7["Increment dropped_samples"]
    end

    subgraph Consumer["Consumer: telemetry_task"]
        C1["Read read_index"]
        C2["Read write_index with acquire ordering"]
        C3{"Ring empty?"}
        C4["Copy sample"]
        C5["Publish read_index with release ordering"]
    end

    P1 --> P2 --> P3 --> P4
    P4 -->|"No"| P5 --> P6
    P4 -->|"Yes"| P7

    C1 --> C2 --> C3
    C3 -->|"No"| C4 --> C5
    C3 -->|"Yes"| C1

    P6 -. "sample becomes visible" .-> C2
    C5 -. "space becomes visible" .-> P3
```

## Deadline and Overload Detection

```mermaid
flowchart TD
    A["Control release occurs every 1000 us"] --> B["Measure completion time"]
    B --> C["response_time_us = completion_time_us - release_time_us"]
    C --> D{"response_time_us > CONTROL_DEADLINE_US?"}
    D -->|"No"| E["Deadline met"]
    D -->|"Yes"| F["Deadline miss"]
    F --> G["Increment miss count"]
    G --> H["Pulse GPIO 3"]
    H --> I["Report miss in telemetry"]

    A --> J{"Next release arrives before task blocks again?"}
    J -->|"Yes"| K["Increment overrun counter"]
    J -->|"No"| L["Normal periodic execution"]
```

## High-Level Data Path

```mermaid
flowchart TD
    Timer["Periodic esp_timer"] --> Notify["Task notification"]
    Notify --> Control["Pinned control_task"]
    Control --> Work["Synthetic control workload"]
    Work --> Measure["Timing measurement"]
    Measure --> GPIO["GPIO instrumentation"]
    Measure --> Sample["timing_sample_t"]
    Sample --> Ring["Lock-free ring buffer"]
    Ring --> Telemetry["telemetry_task"]
    Telemetry --> Log["ESP_LOGI aggregate report"]

    Stress["service_stress task"] --> Load["Background CPU load on core 1"]
    Stress --> Critical["Short critical section"]
    Critical --> Shared["Guarded shared state"]
```
