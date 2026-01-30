# ESP32 UART Event Reference - System Architecture

## Main Application Flow

```mermaid
flowchart TB
    Start([app_main]) --> Init[Initialize UART<br/>Event Mode]
    Init --> EventGroup[Create Event Group]
    EventGroup --> Banner[Print Banner]
    Banner --> CreateTasks[Create FreeRTOS Tasks]
    
    CreateTasks --> RXTask[UART RX Event Task]
    CreateTasks --> TXTask[TX Heartbeat Task]
    
    RXTask --> WaitEvent[Wait for UART Event<br/>xQueueReceive]
    WaitEvent --> CheckEvent{Event Type?}
    
    CheckEvent -->|UART_DATA| ReadData[Read Bytes from UART]
    ReadData --> Accumulate[Accumulate into Line Buffer]
    Accumulate --> CheckLine{Complete Line?}
    CheckLine -->|Yes CR/LF| ParseLine[Parse and Handle Command]
    CheckLine -->|No| WaitEvent
    ParseLine --> Echo[Echo Line]
    Echo --> CheckCmd{Command?}
    CheckCmd -->|help| SendHelp[Send Help Message]
    CheckCmd -->|status| SendStatus[Send Error Statistics]
    CheckCmd -->|unknown| SendUnknown[Send Unknown Message]
    SendHelp --> WaitEvent
    SendStatus --> WaitEvent
    SendUnknown --> WaitEvent
    
    CheckEvent -->|UART_FIFO_OVF| HandleFIFO[Increment FIFO Counter<br/>Flush Input<br/>Reset Queue]
    HandleFIFO --> WaitEvent
    
    CheckEvent -->|UART_BUFFER_FULL| HandleBuffer[Increment Buffer Counter<br/>Flush Input<br/>Reset Queue]
    HandleBuffer --> WaitEvent
    
    CheckEvent -->|UART_FRAME_ERR| HandleFrame[Increment Frame Counter<br/>Flush Input]
    HandleFrame --> WaitEvent
    
    CheckEvent -->|UART_PARITY_ERR| HandleParity[Increment Parity Counter<br/>Flush Input]
    HandleParity --> WaitEvent
    
    TXTask --> WaitReady[Wait for UART Ready Event]
    WaitReady --> TXLoop[Loop Every 3 Seconds]
    TXLoop --> SendHeartbeat[Send Heartbeat Message<br/>with Counter]
    SendHeartbeat --> Delay[Delay 3000ms]
    Delay --> TXLoop
    
    style Start fill:#e1f5e1
    style RXTask fill:#e3f2fd
    style TXTask fill:#fff3e0
    style CheckEvent fill:#f3e5f5
    style CheckCmd fill:#f3e5f5
    style HandleFIFO fill:#ffebee
    style HandleBuffer fill:#ffebee
    style HandleFrame fill:#ffebee
    style HandleParity fill:#ffebee
```

## UART Event Processing Sequence

```mermaid
sequenceDiagram
    participant ISR as UART ISR
    participant Queue as Event Queue
    participant Task as RX Event Task
    participant Handler as Line Handler
    participant UART as UART Driver
    
    ISR->>Queue: Post UART_DATA Event
    Queue->>Task: xQueueReceive (blocking)
    Task->>UART: uart_read_bytes()
    UART-->>Task: Return received bytes
    Task->>Task: Accumulate bytes in line buffer
    
    alt Complete Line (CR/LF detected)
        Task->>Handler: handle_line(line)
        Handler->>Handler: Parse command
        alt help command
            Handler->>UART: uart_write_str("commands: help, status")
        else status command
            Handler->>UART: uart_write_str(error statistics)
        else unknown command
            Handler->>UART: uart_write_str("unknown cmd")
        end
    else Buffer overflow
        Task->>UART: Reset line buffer
        Task->>UART: uart_write_str("warn: line too long")
    end
    
    Task->>Queue: Wait for next event
    
    Note over ISR,Queue: Error Event Example
    ISR->>Queue: Post UART_FIFO_OVF Event
    Queue->>Task: xQueueReceive
    Task->>Task: Increment error counter
    Task->>UART: uart_flush_input()
    Task->>Queue: xQueueReset()
    Task->>Queue: Wait for next event
```

## Component Architecture

