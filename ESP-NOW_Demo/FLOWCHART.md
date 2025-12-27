# ESP-NOW Project Flowcharts

This document contains Mermaid flowcharts that visualize the ESP-NOW communication flow and architecture.

## Overall System Architecture

```mermaid
graph TB
    subgraph "Sender Device ESP32-S3"
        S1[app_main]
        S2[init_nvs]
        S3[init_wifi_for_espnow]
        S4[init_espnow]
        S5[espnow_config_peer]
        S6[Create Queue & Event Group]
        S7[sender_task]
        S8[Build Packet]
        S9[esp_now_send]
        S10[espnow_send_cb]
        S11[Wait for Event]
        
        S1 --> S2
        S2 --> S3
        S3 --> S4
        S4 --> S5
        S5 --> S6
        S6 --> S7
        S7 --> S8
        S8 --> S9
        S9 --> S10
        S10 --> S11
        S11 --> S7
    end
    
    subgraph "Receiver Device ESP32-S3"
        R1[app_main]
        R2[init_nvs]
        R3[init_wifi_for_espnow]
        R4[init_espnow]
        R5[espnow_config_peer]
        R6[Create Queue & Event Group]
        R7[receiver_task]
        R8[espnow_recv_cb]
        R9[Queue Packet]
        R10[Process Packet]
        R11[Log Output]
        
        R1 --> R2
        R2 --> R3
        R3 --> R4
        R4 --> R5
        R5 --> R6
        R6 --> R7
        R8 --> R9
        R9 --> R7
        R7 --> R10
        R10 --> R11
        R11 --> R7
    end
    
    S9 -.->|ESP-NOW Packet| R8
    
    style S7 fill:#90EE90
    style R7 fill:#87CEEB
    style S9 fill:#FFD700
    style R8 fill:#FFD700
```

## Sender Device Detailed Flow

```mermaid
flowchart TD
    Start([Start]) --> InitNVS[Initialize NVS]
    InitNVS --> InitWiFi[Initialize Wi-Fi in STA Mode]
    InitWiFi --> SetChannel[Set Wi-Fi Channel]
    SetChannel --> InitESPNOW[Initialize ESP-NOW]
    InitESPNOW --> RegisterCallbacks[Register Send/Recv Callbacks]
    RegisterCallbacks --> ParseMAC{Parse Peer MAC Address}
    
    ParseMAC -->|Invalid| Error1[Log Error & Exit]
    ParseMAC -->|Valid| CheckBroadcast{Broadcast MAC?}
    
    CheckBroadcast -->|Yes FF:FF:FF:FF:FF:FF| SkipPeer[Skip Peer Add]
    CheckBroadcast -->|No| AddPeer[Add Peer to ESP-NOW]
    
    SkipPeer --> CreateQueue[Create RX Queue]
    AddPeer --> CreateQueue
    CreateQueue --> CreateEventGroup[Create Event Group]
    CreateEventGroup --> StartTask[Start sender_task]
    
    StartTask --> SenderLoop{Sender Task Loop}
    SenderLoop --> BuildPacket[Build Packet<br/>version=1, type=1<br/>seq++, counter++]
    BuildPacket --> SendPacket[esp_now_send]
    SendPacket --> WaitEvent[Wait for SEND_DONE_BIT<br/>200ms timeout]
    
    WaitEvent --> Delay[Delay 1 second]
    Delay --> SenderLoop
    
    SendPacket -.->|Async| SendCallback[espnow_send_cb]
    SendCallback --> CheckStatus{Send Status}
    CheckStatus -->|SUCCESS| LogSuccess[Log: Send SUCCESS]
    CheckStatus -->|FAIL| LogFail[Log: Send FAIL]
    LogSuccess --> SetBit[Set SEND_DONE_BIT]
    LogFail --> SetBit
    SetBit -.->|Signal| WaitEvent
    
    style Start fill:#90EE90
    style SenderLoop fill:#FFD700
    style SendPacket fill:#FFA500
    style SendCallback fill:#FF6347
```

## Receiver Device Detailed Flow

