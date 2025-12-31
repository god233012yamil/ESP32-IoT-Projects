# Project Flowchart

This document contains a detailed flowchart of the ESP-IDF FreeRTOS Synchronization Demo using Mermaid syntax.

## Complete System Flow

```mermaid
graph TD
    Start([ESP32 Boot]) --> AppMain[app_main Function]
    
    AppMain --> CreatePrimitives[Create Synchronization Primitives]
    CreatePrimitives --> CreateMutex[Create I2C Mutex]
    CreateMutex --> CreateBinarySem[Create Binary Semaphore for GPIO]
    CreateBinarySem --> CreateCountingSem[Create Counting Semaphore<br/>Count: BUFFER_POOL_SIZE = 3]
    
    CreateCountingSem --> CheckPrimitives{All Primitives<br/>Created OK?}
    CheckPrimitives -->|No| Error[Log Error & Exit]
    CheckPrimitives -->|Yes| InitI2C[Initialize I2C Driver]
    
    InitI2C --> CheckI2C{I2C Init<br/>Success?}
    CheckI2C -->|No| WarnI2C[Log Warning<br/>Continue Anyway]
    CheckI2C -->|Yes| InitGPIO[Initialize GPIO]
    WarnI2C --> InitGPIO
    
    InitGPIO --> ConfigGPIO[Configure GPIO0 as Input<br/>Pull-up Enabled<br/>Falling Edge Interrupt]
    ConfigGPIO --> InstallISR[Install GPIO ISR Service]
    InstallISR --> AttachHandler[Attach ISR Handler to GPIO0]
    
    AttachHandler --> CreateTasks[Create FreeRTOS Tasks]
    CreateTasks --> Task1[Create i2c_task_sensor<br/>Priority: 5]
    Task1 --> Task2[Create i2c_task_eeprom<br/>Priority: 5]
    Task2 --> Task3[Create gpio_event_task<br/>Priority: 10]
    Task3 --> Task4[Create 5 Worker Tasks<br/>Priority: 4]
    
    Task4 --> LogReady[Log: Tasks Started]
    LogReady --> Running[System Running]
    
    Running --> ParallelExec[Parallel Task Execution]
    
    ParallelExec --> I2CSensor[I2C Sensor Task]
    ParallelExec --> I2CEEPROM[I2C EEPROM Task]
    ParallelExec --> GPIOTask[GPIO Event Task]
    ParallelExec --> Workers[Worker Tasks 0-4]
    
    style Start fill:#e1f5e1
    style Error fill:#ffe1e1
    style Running fill:#e1f0ff
    style ParallelExec fill:#fff4e1
```

## I2C Sensor Task Flow (Mutex Pattern)

```mermaid
graph TD
    SensorStart([i2c_task_sensor Start]) --> SensorLoop{Infinite Loop}
    
    SensorLoop --> TakeMutex[Attempt to Take I2C Mutex<br/>Timeout: 500ms]
    
    TakeMutex --> MutexSuccess{Mutex<br/>Acquired?}
    
    MutexSuccess -->|Yes| LockLog[Log: I2C SENSOR: bus locked]
    LockLog --> CreateCmd[Create I2C Command Link]
    CreateCmd --> AddStart[Add Start Condition]
    AddStart --> AddAddr[Write Device Address 0x48<br/>+ Write Bit]
    AddAddr --> AddStop[Add Stop Condition]
    AddStop --> ExecuteCmd[Execute I2C Command<br/>Timeout: 20ms]
    ExecuteCmd --> DeleteCmd[Delete Command Link]
    DeleteCmd --> ReleaseLog[Log: I2C SENSOR: bus released]
    ReleaseLog --> GiveMutex[Release I2C Mutex]
    GiveMutex --> Delay1000[Delay 1000ms]
    
    MutexSuccess -->|No - Timeout| TimeoutLog[Log: Failed to lock bus]
    TimeoutLog --> Delay1000
    
    Delay1000 --> SensorLoop
    
    style SensorStart fill:#e1f5e1
    style GiveMutex fill:#ffe1e1
```

## I2C EEPROM Task Flow (Mutex Pattern)

```mermaid
graph TD
    EEPROMStart([i2c_task_eeprom Start]) --> EEPROMLoop{Infinite Loop}
    
    EEPROMLoop --> TakeMutex[Attempt to Take I2C Mutex<br/>Timeout: 500ms]
    
    TakeMutex --> MutexSuccess{Mutex<br/>Acquired?}
    
    MutexSuccess -->|Yes| LockLog[Log: I2C EEPROM: bus locked]
    LockLog --> CreateCmd[Create I2C Command Link]
    CreateCmd --> AddStart[Add Start Condition]
    AddStart --> AddAddr[Write Device Address 0x50<br/>+ Write Bit]
    AddAddr --> WriteData1[Write Byte: 0x00]
    WriteData1 --> WriteData2[Write Byte: 0xAA]
    WriteData2 --> AddStop[Add Stop Condition]
    AddStop --> ExecuteCmd[Execute I2C Command<br/>Timeout: 20ms]
    ExecuteCmd --> DeleteCmd[Delete Command Link]
    DeleteCmd --> ReleaseLog[Log: I2C EEPROM: bus released]
    ReleaseLog --> GiveMutex[Release I2C Mutex]
    GiveMutex --> Delay2000[Delay 2000ms]
    
    MutexSuccess -->|No - Timeout| TimeoutLog[Log: Failed to lock bus]
    TimeoutLog --> Delay2000
    
    Delay2000 --> EEPROMLoop
    
    style EEPROMStart fill:#e1f5e1
    style GiveMutex fill:#ffe1e1
```

