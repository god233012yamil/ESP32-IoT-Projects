# Project Flow Diagrams

This document contains detailed Mermaid flowcharts for the ESP32 Scheduling Demo project.

## Overall System Flow

```mermaid
flowchart TD
    Start([ESP32 Boot]) --> AppMain[app_main]
    AppMain --> CheckMode{Check<br/>CONFIG Mode}
    
    CheckMode -->|PREEMPTIVE| PreemptLog[Log: Mode PREEMPTIVE]
    CheckMode -->|COOPERATIVE| CoopLog[Log: Mode COOPERATIVE]
    CheckMode -->|None| ErrorLog[Log: No mode selected]
    
    PreemptLog --> StartPreempt[start_preemptive_demo]
    CoopLog --> StartCoop[start_cooperative_demo]
    ErrorLog --> End([End])
    
    StartPreempt --> PreemptFlow
    StartCoop --> CoopFlow
    
    subgraph PreemptFlow[Preemptive Mode Flow]
        direction TB
        PM1[Create Mutex] --> PM2{Mutex OK?}
        PM2 -->|No| PMErr[Log Error & Return]
        PM2 -->|Yes| PM3[Create Sensor Task<br/>Priority: 5]
        PM3 --> PM4[Create Network Task<br/>Priority: 4]
        PM4 --> PM5[Create High Priority Task<br/>Priority: 8]
        PM5 --> PM6[Log: Demo Started]
        PM6 --> PMRun[Tasks Running...]
    end
    
    subgraph CoopFlow[Cooperative Mode Flow]
        direction TB
        CM1[Create Event Queue<br/>Size: CONFIG_COOP_EVENT_QUEUE_LEN] --> CM2{Queue OK?}
        CM2 -->|No| CMErr[Log Error & Return]
        CM2 -->|Yes| CM3[Create Periodic Timer<br/>Period: CONFIG_COOP_TIMER_PERIOD_MS]
        CM3 --> CM4{Timer OK?}
        CM4 -->|No| CMErr2[Log Error & Return]
        CM4 -->|Yes| CM5[Create Main Loop Task<br/>Priority: 5]
        CM5 --> CM6[Start Timer]
        CM6 --> CM7{Timer Started?}
        CM7 -->|No| CMErr3[Log Error & Return]
        CM7 -->|Yes| CM8[Log: Demo Started]
        CM8 --> CMRun[Event Loop Running...]
    end
    
    style Start fill:#90EE90
    style End fill:#FFB6C1
    style PreemptLog fill:#FFE4B5
    style CoopLog fill:#ADD8E6
    style ErrorLog fill:#FF6B6B
```

## Preemptive Mode - Detailed Task Flow

```mermaid
flowchart TD
    subgraph SensorTask[Sensor Task - Priority 5]
        ST1([Task Start]) --> ST2[Wait 500ms]
        ST2 --> ST3[CPU Work<br/>200k iterations]
        ST3 --> ST4[Acquire Mutex]
        ST4 --> ST5[Increment Counter +1]
        ST5 --> ST6[Read Counter Value]
        ST6 --> ST7[Release Mutex]
        ST7 --> ST8[Log Counter Value]
        ST8 --> ST2
    end
    
    subgraph NetworkTask[Network Task - Priority 4]
        NT1([Task Start]) --> NT2[Wait 800ms]
        NT2 --> NT3[CPU Work<br/>350k iterations]
        NT3 --> NT4[Acquire Mutex]
        NT4 --> NT5[Increment Counter +2]
        NT5 --> NT6[Read Counter Value]
        NT6 --> NT7[Release Mutex]
        NT7 --> NT8[Log Counter Value]
        NT8 --> NT2
    end
    
    subgraph HighPrioTask[High Priority Task - Priority 8]
        HT1([Task Start]) --> HT2[Wait 1500ms]
        HT2 --> HT3[CPU Work<br/>250k iterations]
        HT3 --> HT4[Acquire Mutex]
        HT4 --> HT5[Increment Counter +10]
        HT5 --> HT6[Read Counter Value]
        HT6 --> HT7[Release Mutex]
        HT7 --> HT8[Log: BURST Event]
        HT8 --> HT2
    end
    
    subgraph Scheduler[FreeRTOS Scheduler]
        Sched{Priority<br/>Decision}
    end
    
    subgraph SharedResource[Shared Resource]
        Mutex[Mutex Lock]
        Counter[(Shared Counter)]
        Mutex -.protects.-> Counter
    end
    
    ST4 --> Mutex
    ST7 --> Mutex
    NT4 --> Mutex
    NT7 --> Mutex
    HT4 --> Mutex
    HT7 --> Mutex
    
    ST5 --> Counter
    NT5 --> Counter
    HT5 --> Counter
    
    SensorTask -.-> Sched
    NetworkTask -.-> Sched
    HighPrioTask -.-> Sched
    
    style SensorTask fill:#E8F5E9
    style NetworkTask fill:#E3F2FD
    style HighPrioTask fill:#FFF3E0
    style Mutex fill:#FFE4E1
    style Counter fill:#FFE4E1
```