```mermaid
flowchart TD
    Start([Start]) --> InitNVS[Initialize NVS]
    InitNVS --> InitWiFi[Initialize Wi-Fi in STA Mode]
    InitWiFi --> SetChannel[Set Wi-Fi Channel]
    SetChannel --> InitESPNOW[Initialize ESP-NOW]
    InitESPNOW --> RegisterCallbacks[Register Send/Recv Callbacks]
    RegisterCallbacks --> ParseMAC{Parse Peer MAC Address}
    
    ParseMAC -->|Invalid| Error1[Log Error & Exit]
    ParseMAC -->|Valid| CreateQueue[Create RX Queue<br/>Size: 16 items]
    
    CreateQueue --> CreateEventGroup[Create Event Group]
    CreateEventGroup --> StartTask[Start receiver_task]
    
    StartTask --> ReceiverLoop{Receiver Task Loop}
    ReceiverLoop --> WaitQueue[Wait on Queue<br/>portMAX_DELAY]
    
    WaitQueue -->|Item Received| ExtractData[Extract Packet Data]
    ExtractData --> FormatMAC[Format Source MAC]
    FormatMAC --> LogPacket[Log Packet Details<br/>MAC, ver, type, seq, counter]
    LogPacket --> ReceiverLoop
    
    IncomingPacket([ESP-NOW Packet Arrives]) -.->|Wi-Fi ISR| RecvCallback[espnow_recv_cb]
    RecvCallback --> ValidatePacket{Valid Packet?}
    ValidatePacket -->|No| DiscardPacket[Discard]
    ValidatePacket -->|Yes| CopyData[Copy to rx_item_t]
    CopyData --> QueueSend[xQueueSend<br/>Don't Block]
    QueueSend -.->|Queued| WaitQueue
    
    style Start fill:#87CEEB
    style ReceiverLoop fill:#FFD700
    style RecvCallback fill:#FF6347
    style LogPacket fill:#90EE90
```

## Packet Structure

```mermaid
classDiagram
    class app_packet_t {
        +uint8_t version
        +uint8_t msg_type
        +uint16_t seq
        +uint32_t counter
    }
    
    class rx_item_t {
        +uint8_t src_mac[6]
        +int len
        +app_packet_t pkt
    }
    
    rx_item_t --> app_packet_t : contains
    
    note for app_packet_t "Total Size: 8 bytes\nversion: Protocol version (1)\nmsg_type: Message type (1)\nseq: Sequence number\ncounter: Application counter"
    
    note for rx_item_t "Used internally by receiver\nto pass data from callback\nto receiver task via queue"
```

## Initialization Sequence

```mermaid
sequenceDiagram
    participant App as app_main()
    participant NVS as NVS Flash
    participant WiFi as Wi-Fi Driver
    participant ESPNOW as ESP-NOW
    participant Task as sender_task/receiver_task
    
    App->>NVS: nvs_flash_init()
    NVS-->>App: ESP_OK
    
    App->>WiFi: esp_netif_init()
    App->>WiFi: esp_event_loop_create_default()
    App->>WiFi: esp_wifi_init()
    App->>WiFi: esp_wifi_set_mode(WIFI_MODE_STA)
    App->>WiFi: esp_wifi_start()
    App->>WiFi: esp_wifi_set_channel(channel)
    WiFi-->>App: Wi-Fi Ready
    
    App->>ESPNOW: esp_now_init()
    ESPNOW-->>App: ESP_OK
    
    App->>ESPNOW: esp_now_register_send_cb()
    App->>ESPNOW: esp_now_register_recv_cb()
    
    alt Sender Mode
        App->>ESPNOW: esp_now_add_peer()
        ESPNOW-->>App: Peer Added
        App->>Task: xTaskCreate(sender_task)
    else Receiver Mode
        App->>Task: xTaskCreate(receiver_task)
    end
    
    Task-->>App: Task Running
```

## Send Operation Flow

```mermaid
sequenceDiagram
    participant ST as sender_task
    participant ESP as ESP-NOW API
    participant WiFi as Wi-Fi Driver
    participant CB as espnow_send_cb
    participant EG as Event Group
    
    ST->>ST: Build app_packet_t
    ST->>ESP: esp_now_send(peer_mac, data, len)
    ESP->>WiFi: Queue TX
    WiFi-->>ESP: Queued
    ESP-->>ST: ESP_OK
    
    ST->>EG: xEventGroupWaitBits(SEND_DONE_BIT, 200ms)
    
    Note over WiFi: Packet Transmitted
    
    WiFi->>CB: Callback with status
    CB->>CB: Log status
    CB->>EG: xEventGroupSetBits(SEND_DONE_BIT)
    EG-->>ST: Bit Set
    
    ST->>ST: vTaskDelay(1000ms)
    ST->>ST: Loop
```

## Receive Operation Flow

```mermaid
sequenceDiagram
    participant WiFi as Wi-Fi Driver
    participant CB as espnow_recv_cb
    participant Q as RX Queue
    participant RT as receiver_task
    participant Log as Serial Output
    
    Note over WiFi: ESP-NOW Packet Arrives
    
    WiFi->>CB: Callback(src_addr, data, len)
    CB->>CB: Validate packet
    CB->>CB: Copy to rx_item_t
    CB->>Q: xQueueSend(item, 0)
    Q-->>CB: Queued
    CB-->>WiFi: Return
    
    RT->>Q: xQueueReceive(portMAX_DELAY)
    Q-->>RT: rx_item_t
    
    RT->>RT: Extract packet data
    RT->>RT: Format MAC address
    RT->>Log: ESP_LOGI(packet details)
    Log-->>RT: Logged
    
    RT->>Q: Wait for next packet
```