## GPIO Interrupt Flow (Binary Semaphore Pattern)

```mermaid
graph TD
    HWEvent[Hardware Event:<br/>BOOT Button Pressed] --> GPIOInt[GPIO0 Falling Edge<br/>Interrupt Triggered]
    
    GPIOInt --> ISR[gpio_isr_handler Executes<br/>in IRAM]
    
    ISR --> CheckSem{Binary Semaphore<br/>Created?}
    CheckSem -->|Yes| GiveSem[Give Binary Semaphore<br/>from ISR]
    CheckSem -->|No| ISRExit
    
    GiveSem --> CheckYield{Higher Priority<br/>Task Woken?}
    CheckYield -->|Yes| Yield[Yield to Higher<br/>Priority Task]
    CheckYield -->|No| ISRExit[ISR Exit]
    Yield --> ISRExit
    
    ISRExit -.Task Switch.-> TaskWake[gpio_event_task Wakes Up]
    
    TaskStart([gpio_event_task Start]) --> TaskLoop{Infinite Loop}
    TaskLoop --> BlockSem[Block on Binary Semaphore<br/>Timeout: Infinite]
    
    BlockSem -.Waiting.-> TaskWake
    
    TaskWake --> Debounce[Delay 40ms<br/>for Debouncing]
    Debounce --> ReadGPIO[Read GPIO0 Level]
    ReadGPIO --> LogEvent[Log: GPIO EVENT: ISR signaled<br/>gpio=0 level=X]
    LogEvent --> TaskLoop
    
    style HWEvent fill:#fff4e1
    style ISR fill:#ffe1e1
    style TaskStart fill:#e1f5e1
    style Yield fill:#e1f0ff
```

## Worker Task Flow (Counting Semaphore Pattern)

```mermaid
graph TD
    WorkerStart([worker_task Start<br/>ID: 0-4]) --> WorkerLoop{Infinite Loop}
    
    WorkerLoop --> TakePool[Attempt to Take Pool Semaphore<br/>Timeout: 1000ms]
    
    TakePool --> PoolSuccess{Semaphore<br/>Acquired?}
    
    PoolSuccess -->|Yes - Slot Available| AcquireLog[Log: WORKER X: acquired pool slot]
    AcquireLog --> SimulateWork[Simulate Work<br/>Delay 600ms]
    SimulateWork --> ReleaseLog[Log: WORKER X: releasing pool slot]
    ReleaseLog --> GivePool[Give Pool Semaphore<br/>Increment Available Count]
    GivePool --> StaggerDelay[Delay: 200 + ID × 50 ms<br/>Stagger Next Attempt]
    
    PoolSuccess -->|No - Timeout<br/>All 3 Slots Busy| TimeoutLog[Log: WORKER X: timed out]
    TimeoutLog --> StaggerDelay
    
    StaggerDelay --> WorkerLoop
    
    style WorkerStart fill:#e1f5e1
    style GivePool fill:#ffe1e1
    style TimeoutLog fill:#fff4e1
```

## Resource Pool State Diagram

```mermaid
stateDiagram-v2
    [*] --> Available3: Initialize Counting Semaphore<br/>Count = 3
    
    Available3 --> Available2: Worker takes semaphore
    Available2 --> Available1: Worker takes semaphore
    Available1 --> Available0: Worker takes semaphore
    
    Available0 --> Available1: Worker releases semaphore
    Available1 --> Available2: Worker releases semaphore
    Available2 --> Available3: Worker releases semaphore
    
    Available3 --> Available2: Worker takes semaphore
    Available2 --> Available3: Worker releases semaphore
    
    Available1 --> Available0: Worker takes semaphore
    Available0 --> Available1: Worker releases semaphore
    
    note right of Available0
        Pool Full: 3 Workers Active
        Workers 4 & 5 Blocked/Timeout
    end note
    
    note right of Available3
        Pool Empty: 0 Workers Active
        All Semaphore Slots Available
    end note
```

## Synchronization Primitive Interaction Timeline

