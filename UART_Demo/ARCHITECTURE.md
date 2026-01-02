# UART Reference Architecture - System Flow Diagram

This document contains the detailed system architecture flowchart for the ESP32 UART Reference project.

## Complete System Flow

```mermaid
graph TB
    subgraph "UART Hardware"
        UART[UART Peripheral]
    end
    
    subgraph "ESP-IDF Driver Layer"
        DRV[UART Driver]
        EVTQ[Event Queue]
        RXBUF[Driver RX Buffer<br/>4096 bytes]
        TXBUF[Driver TX Buffer<br/>2048 bytes]
    end
    
    subgraph "Application Layer"
        RXTASK[RX Event Task<br/>Priority: 12]
        STREAM[StreamBuffer<br/>4096 bytes]
        PARSER[Parser Task<br/>Priority: 10]
        LINEACC[Line Accumulator<br/>256 bytes]
        HANDLER[Command Handler]
        TXQ[TX Queue<br/>10 messages]
        TXTASK[TX Task<br/>Priority: 10]
    end
    
    %% RX Path
    UART -->|IRQ| DRV
    DRV -->|Writes| RXBUF
    DRV -->|Events| EVTQ
    EVTQ -->|Blocks on| RXTASK
    RXTASK -->|uart_read_bytes| RXBUF
    RXTASK -->|xStreamBufferSend| STREAM
    STREAM -->|xStreamBufferReceive| PARSER
    PARSER -->|Accumulates| LINEACC
    LINEACC -->|Complete Line| HANDLER
    
    %% TX Path
    HANDLER -->|tx_send_str| TXQ
    TXQ -->|xQueueReceive| TXTASK
    TXTASK -->|uart_write_bytes| TXBUF
    TXBUF -->|DMA/IRQ| DRV
    DRV -->|Transmits| UART
    
    %% Styling
    classDef hardware fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    classDef driver fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    classDef app fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px
    classDef buffer fill:#f3e5f5,stroke:#6a1b9a,stroke-width:2px
    
    class UART hardware
    class DRV,EVTQ,RXBUF,TXBUF driver
    class RXTASK,PARSER,TXTASK,HANDLER app
    class STREAM,LINEACC,TXQ buffer
```

## Task State Diagram

```mermaid
stateDiagram-v2
    [*] --> Init: app_main()
    
    Init --> RXReady: Create RX Task
    Init --> ParserReady: Create Parser Task
    Init --> TXReady: Create TX Task
    
    state RXReady {
        [*] --> WaitEvent: xQueueReceive
        WaitEvent --> ReadUART: UART_DATA event
        ReadUART --> PushStream: uart_read_bytes()
        PushStream --> WaitEvent: xStreamBufferSend()
        
        WaitEvent --> HandleOverflow: OVERFLOW event
        HandleOverflow --> WaitEvent: Flush & Reset
        
        WaitEvent --> HandleError: ERROR event
        HandleError --> WaitEvent: Log error
    }
    
    state ParserReady {
        [*] --> WaitData: xStreamBufferReceive
        WaitData --> Accumulate: Bytes available
        Accumulate --> WaitData: No newline yet
        Accumulate --> Dispatch: Newline found
        Dispatch --> WaitData: handle_line()
    }
    
    state TXReady {
        [*] --> WaitMessage: xQueueReceive
        WaitMessage --> Transmit: Message available
        Transmit --> WaitDone: uart_write_bytes()
        WaitDone --> WaitMessage: uart_wait_tx_done()
    }
```

## Data Structure Relationships

```mermaid
classDiagram
    class uart_event_t {
        <<ESP-IDF>>
        +type: uart_event_type_t
        +size: size_t
    }
    
    class uart_tx_msg_t {
        +len: size_t
        +data[256]: uint8_t
    }
    
    class line_acc_t {
        +line[256]: char
        +len: size_t
        +line_acc_reset()
        +line_acc_push(data, n): int
    }
    
    class QueueHandle_t {
        <<FreeRTOS>>
    }
    
    class StreamBufferHandle_t {
        <<FreeRTOS>>
    }
    
    uart_event_t --> QueueHandle_t: uart_evt_queue
    uart_tx_msg_t --> QueueHandle_t: tx_queue
    line_acc_t --> StreamBufferHandle_t: Consumes from rx_stream
```