## State Machine Overview

```mermaid
stateDiagram-v2
    [*] --> Initialization
    
    Initialization --> ConfigCheck: NVS & WiFi Ready
    ConfigCheck --> SenderMode: CONFIG_ESPNOW_ROLE_SENDER
    ConfigCheck --> ReceiverMode: !CONFIG_ESPNOW_ROLE_SENDER
    
    state SenderMode {
        [*] --> Idle
        Idle --> BuildingPacket: Timer Expired
        BuildingPacket --> Sending: Packet Ready
        Sending --> WaitingConfirm: Packet Sent
        WaitingConfirm --> Idle: Callback Received
        WaitingConfirm --> Idle: Timeout
    }
    
    state ReceiverMode {
        [*] --> Listening
        Listening --> Processing: Packet Received
        Processing --> Listening: Logged
    }
    
    SenderMode --> [*]: Error
    ReceiverMode --> [*]: Error
```

## Memory and Resource Flow

```mermaid
graph LR
    subgraph "Sender Resources"
        A[Stack: 4096 bytes]
        B[Event Group]
        C[Static Variables:<br/>s_peer_mac,<br/>s_seq]
    end
    
    subgraph "Receiver Resources"
        D[Stack: 4096 bytes]
        E[RX Queue: 16 items]
        F[Each item:<br/>rx_item_t 20 bytes]
    end
    
    subgraph "Shared Resources"
        G[Wi-Fi Driver]
        H[ESP-NOW Buffer]
        I[NVS Flash]
    end
    
    A --> G
    B --> G
    C --> G
    D --> G
    E --> H
    F --> H
    G --> I
```

## Channel Configuration Impact

```mermaid
graph TD
    A[ESP-NOW Communication] --> B{Same Channel?}
    B -->|Yes| C[Communication OK]
    B -->|No| D[Communication FAIL]
    
    C --> E[Packets Delivered]
    D --> F[Send Status: FAIL]
    D --> G[No Packets Received]
    
    E --> H{Broadcast or Unicast?}
    H -->|Broadcast FF:FF:FF:FF:FF:FF| I[All devices on channel receive]
    H -->|Unicast| J[Only specific peer receives]
    
    I --> K[No encryption]
    J --> L{Encryption enabled?}
    L -->|Yes| M[Encrypted with LMK key]
    L -->|No| N[Plain text]
    
    style C fill:#90EE90
    style D fill:#FF6347
    style E fill:#90EE90
    style F fill:#FF6347
    style G fill:#FF6347
```

## Error Handling Flow

```mermaid
flowchart TD
    Start([Function Call]) --> Check1{NVS Init OK?}
    Check1 -->|No| Erase[Erase NVS]
    Erase --> Retry1[Retry Init]
    Retry1 --> Check1
    Check1 -->|Yes| Check2{WiFi Init OK?}
    
    Check2 -->|No| Error2[Log Error & Return]
    Check2 -->|Yes| Check3{ESP-NOW Init OK?}
    
    Check3 -->|No| Error3[Log Error & Return]
    Check3 -->|Yes| Check4{MAC Valid?}
    
    Check4 -->|No| Error4[Log Invalid Format & Return]
    Check4 -->|Yes| Check5{Queue Created?}
    
    Check5 -->|No| Error5[Log Error & Return]
    Check5 -->|Yes| Check6{Event Group Created?}
    
    Check6 -->|No| Error6[Log Error & Return]
    Check6 -->|Yes| Success[Continue Normal Operation]
    
    style Success fill:#90EE90
    style Error2 fill:#FF6347
    style Error3 fill:#FF6347
    style Error4 fill:#FF6347
    style Error5 fill:#FF6347
    style Error6 fill:#FF6347
```

---

## Notes on Flowcharts

### Key Components

1. **NVS (Non-Volatile Storage)**: Required by Wi-Fi driver for storing calibration data
2. **Wi-Fi STA Mode**: Station mode without connecting to an access point
3. **ESP-NOW Callbacks**: Execute in Wi-Fi task context (high priority)
4. **FreeRTOS Queue**: Thread-safe communication between callback and task
5. **Event Group**: Synchronization mechanism for send confirmation

### Design Decisions

- **Callback Minimal Work**: Callbacks do minimal processing to avoid blocking Wi-Fi driver
- **Queue-Based Handoff**: Received data is queued for processing in normal task context
- **Event-Based Synchronization**: Sender waits for confirmation using event bits
- **One-Second Interval**: Balances demonstration with manageable log output

### Performance Considerations

- Queue size of 16 can handle burst traffic
- 200ms send timeout prevents indefinite blocking
- 1-second send interval is conservative; can be reduced to 10-100ms
- Task stack of 4096 bytes is sufficient for current operations

---

For more details, see the [README.md](README.md) file.