```mermaid
sequenceDiagram
    participant App as app_main
    participant ISensor as I2C Sensor Task
    participant IEEPROM as I2C EEPROM Task
    participant Mutex as I2C Mutex
    participant Button as BOOT Button
    participant ISR as GPIO ISR
    participant BinSem as Binary Semaphore
    participant GTask as GPIO Event Task
    participant W0 as Worker 0
    participant W3 as Worker 3
    participant CSem as Counting Semaphore<br/>(Count: 3)
    
    App->>Mutex: Create Mutex
    App->>BinSem: Create Binary Semaphore
    App->>CSem: Create Counting Semaphore (3)
    App->>ISensor: Create Task (Priority 5)
    App->>IEEPROM: Create Task (Priority 5)
    App->>GTask: Create Task (Priority 10)
    App->>W0: Create Worker 0-4 (Priority 4)
    
    Note over ISensor,IEEPROM: Mutex Pattern
    ISensor->>Mutex: Take (500ms timeout)
    Mutex-->>ISensor: Granted
    Note over ISensor: Execute I2C Transaction
    ISensor->>Mutex: Give
    
    IEEPROM->>Mutex: Take (500ms timeout)
    Note over IEEPROM: Wait - Mutex Busy
    Mutex-->>IEEPROM: Granted
    Note over IEEPROM: Execute I2C Transaction
    IEEPROM->>Mutex: Give
    
    Note over Button,GTask: Binary Semaphore Pattern
    Button->>ISR: Interrupt (Falling Edge)
    ISR->>BinSem: Give from ISR
    ISR-->>GTask: Wake Task
    GTask->>BinSem: Take (infinite wait)
    BinSem-->>GTask: Granted
    Note over GTask: Process Event
    
    Note over W0,CSem: Counting Semaphore Pattern
    W0->>CSem: Take (1000ms timeout)
    CSem-->>W0: Granted (Count: 3→2)
    W0->>CSem: Take (1000ms timeout)
    CSem-->>W0: Granted (Count: 2→1)
    W0->>CSem: Take (1000ms timeout)
    CSem-->>W0: Granted (Count: 1→0)
    
    W3->>CSem: Take (1000ms timeout)
    Note over W3: Wait - Pool Full (Count: 0)
    Note over CSem: Timeout after 1000ms
    CSem-->>W3: Timeout
    
    Note over W0: Work Complete (600ms)
    W0->>CSem: Give
    CSem-->>CSem: Count: 0→1
    
    W3->>CSem: Take (1000ms timeout)
    CSem-->>W3: Granted (Count: 1→0)
```

## Task State Transitions

```mermaid
stateDiagram-v2
    [*] --> Ready: Task Created
    
    Ready --> Running: Scheduler Selects Task
    Running --> Ready: Preempted by Higher Priority
    Running --> Blocked: Wait on Semaphore/Mutex
    Running --> Delayed: vTaskDelay() Called
    
    Blocked --> Ready: Semaphore/Mutex Available
    Blocked --> Ready: Timeout Expired
    Delayed --> Ready: Delay Period Expired
    
    Ready --> Running: Scheduler Selects Task
    
    note right of Blocked
        I2C Tasks: Waiting on Mutex
        GPIO Task: Waiting on Binary Sem
        Workers: Waiting on Counting Sem
    end note
    
    note right of Delayed
        I2C Sensor: 1000ms
        I2C EEPROM: 2000ms
        Workers: 200-450ms (staggered)
        GPIO Task: 40ms (debounce)
    end note
```

## Priority and Scheduling

```mermaid
graph LR
    subgraph "Task Priority Levels"
        P10[Priority 10<br/>GPIO Event Task<br/>Highest Priority]
        P5[Priority 5<br/>I2C Sensor Task<br/>I2C EEPROM Task]
        P4[Priority 4<br/>Worker Tasks 0-4<br/>Lowest Priority]
    end
    
    P10 -.Preempts.-> P5
    P10 -.Preempts.-> P4
    P5 -.Preempts.-> P4
    
    style P10 fill:#ffe1e1
    style P5 fill:#fff4e1
    style P4 fill:#e1f0ff
```

## Key Observations from Flowcharts

### Mutex Behavior
- **Only one task** can hold the I2C mutex at any time
- **Priority inheritance** prevents priority inversion
- **Timeout-based** acquisition prevents deadlock
- **Alternating access** between sensor and EEPROM tasks

### Binary Semaphore Behavior
- **ISR gives** semaphore (safe from interrupt context)
- **Task takes** semaphore (blocks until signaled)
- **Event-driven** processing (immediate response to button press)
- **Minimal ISR time** (processing deferred to task)

### Counting Semaphore Behavior
- **Initial count: 3** (three available resources)
- **Decrement on take**, **increment on give**
- **Maximum 3 concurrent** workers active
- **Workers 4-5 block or timeout** when pool is full
- **Fair scheduling** ensures all workers eventually get access

### System Design Patterns
1. **Mutex** → Resource protection (shared I2C bus)
2. **Binary Semaphore** → Event notification (ISR to task)
3. **Counting Semaphore** → Resource pooling (limited slots)
