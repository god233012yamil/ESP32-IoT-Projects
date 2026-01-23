# ESP32 Low-Power Reference Project Flowchart

This document contains the system architecture flowchart showing the complete firmware execution flow.

## Main Application Flow

```mermaid
flowchart TD
    A[Power On / Reset] --> B[app_main]
    B --> C[Enable Power Management<br/>DFS + Light Sleep]
    C --> D[Initialize Sensor Power GPIO]
    D --> E[Check Wakeup Cause]
    E --> F[Configure Deep Sleep Wake Sources<br/>Timer + Optional GPIO]
    F --> G[Create Event-Driven Button Task]
    G --> H[Initialize Runtime GPIO Interrupt]
    H --> I[Perform Work Burst]
    
    I --> J{Wi-Fi Enabled?}
    J -->|Yes| K[Power On Sensor]
    J -->|No| K
    K --> L[Read Sensor Data]
    L --> M[Power Off Sensor]
    
    M --> N{Wi-Fi Enabled?}
    N -->|Yes| O[wifi_manager_connect]
    N -->|No| Q
    
    O --> P{Connected?}
    P -->|Yes| Q[wifi_manager_demo_tx<br/>TCP Transaction]
    P -->|No| R[wifi_manager_shutdown]
    Q --> R
    
    R --> S[Delay for Log Flush<br/>50ms]
    S --> T[Enter Deep Sleep]
    
    T --> U{Wake Event}
    U -->|Timer Expired| V[Wake from Deep Sleep]
    U -->|GPIO Trigger| V
    
    V --> E
    
    W[Button ISR] -.->|Task Notification| X[Button Task Wakes]
    X --> Y[do_work_burst]
    Y --> Z[Task Blocks Again]
    Z -.-> W
    
    style T fill:#ff6b6b
    style C fill:#4ecdc4
    style O fill:#ffe66d
    style I fill:#95e1d3
```

## Wi-Fi Manager Lifecycle

```mermaid
flowchart LR
    A[wifi_manager_connect] --> B[Init Wi-Fi Once]
    B --> C[Create Event Group]
    C --> D[Set Wi-Fi Config]
    D --> E[Enable Modem Sleep]
    E --> F[Start Wi-Fi]
    F --> G[Wait for Event]
    
    G --> H{Connected?}
    H -->|Yes| I[Return ESP_OK]
    H -->|Timeout| J[Return ESP_ERR_TIMEOUT]
    H -->|Failed| K[Return ESP_FAIL]
    
    I --> L[wifi_manager_demo_tx]
    L --> M[DNS Lookup]
    M --> N[Create Socket]
    N --> O[Connect TCP]
    O --> P[Close Socket]
    
    P --> Q[wifi_manager_shutdown]
    Q --> R[Disconnect]
    R --> S[Stop Wi-Fi]
    S --> T[Deinit Wi-Fi]
    
    style A fill:#ffe66d
    style Q fill:#ff6b6b
    style I fill:#95e1d3
```

## Power State Transitions

```mermaid
stateDiagram-v2
    [*] --> Active: Power On / Wake
    
    Active --> LightSleep: Task Blocks<br/>(PM enabled)
    LightSleep --> Active: Interrupt / Event
    
    Active --> DeepSleep: esp_deep_sleep_start()
    DeepSleep --> Active: Timer Wake
    DeepSleep --> Active: GPIO Wake (EXT0)
    
    Active --> [*]: Shutdown
    
    note right of DeepSleep
        Current: 10-50 µA
        Duration: 99%+ of time
    end note
    
    note right of Active
        Current: 80-240 mA
        Duration: < 1% of time
    end note
    
    note right of LightSleep
        Current: 0.8-2 mA
        Automatic when idle
    end note
```

## Event-Driven Task Pattern

```mermaid
sequenceDiagram
    participant ISR as Button ISR
    participant RTOS as FreeRTOS
    participant Task as Button Task
    participant Work as do_work_burst()
    
    Task->>RTOS: ulTaskNotifyTake(portMAX_DELAY)
    Note over Task,RTOS: Task blocks, CPU can sleep
    
    ISR->>RTOS: vTaskNotifyGiveFromISR()
    Note over ISR,RTOS: Button pressed
    
    RTOS->>Task: Wake task
    Task->>Work: Execute work burst
    Work-->>Task: Complete
    
    Task->>RTOS: ulTaskNotifyTake(portMAX_DELAY)
    Note over Task,RTOS: Task blocks again
```

## Timing Diagram

```mermaid
gantt
    title ESP32 Low-Power Duty Cycle (300s period)
    dateFormat X
    axisFormat %S
    
    section Power States
    Deep Sleep (20µA)   :done, sleep1, 0, 297000
    Wake & Init         :active, wake, 297000, 297500
    Sensor Read         :active, sensor, 297500, 297510
    Wi-Fi Connect       :crit, wifi1, 297510, 300000
    Wi-Fi TX            :crit, wifitx, 300000, 300500
    Wi-Fi Shutdown      :active, wifi2, 300500, 300600
    Sleep Entry         :active, entry, 300600, 300650
    Deep Sleep (20µA)   :done, sleep2, 300650, 597650
```

## Legend

| Color | Meaning |
|-------|---------|
| 🟢 Green | Normal operation, low power |
| 🟡 Yellow | Wi-Fi operations, medium power |
| 🔴 Red | Critical state (deep sleep entry) |
| 🔵 Blue | Power management enabled |

## Notes

- **Solid lines** represent actual code execution flow
- **Dashed lines** represent interrupt/notification mechanisms
- **Decision diamonds** represent conditional logic
- **Colored boxes** highlight power-critical states

## Usage

To render these diagrams:
1. Use [Mermaid Live Editor](https://mermaid.live/)
2. Use GitHub (renders automatically)
3. Use VS Code with Mermaid extension
4. Generate PNG/SVG using Mermaid CLI

```bash
# Install Mermaid CLI
npm install -g @mermaid-js/mermaid-cli

# Generate PNG
mmdc -i FLOWCHART.md -o flowchart.png
```
