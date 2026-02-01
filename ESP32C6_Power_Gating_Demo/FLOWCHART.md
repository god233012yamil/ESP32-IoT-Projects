# ESP32-C6 Power Gating Demo - Project Flow Chart

## System Architecture Flow

This flowchart illustrates the complete lifecycle of the ESP32-C6 Power Gating Demo, from boot/wake through task execution to deep sleep.

```mermaid
graph TD
    A[System Boot/Wake] --> B[Initialize Subsystems]
    B --> C[Create Event Groups]
    C --> D[Initialize Bus-Safe GPIO]
    D --> E[Initialize Power Gating]
    E --> F[Start Application Tasks]
    
    F --> G[Measurement Task]
    F --> H[Communication Task]
    F --> I[Power Manager Task]
    
    G --> G1[Enable Gated Rail]
    G1 --> G2[Wait for Rail Stabilization]
    G2 --> G3[Read Sensor Stub]
    G3 --> G4[Set EVT_MEAS_DONE]
    G4 --> G5[Delete Self]
    
    H --> H1[Wait for EVT_MEAS_DONE]
    H1 --> H2[Simulate Data Transmission]
    H2 --> H3[Set EVT_COMM_DONE]
    H3 --> H4[Delete Self]
    
    I --> I1[Wait for EVT_MEAS_DONE<br/>and EVT_COMM_DONE]
    I1 --> I2[Apply Bus-Safe State]
    I2 --> I3[Disable Gated Rail]
    I3 --> I4[Short Delay for Rail Collapse]
    I4 --> I5[Configure Deep Sleep]
    I5 --> I6[Enter Deep Sleep]
    
    I6 -.Timer Wake.-> A
    
    style A fill:#e1f5ff
    style G1 fill:#fff4e6
    style I2 fill:#ffe6e6
    style I3 fill:#ffe6e6
    style I6 fill:#e6f3ff
```

## Detailed Phase Descriptions

### Phase 1: System Initialization (Blue)
- **Boot/Wake**: System starts from power-on reset or timer wake from deep sleep
- **Event Groups**: FreeRTOS synchronization primitives created for inter-task communication
- **Bus-Safe Init**: I2C SCL/SDA pins configured as high-impedance inputs (prevents phantom powering during boot)
- **Power Gating Init**: Enable GPIO configured as output, rail kept in OFF state

### Phase 2: Task Spawning
Three independent FreeRTOS tasks are created with different priorities:
- **Measurement Task** (Priority 5): Handles sensor power and data acquisition
- **Communication Task** (Priority 4): Handles data transmission simulation
- **Power Manager Task** (Priority 6, highest): Orchestrates shutdown sequence

### Phase 3: Measurement Task (Orange)
1. **Enable Gated Rail**: Drive enable GPIO to turn on external power
2. **Stabilization Wait**: Delay for voltage settling and peripheral boot (configurable)
3. **Sensor Read**: Execute stub sensor transaction (hardware-agnostic)
4. **Signal Completion**: Set EVT_MEAS_DONE event bit
5. **Delete Self**: Task cleanup

### Phase 4: Communication Task
1. **Wait for Data**: Block until EVT_MEAS_DONE is set
2. **Simulate Transmission**: Stub for Wi-Fi/BLE data upload
3. **Signal Completion**: Set EVT_COMM_DONE event bit
4. **Delete Self**: Task cleanup

### Phase 5: Power-Down Sequence (Red - Critical!)
1. **Wait for All Tasks**: Block until both EVT_MEAS_DONE and EVT_COMM_DONE are set
2. **Apply Bus-Safe State**: 
   - Set I2C SCL/SDA to INPUT mode
   - Disable internal pull-up/pull-down resistors
   - Prevents back-powering through ESD diodes
3. **Disable Gated Rail**: Drive enable GPIO to turn off external power
4. **Rail Collapse Delay**: Brief wait for voltage decay (helpful for verification)
5. **Configure Deep Sleep**: Set timer wake source and power domain options
6. **Enter Deep Sleep**: CPU and most peripherals powered down

### Phase 6: Sleep Period (Light Blue)
- System in ultra-low-power state
- Only RTC and timer active
- Wake on timer expiration
- Loop back to Phase 1

## Task Synchronization