## Cooperative Mode - Detailed Event Flow

```mermaid
flowchart TD
    subgraph TimerCallback[Timer Callback - Every 250ms]
        TC1([Timer Fires]) --> TC2{Phase Counter}
        TC2 -->|Phase 0| TC3[Post EVT_SENSOR]
        TC2 -->|Phase 1| TC4[Post EVT_NET]
        TC2 -->|Phase 2| TC5[Post EVT_UI]
        TC3 --> TC6[Increment Phase]
        TC4 --> TC6
        TC5 --> TC6
        TC6 --> TC7{Queue Full?}
        TC7 -->|Yes| TC8[Log: Queue Full<br/>Drop Event]
        TC7 -->|No| TC9[Event Added to Queue]
        TC8 --> TC10([Return])
        TC9 --> TC10
    end
    
    subgraph MainLoop[Main Event Loop Task]
        ML1([Task Start]) --> ML2[Wait for Event<br/>from Queue]
        ML2 --> ML3{Event<br/>Received?}
        ML3 -->|Yes| ML4{Event Type?}
        ML4 -->|EVT_SENSOR| ML5[Call handle_sensor_event]
        ML4 -->|EVT_NET| ML6[Call handle_net_event]
        ML4 -->|EVT_UI| ML7[Call handle_ui_event]
        ML4 -->|Unknown| ML8[Log Warning]
        ML5 --> ML2
        ML6 --> ML2
        ML7 --> ML2
        ML8 --> ML2
    end
    
    subgraph SensorHandler[Sensor Event Handler]
        SH1([Handler Called]) --> SH2[CPU Work<br/>180k iterations]
        SH2 --> SH3[Increment Counter +1]
        SH3 --> SH4[Log Event Info]
        SH4 --> SH5([Return])
    end
    
    subgraph NetHandler[Network Event Handler]
        NH1([Handler Called]) --> NH2[CPU Work<br/>260k iterations]
        NH2 --> NH3[Increment Counter +2]
        NH3 --> NH4[Log Event Info]
        NH4 --> NH5([Return])
    end
    
    subgraph UIHandler[UI Event Handler]
        UH1([Handler Called]) --> UH2[CPU Work<br/>120k iterations]
        UH2 --> UH3[Increment Counter +3]
        UH3 --> UH4[Log Event Info]
        UH4 --> UH5([Return])
    end
    
    subgraph Queue[Event Queue FIFO]
        Q1[Event 1]
        Q2[Event 2]
        Q3[Event 3]
        QN[...]
        Q1 --> Q2 --> Q3 --> QN
    end
    
    subgraph SimpleCounter[Simple Counter]
        SC[(Counter<br/>No Mutex Needed)]
    end
    
    TC9 --> Queue
    ML2 --> Queue
    
    ML5 --> SensorHandler
    ML6 --> NetHandler
    ML7 --> UIHandler
    
    SH3 --> SimpleCounter
    NH3 --> SimpleCounter
    UH3 --> SimpleCounter
    
    style TimerCallback fill:#FFF9C4
    style MainLoop fill:#C8E6C9
    style SensorHandler fill:#FFECB3
    style NetHandler fill:#B3E5FC
    style UIHandler fill:#F8BBD0
    style Queue fill:#E1BEE7
    style SimpleCounter fill:#C5CAE9
```

## Preemptive vs Cooperative Comparison Flow