## Message Flow Sequence

```mermaid
sequenceDiagram
    participant External as External Device
    participant UART as UART Hardware
    participant Driver as UART Driver
    participant RXTask as RX Event Task
    participant Stream as StreamBuffer
    participant Parser as Parser Task
    participant Handler as Command Handler
    participant TXQueue as TX Queue
    participant TXTask as TX Task
    
    %% Reception Flow
    External->>UART: Sends "PING\n"
    UART->>Driver: IRQ triggered
    Driver->>Driver: Store in RX buffer
    Driver->>RXTask: Post UART_DATA event
    activate RXTask
    RXTask->>Driver: uart_read_bytes()
    Driver-->>RXTask: Returns bytes
    RXTask->>Stream: xStreamBufferSend()
    deactivate RXTask
    
    Stream->>Parser: xStreamBufferReceive()
    activate Parser
    Parser->>Parser: line_acc_push()
    Parser->>Parser: Detect '\n'
    Parser->>Handler: handle_line("PING")
    deactivate Parser
    
    %% Transmission Flow
    activate Handler
    Handler->>Handler: strcmp("PING")
    Handler->>TXQueue: tx_send_str("PONG\n")
    deactivate Handler
    
    TXQueue->>TXTask: xQueueReceive()
    activate TXTask
    TXTask->>Driver: uart_write_bytes()
    Driver->>UART: DMA transfer
    UART->>External: Sends "PONG\n"
    TXTask->>Driver: uart_wait_tx_done()
    deactivate TXTask
```

## Error Handling Flow

```mermaid
flowchart TD
    Start([UART Event]) --> CheckType{Event Type?}
    
    CheckType -->|UART_DATA| ReadData[Read bytes from<br/>UART buffer]
    ReadData --> PushStream[Push to StreamBuffer]
    PushStream --> CheckFull{Stream Full?}
    CheckFull -->|No| Success([Continue])
    CheckFull -->|Yes| WarnDrop[Log warning:<br/>Bytes dropped]
    WarnDrop --> Success
    
    CheckType -->|UART_FIFO_OVF| FlushInput[Flush UART input]
    CheckType -->|UART_BUFFER_FULL| FlushInput
    FlushInput --> ResetQueue[Reset event queue]
    ResetQueue --> LogOvf[Log overflow warning]
    LogOvf --> Success
    
    CheckType -->|UART_FRAME_ERR| LogFrame[Log frame error]
    LogFrame --> Success
    
    CheckType -->|UART_PARITY_ERR| LogParity[Log parity error]
    LogParity --> Success
    
    CheckType -->|Other| Ignore[Ignore event]
    Ignore --> Success
```

## Buffer Size Optimization

```mermaid
graph LR
    subgraph "Small Buffers (Default)"
        A1[RX: 4KB<br/>TX: 2KB<br/>Stream: 4KB]
        B1[Low memory<br/>footprint]
        C1[Risk of overflow<br/>under load]
    end
    
    subgraph "Medium Buffers"
        A2[RX: 8KB<br/>TX: 4KB<br/>Stream: 8KB]
        B2[Balanced<br/>performance]
        C2[Handles moderate<br/>bursts]
    end
    
    subgraph "Large Buffers"
        A3[RX: 16KB<br/>TX: 8KB<br/>Stream: 16KB]
        B3[Maximum<br/>reliability]
        C3[Higher memory<br/>usage]
    end
    
    A1 --> B1 --> C1
    A2 --> B2 --> C2
    A3 --> B3 --> C3
```

## Notes

### Color Coding
- **Blue**: Hardware layer (UART peripheral)
- **Yellow**: Driver layer (ESP-IDF UART driver)
- **Green**: Application layer (FreeRTOS tasks)
- **Purple**: Buffer/queue abstractions

### Critical Paths
1. **RX Hot Path**: UART IRQ → Driver → RX Task → StreamBuffer (minimize latency)
2. **TX Serialization**: All writes through TX Task (prevent interleaving)
3. **Parser Decoupling**: StreamBuffer isolates I/O from protocol logic

### Performance Considerations
- RX Task priority (12) > Parser/TX (10) ensures interrupt servicing
- StreamBuffer acts as shock absorber for burst traffic
- TX queue depth (10) prevents blocking on output
- Line accumulator limits command length (256 bytes) for memory safety
