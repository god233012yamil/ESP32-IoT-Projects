# System Flowchart

This document contains a comprehensive Mermaid flowchart illustrating the complete flow of the ESP32-S3 Event Groups demonstration project.

## Complete System Flow

```mermaid
graph TD
    Start([app_main Entry]) --> CreateEG[Create Event Group g_evt]
    CreateEG --> SpawnTasks[Spawn All Tasks]
    
    SpawnTasks --> InitTask[init_task Priority:8]
    SpawnTasks --> ADCTask[adc_task Priority:6]
    SpawnTasks --> TempTask[temp_task Priority:6]
    SpawnTasks --> GPIOTask[gpio_task Priority:6]
    SpawnTasks --> AggTask[aggregator_task Priority:7]
    SpawnTasks --> DiagTask[diagnostics_task Priority:5]
    
    %% Init Task Flow
    InitTask --> InitGPIO{Initialize GPIO<br/>DEMO_GPIO_INPUT}
    InitGPIO -->|Success| SetGPIOInit[Set EVT_GPIO_INIT bit]
    InitGPIO -->|Failure| SkipGPIOInit[Skip GPIO init bit]
    
    SetGPIOInit --> InitADC{Initialize ADC<br/>Oneshot Driver}
    SkipGPIOInit --> InitADC
    
    InitADC -->|Success| SetADCInit[Set EVT_ADC_INIT bit]
    InitADC -->|Failure| SkipADCInit[Skip ADC init bit]
    
    SetADCInit --> SimI2C[Simulate I2C Init<br/>Delay 150ms]
    SkipADCInit --> SimI2C
    
    SimI2C --> SetI2CInit[Set EVT_I2C_INIT bit]
    SetI2CInit --> SimNet[Simulate Network Init<br/>Delay 250ms]
    SimNet --> SetNetInit[Set EVT_NET_INIT bit]
    SetNetInit --> InitDone[Self-Delete Task]
    
    %% ADC Task Flow
    ADCTask --> WaitADCInit[Wait for EVT_ADC_INIT<br/>Block: portMAX_DELAY]
    WaitADCInit --> ADCLoop{ADC Task Loop}
    ADCLoop --> ReadADC[Read ADC Channel 0<br/>adc_oneshot_read]
    ReadADC -->|Success| StoreADC[Store to g_last_adc_raw]
    ReadADC -->|Failure| SkipADCSet[Skip setting bit]
    StoreADC --> SetADCReady[Set EVT_ADC_READY bit]
    SetADCReady --> ADCDelay[Delay 500ms]
    SkipADCSet --> ADCDelay
    ADCDelay --> ADCLoop
    
    %% Temperature Task Flow
    TempTask --> WaitI2CInit[Wait for EVT_I2C_INIT<br/>Block: portMAX_DELAY]
    WaitI2CInit --> TempLoop{Temp Task Loop}
    TempLoop --> GenTemp[Generate Simulated<br/>Temperature 22-28°C]
    GenTemp --> StoreTemp[Store to g_last_temp_c]
    StoreTemp --> SetTempReady[Set EVT_TEMP_READY bit]
    SetTempReady --> TempDelay[Delay 1000ms]
    TempDelay --> TempLoop
    
    %% GPIO Task Flow
    GPIOTask --> WaitGPIOInit[Wait for EVT_GPIO_INIT<br/>Block: portMAX_DELAY]
    WaitGPIOInit --> GPIOLoop{GPIO Task Loop}
    GPIOLoop --> ReadGPIO[Read GPIO Level<br/>gpio_get_level]
    ReadGPIO --> StoreGPIO[Store to g_last_gpio_lvl]
    StoreGPIO --> CheckStable{Same as Last<br/>3 Times?}
    CheckStable -->|Yes| SetGPIOReady[Set EVT_GPIO_READY bit<br/>Reset counter]
    CheckStable -->|No| ResetCounter[Reset Stability Counter]
    SetGPIOReady --> GPIODelay[Delay 100ms]
    ResetCounter --> GPIODelay
    GPIODelay --> GPIOLoop
    
    %% Aggregator Task Flow
    AggTask --> WaitAllInit[Wait for ALL Init Bits<br/>EVT_ALL_INIT_MASK<br/>Block: portMAX_DELAY]
    WaitAllInit --> AggLoop{Aggregator Loop}
    AggLoop --> WaitAllData[Wait for ALL Data Bits<br/>EVT_ALL_DATA_MASK<br/>Auto-clear: YES<br/>Timeout: 3000ms]
    WaitAllData -->|All Bits Set| CheckBits{All Bits<br/>Received?}
    WaitAllData -->|Timeout| LogTimeout[Log: Aggregator timeout]
    CheckBits -->|Yes| BuildPayload[Build JSON Payload<br/>timestamp, adc, temp, gpio]
    CheckBits -->|No| LogTimeout
    BuildPayload --> LogPayload[ESP_LOGI Payload]
    LogPayload --> AggDelay[Delay 200ms]
    LogTimeout --> AggLoop
    AggDelay --> AggLoop
    
    %% Diagnostics Task Flow
    DiagTask --> WaitAllInit2[Wait for ALL Init Bits<br/>EVT_ALL_INIT_MASK<br/>Block: portMAX_DELAY]
    WaitAllInit2 --> DiagLoop{Diagnostics Loop}
    DiagLoop --> WaitAnyData[Wait for ANY Data Bit<br/>EVT_ANY_DATA_MASK<br/>Auto-clear: NO<br/>Timeout: 5000ms]
    WaitAnyData -->|Bits Set| CheckNoBits{Any Bits<br/>Detected?}
    WaitAnyData -->|Timeout| LogNoEvent[Log: No data events]
    CheckNoBits -->|Yes| LogEvents[Log Individual Events:<br/>ADC/Temp/GPIO Ready]
    CheckNoBits -->|No| LogNoEvent
    LogEvents --> DiagDelay[Delay 100ms]
    LogNoEvent --> DiagLoop
    DiagDelay --> DiagLoop
    
    %% Styling
    classDef initClass fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    classDef taskClass fill:#fff3e0,stroke:#e65100,stroke-width:2px
    classDef eventClass fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    classDef decisionClass fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    classDef dataClass fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px
    
    class Start,CreateEG,SpawnTasks initClass
    class InitTask,ADCTask,TempTask,GPIOTask,AggTask,DiagTask taskClass
    class SetGPIOInit,SetADCInit,SetI2CInit,SetNetInit,SetADCReady,SetTempReady,SetGPIOReady eventClass
    class InitGPIO,InitADC,CheckStable,CheckBits,CheckNoBits decisionClass
    class StoreADC,StoreTemp,StoreGPIO,BuildPayload dataClass
```