```mermaid
flowchart LR
    subgraph Preemptive[Preemptive Scheduling]
        direction TB
        P1[Multiple Tasks<br/>Running Concurrently]
        P2[FreeRTOS Scheduler<br/>Manages Switching]
        P3[Higher Priority<br/>Can Preempt]
        P4[Mutex Required<br/>for Shared Data]
        P5[Complex but<br/>Responsive]
        
        P1 --> P2 --> P3 --> P4 --> P5
    end
    
    subgraph Cooperative[Cooperative Scheduling]
        direction TB
        C1[Single Task<br/>Event Loop]
        C2[Events Processed<br/>Sequentially]
        C3[Run to<br/>Completion]
        C4[No Mutex<br/>Needed]
        C5[Simple but<br/>Can Block]
        
        C1 --> C2 --> C3 --> C4 --> C5
    end
    
    Start([Task Execution Model]) --> Preemptive
    Start --> Cooperative
    
    Preemptive -.comparison.-> Feature1{Real-time<br/>Requirements?}
    Cooperative -.comparison.-> Feature1
    
    Feature1 -->|High| UseP[Use Preemptive]
    Feature1 -->|Low| UseC[Use Cooperative]
    
    Preemptive -.comparison.-> Feature2{System<br/>Complexity?}
    Cooperative -.comparison.-> Feature2
    
    Feature2 -->|Can Handle| UseP
    Feature2 -->|Keep Simple| UseC
    
    Preemptive -.comparison.-> Feature3{Concurrent<br/>Operations?}
    Cooperative -.comparison.-> Feature3
    
    Feature3 -->|Many| UseP
    Feature3 -->|Few| UseC
    
    style Preemptive fill:#FFE4E1
    style Cooperative fill:#E0F2F7
    style Start fill:#D4EDDA
    style UseP fill:#FFB6C1
    style UseC fill:#ADD8E6
```

## State Transition Diagram - Preemptive Mode

```mermaid
stateDiagram-v2
    [*] --> Ready: Task Created
    Ready --> Running: Scheduler Selects
    Running --> Ready: Preempted by Higher Priority
    Running --> Blocked: Wait for Mutex/Delay
    Blocked --> Ready: Mutex Available/Delay Expired
    Running --> [*]: Task Deleted (Never in this demo)
    
    note right of Running
        Task executes:
        - CPU work
        - Mutex operations
        - Counter increment
        - Logging
    end note
    
    note right of Blocked
        Task waiting for:
        - vTaskDelay() timer
        - Mutex acquisition
    end note
```

## State Transition Diagram - Cooperative Mode

```mermaid
stateDiagram-v2
    [*] --> WaitingForEvent: Loop Task Started
    WaitingForEvent --> ProcessingEvent: Event Received from Queue
    ProcessingEvent --> HandlerExecution: Dispatch to Handler
    HandlerExecution --> WaitingForEvent: Handler Complete
    
    note right of WaitingForEvent
        Task blocked on:
        xQueueReceive()
        No CPU usage
    end note
    
    note right of HandlerExecution
        Handler runs to completion:
        - CPU work
        - Counter increment
        - Logging
        NO interruption possible
    end note
```

## Timing Diagram - Preemptive Mode

```mermaid
gantt
    title Preemptive Task Execution Timeline (First 2 seconds)
    dateFormat  X
    axisFormat %L ms
    
    section High Priority (P=8)
    Idle           :0, 1500
    HIGH Burst     :1500, 1800
    Idle           :1800, 3000
    
    section Sensor (P=5)
    Work+Counter   :0, 100
    Delay          :100, 500
    Work+Counter   :500, 600
    Delay          :600, 1000
    Work+Counter   :1000, 1100
    Preempted      :1500, 1800
    Resume         :1800, 1900
    
    section Network (P=4)
    Work+Counter   :0, 150
    Delay          :150, 800
    Work+Counter   :800, 950
    Preempted      :1500, 1800
    Resume         :1800, 2000
```

## Timing Diagram - Cooperative Mode

```mermaid
gantt
    title Cooperative Event Processing Timeline (First 1 second)
    dateFormat  X
    axisFormat %L ms
    
    section Timer Posts
    EVT_SENSOR     :milestone, 0, 0
    EVT_NET        :milestone, 250, 250
    EVT_UI         :milestone, 500, 500
    EVT_SENSOR     :milestone, 750, 750
    EVT_NET        :milestone, 1000, 1000
    
    section Event Loop
    Process SENSOR :0, 80
    Idle           :80, 250
    Process NET    :250, 400
    Idle           :400, 500
    Process UI     :500, 600
    Idle           :600, 750
    Process SENSOR :750, 830
    Idle           :830, 1000
    Process NET    :1000, 1150
```

---

**Note**: These diagrams can be rendered in any Mermaid-compatible viewer, including:
- GitHub (directly in README.md)
- GitLab
- [Mermaid Live Editor](https://mermaid.live)
- VS Code with Mermaid extension
- Documentation sites (MkDocs, Docusaurus, etc.)