```mermaid
sequenceDiagram
    participant Main as app_main()
    participant Meas as Measurement Task
    participant Comm as Communication Task
    participant PwrMgr as Power Manager Task
    
    Main->>Meas: xTaskCreate()
    Main->>Comm: xTaskCreate()
    Main->>PwrMgr: xTaskCreate()
    
    Meas->>Meas: pg_set_enabled(true)
    Meas->>Meas: vTaskDelay(stabilize_ms)
    Meas->>Meas: fake_sensor_read()
    Meas->>Comm: xEventGroupSetBits(EVT_MEAS_DONE)
    
    Comm->>Comm: Wait for EVT_MEAS_DONE
    Comm->>Comm: Simulate transmission
    Comm->>PwrMgr: xEventGroupSetBits(EVT_COMM_DONE)
    
    PwrMgr->>PwrMgr: Wait for EVT_MEAS_DONE | EVT_COMM_DONE
    PwrMgr->>PwrMgr: bus_safe_apply_before_power_off()
    PwrMgr->>PwrMgr: pg_set_enabled(false)
    PwrMgr->>PwrMgr: sleep_ctrl_enter_deep_sleep()
    
    Note over PwrMgr: Deep Sleep
    Note over PwrMgr: Timer Wake
```

## Power Gating State Machine

```mermaid
stateDiagram-v2
    [*] --> PowerOff: Boot/Wake
    PowerOff --> RailEnabling: pg_set_enabled(true)
    RailEnabling --> RailStabilizing: GPIO asserted
    RailStabilizing --> RailOn: Delay complete
    RailOn --> SensorActive: Peripheral ready
    SensorActive --> WorkComplete: Data acquired
    WorkComplete --> BusSafe: Apply safe state
    BusSafe --> RailDisabling: pg_set_enabled(false)
    RailDisabling --> PowerOff: GPIO de-asserted
    PowerOff --> DeepSleep: sleep_ctrl_enter()
    DeepSleep --> [*]: Timer wake
    
    note right of BusSafe
        Critical step:
        Prevents phantom
        powering via GPIOs
    end note
```

## GPIO State Transitions

```mermaid
graph LR
    A[Boot: GPIO undefined] --> B[pg_init: GPIO OUTPUT, LOW]
    B --> C[Measurement: GPIO HIGH]
    C --> D[Work Complete]
    D --> E[bus_safe: GPIO INPUT, no pulls]
    E --> F[pg_disable: GPIO OUTPUT, LOW]
    F --> G[Deep Sleep: GPIO hold state]
    G -.Timer Wake.-> B
    
    style E fill:#ffe6e6
    style F fill:#ffe6e6
```

## Power Consumption Profile

```mermaid
graph LR
    A[Deep Sleep<br/>5-20 µA] -->|Timer Wake| B[Boot<br/>~40 mA]
    B -->|pg_init| C[Idle<br/>~15 mA]
    C -->|Rail Enable| D[Active + Sensor<br/>~50 mA]
    D -->|Work Done| E[Active<br/>~40 mA]
    E -->|Bus Safe| F[Active<br/>~40 mA]
    F -->|Rail Disable| G[Active<br/>~15 mA]
    G -->|Sleep Entry| A
    
    style A fill:#e6f3ff
    style D fill:#fff4e6
    style E fill:#ffe6e6
    style F fill:#ffe6e6
```

## Configuration Flow

```mermaid
graph TD
    A[idf.py menuconfig] --> B{Select Technique}
    B -->|REG_EN| C[Regulator EN Pin]
    B -->|LOAD_SWITCH| D[Load Switch]
    B -->|PFET_DRIVER| E[P-FET Driver]
    
    C --> F[Configure Enable GPIO]
    D --> F
    E --> F
    
    F --> G[Set Active Polarity]
    G --> H[Set Stabilize Delay]
    H --> I[Set Wake Interval]
    I --> J[Set Bus GPIOs]
    J --> K[Build & Flash]
    
    K --> L{Hardware Ready?}
    L -->|Yes| M[Monitor Operation]
    L -->|No| N[Check Wiring]
    N --> K
```

## Notes

- **Orange nodes**: Active power consumption phases
- **Red nodes**: Critical safety operations (bus-safe, rail disable)
- **Blue nodes**: Low power states
- **Dashed arrows**: Asynchronous wake events
- **Event bits**: FreeRTOS synchronization primitives ensure tasks complete before power-down