## Task Interaction Diagram

```mermaid
sequenceDiagram
    participant Main as app_main
    participant EG as Event Group
    participant Init as init_task
    participant ADC as adc_task
    participant Temp as temp_task
    participant GPIO as gpio_task
    participant Agg as aggregator_task
    participant Diag as diagnostics_task
    
    Main->>EG: Create Event Group
    Main->>Init: Spawn (P:8)
    Main->>ADC: Spawn (P:6)
    Main->>Temp: Spawn (P:6)
    Main->>GPIO: Spawn (P:6)
    Main->>Agg: Spawn (P:7)
    Main->>Diag: Spawn (P:5)
    
    Note over ADC,Diag: All tasks wait for init
    
    Init->>EG: Set EVT_GPIO_INIT
    Init->>EG: Set EVT_ADC_INIT
    Init->>EG: Set EVT_I2C_INIT (delay 150ms)
    Init->>EG: Set EVT_NET_INIT (delay 250ms)
    Init->>Init: Self-Delete
    
    Note over ADC,GPIO: Producer tasks start
    
    ADC->>EG: Wait EVT_ADC_INIT
    EG-->>ADC: Released
    
    Temp->>EG: Wait EVT_I2C_INIT
    EG-->>Temp: Released
    
    GPIO->>EG: Wait EVT_GPIO_INIT
    EG-->>GPIO: Released
    
    Agg->>EG: Wait EVT_ALL_INIT_MASK
    EG-->>Agg: All init bits set
    
    Diag->>EG: Wait EVT_ALL_INIT_MASK
    EG-->>Diag: All init bits set
    
    Note over ADC,Diag: Operational Phase
    
    loop Every 500ms
        ADC->>ADC: Read ADC
        ADC->>EG: Set EVT_ADC_READY
        Diag->>EG: Detect ANY bit (OR)
        Diag->>Diag: Log "ADC ready"
    end
    
    loop Every 1000ms
        Temp->>Temp: Read Temperature
        Temp->>EG: Set EVT_TEMP_READY
        Diag->>EG: Detect ANY bit (OR)
        Diag->>Diag: Log "Temp ready"
    end
    
    loop Every 100ms
        GPIO->>GPIO: Read & Debounce
        GPIO->>EG: Set EVT_GPIO_READY (if stable)
        Diag->>EG: Detect ANY bit (OR)
        Diag->>Diag: Log "GPIO ready"
    end
    
    Agg->>EG: Wait EVT_ALL_DATA_MASK (AND, auto-clear)
    
    Note over ADC,GPIO: All 3 bits eventually set
    
    EG-->>Agg: All data bits set
    Agg->>Agg: Read g_last_adc_raw
    Agg->>Agg: Read g_last_temp_c
    Agg->>Agg: Read g_last_gpio_lvl
    Agg->>Agg: Build JSON payload
    Agg->>Agg: Log payload
    
    Note over Agg: Bits auto-cleared, loop continues
```