```mermaid
graph TB
    subgraph "Application Layer"
        App[app_main]
        LineHandler[Line Handler]
        CmdParser[Command Parser]
    end
    
    subgraph "FreeRTOS Layer"
        RXTask[RX Event Task]
        TXTask[TX Heartbeat Task]
        EventQueue[Event Queue]
        EventGroup[Event Group]
    end
    
    subgraph "Driver Layer"
        UARTDriver[UART Driver]
        ISR[UART ISR]
    end
    
    subgraph "Hardware Layer"
        UARTHW[UART Peripheral]
        GPIO[GPIO Pins]
    end
    
    App --> RXTask
    App --> TXTask
    App --> EventGroup
    RXTask --> EventQueue
    RXTask --> LineHandler
    LineHandler --> CmdParser
    CmdParser --> UARTDriver
    TXTask --> UARTDriver
    EventQueue --> ISR
    ISR --> UARTHW
    UARTDriver --> UARTHW
    UARTHW --> GPIO
    
    style App fill:#e1f5e1
    style RXTask fill:#e3f2fd
    style TXTask fill:#fff3e0
    style UARTDriver fill:#f3e5f5
    style UARTHW fill:#ffebee
```

## Data Flow Diagram

```mermaid
flowchart LR
    Input[External UART<br/>Device] -->|Serial Data| RX[RX Pin<br/>GPIO16]
    RX -->|Hardware| FIFO[UART FIFO<br/>Buffer]
    FIFO -->|DMA/ISR| RingBuf[Ring Buffer<br/>2048 bytes]
    RingBuf -->|Event| Queue[Event Queue<br/>20 events]
    Queue -->|FreeRTOS| Task[RX Event Task]
    Task -->|Process| LineBuf[Line Buffer<br/>256 bytes]
    LineBuf -->|Parse| Handler[Command Handler]
    Handler -->|Response| TXBuf[TX Ring Buffer<br/>2048 bytes]
    TXBuf -->|Hardware| TX[TX Pin<br/>GPIO17]
    TX -->|Serial Data| Output[External UART<br/>Device]
    
    Heartbeat[Heartbeat Task] -.->|Periodic| TXBuf
    
    style Input fill:#e1f5e1
    style Output fill:#e1f5e1
    style Task fill:#e3f2fd
    style Heartbeat fill:#fff3e0
    style Handler fill:#f3e5f5
```

## State Machine - Line Accumulator

```mermaid
stateDiagram-v2
    [*] --> Idle: Initialize
    Idle --> Accumulating: Receive Byte
    Accumulating --> Accumulating: Byte != CR/LF
    Accumulating --> ProcessLine: Byte == CR/LF
    Accumulating --> BufferFull: Buffer Overflow
    BufferFull --> Idle: Reset Buffer
    ProcessLine --> Idle: Line Processed
    
    note right of Accumulating
        Store byte in buffer
        Increment length
    end note
    
    note right of ProcessLine
        Null-terminate
        Call handle_line()
        Reset length
    end note
    
    note right of BufferFull
        Send warning
        Discard line
    end note
```

## Error Recovery Flow

```mermaid
flowchart TB
    Error{Error Type?}
    
    Error -->|FIFO Overflow| FIFO[Increment s_fifo_ovf_count]
    Error -->|Buffer Full| BufFull[Increment s_buf_full_count]
    Error -->|Frame Error| Frame[Increment s_frame_err_count]
    Error -->|Parity Error| Parity[Increment s_parity_err_count]
    
    FIFO --> Log1[Log Warning]
    BufFull --> Log2[Log Warning]
    Frame --> Log3[Log Warning]
    Parity --> Log4[Log Warning]
    
    Log1 --> Flush1[uart_flush_input]
    Log2 --> Flush2[uart_flush_input]
    Log3 --> Flush3[uart_flush_input]
    Log4 --> Flush4[uart_flush_input]
    
    Flush1 --> Reset1[xQueueReset]
    Flush2 --> Reset2[xQueueReset]
    Flush3 --> Continue1[Continue Processing]
    Flush4 --> Continue2[Continue Processing]
    
    Reset1 --> Continue3[Continue Processing]
    Reset2 --> Continue4[Continue Processing]
    
    Continue1 --> Normal[Normal Operation]
    Continue2 --> Normal
    Continue3 --> Normal
    Continue4 --> Normal
    
    style FIFO fill:#ffebee
    style BufFull fill:#ffebee
    style Frame fill:#ffebee
    style Parity fill:#ffebee
    style Normal fill:#e1f5e1
```