## Event Bit State Machine

```mermaid
stateDiagram-v2
    [*] --> SystemBoot
    
    SystemBoot --> InitPhase: Event Group Created
    
    state InitPhase {
        [*] --> WaitingGPIO
        [*] --> WaitingADC
        [*] --> WaitingI2C
        [*] --> WaitingNetwork
        
        WaitingGPIO --> GPIOReady: EVT_GPIO_INIT set
        WaitingADC --> ADCReady: EVT_ADC_INIT set
        WaitingI2C --> I2CReady: EVT_I2C_INIT set
        WaitingNetwork --> NetworkReady: EVT_NET_INIT set
        
        GPIOReady --> AllInitComplete
        ADCReady --> AllInitComplete
        I2CReady --> AllInitComplete
        NetworkReady --> AllInitComplete
    }
    
    InitPhase --> OperationalPhase: EVT_ALL_INIT_MASK satisfied
    
    state OperationalPhase {
        [*] --> WaitingData
        
        WaitingData --> ADCDataReady: EVT_ADC_READY set
        WaitingData --> TempDataReady: EVT_TEMP_READY set
        WaitingData --> GPIODataReady: EVT_GPIO_READY set
        
        ADCDataReady --> CheckAllData
        TempDataReady --> CheckAllData
        GPIODataReady --> CheckAllData
        
        CheckAllData --> AllDataReady: All 3 bits set
        CheckAllData --> WaitingData: Not all bits set
        
        AllDataReady --> DataAggregated: Aggregator processes
        DataAggregated --> WaitingData: Bits cleared (auto)
    }
    
    note right of OperationalPhase
        Diagnostics task monitors
        using OR logic (any bit)
        without clearing bits
    end note
```

## Priority and Synchronization Overview

```mermaid
graph LR
    subgraph "Priority Levels"
        P8[Priority 8<br/>init_task]
        P7[Priority 7<br/>aggregator_task]
        P6A[Priority 6<br/>adc_task]
        P6B[Priority 6<br/>temp_task]
        P6C[Priority 6<br/>gpio_task]
        P5[Priority 5<br/>diagnostics_task]
    end
    
    subgraph "Event Group Bits"
        INIT[Init Bits 0-3<br/>GPIO, ADC, I2C, NET]
        DATA[Data Bits 8-10<br/>ADC, TEMP, GPIO]
    end
    
    P8 -->|Sets| INIT
    P6A -->|Waits then Sets| INIT
    P6A -->|Sets| DATA
    P6B -->|Waits then Sets| INIT
    P6B -->|Sets| DATA
    P6C -->|Waits then Sets| INIT
    P6C -->|Sets| DATA
    P7 -->|Waits AND| INIT
    P7 -->|Waits AND + Clear| DATA
    P5 -->|Waits AND| INIT
    P5 -->|Waits OR| DATA
    
    style P8 fill:#ff6b6b
    style P7 fill:#ffa500
    style P6A fill:#ffff00
    style P6B fill:#ffff00
    style P6C fill:#ffff00
    style P5 fill:#90ee90
    style INIT fill:#e1f5ff
    style DATA fill:#f3e5f5
```

## Notes

### Symbol Legend

- **Rectangle**: Process or action
- **Diamond**: Decision point
- **Rounded Rectangle**: Start/End point
- **Arrows**: Flow direction

### Color Coding

- **Blue** (Light): Initialization processes
- **Orange** (Light): Task operations
- **Purple** (Light): Event setting operations
- **Yellow** (Light): Decision points
- **Green** (Light): Data storage operations

### Key Synchronization Points

1. **Startup Barrier**: All tasks wait for `EVT_ALL_INIT_MASK` before operational phase
2. **Producer Signaling**: Data tasks set individual ready bits periodically
3. **AND Aggregation**: Aggregator waits for ALL data bits (cleared after read)
4. **OR Monitoring**: Diagnostics wakes on ANY data bit (bits not cleared)

### Timing Characteristics

- **ADC Sampling**: 500ms period
- **Temperature Updates**: 1000ms period
- **GPIO Monitoring**: 100ms period
- **Aggregator Timeout**: 3000ms
- **Diagnostics Timeout**: 5000ms
- **I2C Init Simulation**: 150ms delay
- **Network Init Simulation**: 250ms delay
