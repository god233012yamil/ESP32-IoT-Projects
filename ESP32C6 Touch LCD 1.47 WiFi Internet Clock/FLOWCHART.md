# ESP32-C6 WiFi Clock - System Flowchart

This document contains comprehensive flowcharts illustrating the system architecture and execution flow of the ESP32-C6 WiFi Internet Clock application.

## Table of Contents
- [Main Application Flow](#main-application-flow)
- [WiFi Connection State Machine](#wifi-connection-state-machine)
- [Time Synchronization Flow](#time-synchronization-flow)
- [Display Update Task](#display-update-task)
- [System Architecture](#system-architecture)

---

## Main Application Flow

This flowchart shows the complete execution path from system startup to normal operation.

```mermaid
flowchart TD
    Start([System Boot]) --> Init1[Initialize NVS Flash]
    Init1 --> Init2[Initialize Display<br/>Portrait or Landscape]
    Init2 --> Init3[Initialize Backlight<br/>PWM on GPIO 23]
    Init3 --> DispConnecting[Display: Connecting to WiFi...]
    
    DispConnecting --> WiFiInit{WiFi<br/>Initialization}
    WiFiInit -->|Success| WiFiOK[WiFi Connected<br/>Got IP Address]
    WiFiInit -->|Failure<br/>5 Retries| WiFiFail[Display: WiFi<br/>Connection Failed!]
    WiFiFail --> Halt1([System Halts])
    
    WiFiOK --> TZSet[Set Timezone<br/>EST/EDT for Miami]
    TZSet --> SNTPInit[Initialize SNTP Client<br/>Server: pool.ntp.org]
    
    SNTPInit --> WaitSync{Wait for<br/>Time Sync<br/>30s Timeout}
    WaitSync -->|Success| SyncOK[Time Synchronized]
    WaitSync -->|Timeout| SyncFail[Display: Time<br/>Sync Failed!]
    SyncFail --> Halt2([System Halts])
    
    SyncOK --> ClearScreen[Clear Display<br/>Black Background]
    ClearScreen --> CreateTask[Create FreeRTOS Task:<br/>time_display_task<br/>Priority 5, Stack 4096]
    
    CreateTask --> MainLoop{Main Loop}
    MainLoop --> Delay[Delay 1 Second]
    Delay --> MainLoop
    
    style Start fill:#90EE90
    style Halt1 fill:#FFB6C1
    style Halt2 fill:#FFB6C1
    style WiFiOK fill:#87CEEB
    style SyncOK fill:#87CEEB
    style CreateTask fill:#FFD700
```

---

## WiFi Connection State Machine

Detailed state machine showing WiFi connection handling with event-driven architecture.

```mermaid
flowchart TD
    Start([app_main calls<br/>wifi_init_sta]) --> Init[Initialize WiFi<br/>Create Event Group<br/>Register Handlers]
    
    Init --> Config[Set WiFi Config<br/>Mode: STA<br/>Auth: WPA2-PSK<br/>SSID + Password]
    
    Config --> StartWiFi[Start WiFi<br/>esp_wifi_start]
    
    StartWiFi --> WaitStart[Wait for<br/>WIFI_EVENT_STA_START]
    
    WaitStart --> Connect[Attempt Connection<br/>esp_wifi_connect<br/>Retry: 0/5]
    
    Connect --> WaitResult{Wait for Event}
    
    WaitResult -->|IP_EVENT_STA_GOT_IP| Success[Connected!<br/>IP Address Acquired<br/>Set WIFI_CONNECTED_BIT]
    
    WaitResult -->|WIFI_EVENT_STA_DISCONNECTED| CheckRetry{Retry Count<br/>< 5?}
    
    CheckRetry -->|Yes| Increment[Increment Retry<br/>Wait 1 Second]
    Increment --> Connect
    
    CheckRetry -->|No| Failed[Connection Failed<br/>Set WIFI_FAIL_BIT<br/>Display Error Message]
    
    Success --> Return1([Return ESP_OK<br/>Continue to NTP Sync])
    Failed --> Return2([Return ESP_FAIL<br/>System Halts])
    
    style Start fill:#90EE90
    style Success fill:#87CEEB
    style Failed fill:#FFB6C1
    style Return1 fill:#90EE90
    style Return2 fill:#DC143C
```

---

## Time Synchronization Flow

SNTP client initialization and time synchronization process.

```mermaid
flowchart TD
    Start([SNTP Initialize]) --> SetMode[Set Operating Mode:<br/>SNTP_OPMODE_POLL]
    SetMode --> SetServer[Set NTP Server:<br/>pool.ntp.org]
    SetServer --> RegCallback[Register Sync Callback:<br/>time_sync_notification_cb]
    RegCallback --> SNTPStart[Start SNTP Client:<br/>esp_sntp_init]
    
    SNTPStart --> Query[Send NTP Query<br/>UDP Port 123]
    Query --> Wait{Wait for<br/>NTP Response}
    
    Wait -->|Success| Response[Receive NTP Packet<br/>Time Data]
    Wait -->|Timeout| RetryQ{Retry<br/>Count}
    
    RetryQ -->|< 30s| Query
    RetryQ -->|>= 30s| TimeoutFail[Time Sync Failed]
    
    Response --> Callback[Callback Triggered:<br/>time_sync_notification_cb]
    Callback --> SetFlag[Set time_synced = true]
    SetFlag --> UpdateTime[System Time Updated]
    UpdateTime --> TZApply[Apply Timezone & DST]
    TZApply --> Ready([Time Ready for Display])
    
    TimeoutFail --> Error([Display Error Message])
    
    Ready --> AutoSync[Automatic Resync<br/>Every 3600 seconds]
    AutoSync -.->|Hourly| Query
    
    style Start fill:#90EE90
    style Ready fill:#87CEEB
    style Error fill:#FFB6C1
    style Callback fill:#FFD700
    style AutoSync fill:#DDA0DD
    
    note1[NTP Request]
    note2[Network Layer:<br/>LWIP UDP Stack]
    note3[50 bytes per query]
    
    Query -.-> note1
    note1 -.-> note2
    note2 -.-> note3
```

---

## Display Update Task

FreeRTOS task responsible for continuous display updates.

```mermaid
flowchart TD
    TaskStart([time_display_task<br/>FreeRTOS Task]) --> ClearInit[Clear Screen<br/>Fill with Black]
    
    ClearInit --> Loop{Infinite Loop}
    
    Loop --> CheckSync{time_synced<br/>Flag Set?}
    
    CheckSync -->|No| SkipDisplay[Skip Display Update]
    CheckSync -->|Yes| GetTime[Get Current System Time:<br/>time & localtime_r]
    
    GetTime --> FormatDate[Format Date String:<br/>MMM DD YYYY<br/>Example: Dec 04 2024]
    FormatDate --> FormatTime[Format Time String:<br/>HH:MM:SS AM/PM<br/>Example: 03:45:30 PM]
    
    FormatTime --> CalcDatePos[Calculate Date Position:<br/>Center on Screen<br/>x = LCD_WIDTH - string_width / 2]
    CalcDatePos --> DrawDate[Draw Date String<br/>at y=60, Color: Yellow]
    
    DrawDate --> CalcTimePos[Calculate Time Position:<br/>Center on Screen]
    CalcTimePos --> DrawTime[Draw Time String<br/>at y=90, Color: Yellow]
    
    DrawTime --> Complete[Display Updated]
    SkipDisplay --> Delay[vTaskDelay<br/>1000 ms]
    Complete --> Delay
    
    Delay --> Loop
    
    style TaskStart fill:#90EE90
    style Complete fill:#87CEEB
    style Loop fill:#FFD700
    
    subgraph Drawing Process
        DrawDate
        DrawTime
    end
    
    subgraph Time Formatting
        GetTime
        FormatDate
        FormatTime
    end
    
    note1[Display Coordinates:<br/>Landscape Mode<br/>320 x 172 pixels]
    note2[Font: 5x8 or 8x12<br/>Scale: 3x<br/>Color: RGB565]
    
    CalcDatePos -.-> note1
    DrawDate -.-> note2
```

---

## System Architecture

High-level component interaction diagram.

```mermaid
flowchart TB
    subgraph Hardware [Hardware Layer]
        ESP32[ESP32-C6<br/>RISC-V MCU]
        LCD[JD9853 LCD<br/>172x320 RGB565]
        WiFi[2.4GHz WiFi<br/>802.11 b/g/n]
        BL[Backlight LED<br/>PWM Control]
    end
    
    subgraph Drivers [Driver Layer]
        SPIDrv[SPI Master Driver<br/>GPIO 1,2,14,15]
        LCDDrv[LCD Panel Driver<br/>esp_lcd_jd9853]
        PWMDrv[LEDC Driver<br/>GPIO 23]
        WiFiDrv[WiFi Driver<br/>ESP-IDF WiFi]
    end
    
    subgraph Middleware [Middleware Layer]
        LWIP[LWIP TCP/IP Stack<br/>Network Protocol]
        SNTP[SNTP Client<br/>Time Sync]
        EventLoop[Event Loop<br/>WiFi Events]
        NVS[NVS Flash<br/>Config Storage]
    end
    
    subgraph Application [Application Layer]
        Main[app_main<br/>Entry Point]
        WiFiMgr[WiFi Manager<br/>Connection Logic]
        TimeMgr[Time Manager<br/>Timezone & Format]
        DisplayMgr[Display Manager<br/>Rendering Engine]
        Task[Display Task<br/>Update Loop]
    end
    
    subgraph External [External Services]
        NTPServer[NTP Pool<br/>pool.ntp.org]
        Router[WiFi Router<br/>Internet Gateway]
    end
    
    ESP32 --> SPIDrv
    ESP32 --> WiFiDrv
    ESP32 --> PWMDrv
    
    SPIDrv --> LCDDrv
    LCDDrv --> LCD
    PWMDrv --> BL
    WiFiDrv --> WiFi
    
    WiFiDrv --> LWIP
    LWIP --> SNTP
    WiFiDrv --> EventLoop
    
    Main --> WiFiMgr
    Main --> TimeMgr
    Main --> DisplayMgr
    Main --> Task
    
    WiFiMgr --> EventLoop
    WiFiMgr --> WiFiDrv
    TimeMgr --> SNTP
    DisplayMgr --> LCDDrv
    Task --> DisplayMgr
    Task --> TimeMgr
    
    WiFi --> Router
    Router --> NTPServer
    SNTP -.->|UDP 123| NTPServer
    
    Main --> NVS
    
    style ESP32 fill:#FFB6C1
    style LCD fill:#87CEEB
    style Main fill:#FFD700
    style Task fill:#90EE90
    style NTPServer fill:#DDA0DD
```

---

## Component Interaction Sequence

Detailed sequence diagram showing component communication during startup.

```mermaid
sequenceDiagram
    participant Main as app_main()
    participant NVS as NVS Flash
    participant Display as Display Driver
    participant WiFi as WiFi Manager
    participant SNTP as SNTP Client
    participant NTP as NTP Server
    participant Task as Display Task
    
    Main->>NVS: Initialize NVS Flash
    activate NVS
    NVS-->>Main: ESP_OK
    deactivate NVS
    
    Main->>Display: Initialize LCD Panel
    activate Display
    Display->>Display: Configure SPI
    Display->>Display: Send Init Commands
    Display->>Display: Set Orientation
    Display-->>Main: Panel Ready
    deactivate Display
    
    Main->>Display: Show "Connecting..."
    activate Display
    Display->>Display: Draw Text
    deactivate Display
    
    Main->>WiFi: wifi_init_sta()
    activate WiFi
    WiFi->>WiFi: Create Event Group
    WiFi->>WiFi: Register Handlers
    WiFi->>WiFi: Start WiFi
    WiFi->>WiFi: Connect to AP
    
    Note over WiFi: Retry up to 5 times
    
    WiFi-->>Main: WIFI_CONNECTED_BIT
    deactivate WiFi
    
    Main->>Main: Set Timezone (EST/EDT)
    
    Main->>SNTP: Initialize SNTP
    activate SNTP
    SNTP->>SNTP: Set Server
    SNTP->>SNTP: Set Callback
    SNTP->>SNTP: Start Client
    
    SNTP->>NTP: NTP Query (UDP 123)
    activate NTP
    NTP-->>SNTP: NTP Response
    deactivate NTP
    
    SNTP->>SNTP: Trigger Callback
    SNTP-->>Main: time_synced = true
    deactivate SNTP
    
    Main->>Task: xTaskCreate(time_display_task)
    activate Task
    
    loop Every Second
        Task->>Task: Get System Time
        Task->>Task: Format Date/Time
        Task->>Display: Draw Date String
        activate Display
        Display->>Display: Render Characters
        deactivate Display
        Task->>Display: Draw Time String
        activate Display
        Display->>Display: Render Characters
        deactivate Display
        Task->>Task: vTaskDelay(1000ms)
    end
    
    deactivate Task
```

---

## Error Handling Flow

Error handling paths and recovery mechanisms.

```mermaid
flowchart TD
    Start([Error Detected]) --> Type{Error Type}
    
    Type -->|WiFi Init Failed| WiFiErr[WiFi Initialization Error]
    Type -->|WiFi Connect Failed| ConnErr[Connection Failed<br/>After 5 Retries]
    Type -->|SNTP Timeout| SNTPErr[Time Sync Timeout<br/>After 30 Seconds]
    Type -->|Display Error| DispErr[Display Init Failed]
    Type -->|NVS Error| NVSErr[NVS Flash Error]
    
    WiFiErr --> DispFailMsg1[Display Error Message:<br/>WiFi Init Failed]
    ConnErr --> DispFailMsg2[Display Error Message:<br/>WiFi Connection Failed]
    SNTPErr --> DispFailMsg3[Display Error Message:<br/>Time Sync Failed]
    DispErr --> LogError1[Log to Serial:<br/>Display Error]
    NVSErr --> Erase[Erase NVS Flash<br/>nvs_flash_erase]
    
    DispFailMsg1 --> Halt1([System Halts])
    DispFailMsg2 --> Halt2([System Halts])
    DispFailMsg3 --> Halt3([System Halts])
    LogError1 --> Halt4([System Halts])
    
    Erase --> Retry[Retry NVS Init]
    Retry --> RetryCheck{Init Success?}
    RetryCheck -->|Yes| Continue([Continue Boot])
    RetryCheck -->|No| Halt5([System Halts])
    
    style Start fill:#FFB6C1
    style Halt1 fill:#DC143C
    style Halt2 fill:#DC143C
    style Halt3 fill:#DC143C
    style Halt4 fill:#DC143C
    style Halt5 fill:#DC143C
    style Continue fill:#90EE90
    
    subgraph Recoverable Errors
        NVSErr
        Erase
        Retry
    end
    
    subgraph Non-Recoverable Errors
        WiFiErr
        ConnErr
        SNTPErr
        DispErr
    end
```

---

## Memory Management

Memory allocation and usage across system components.

```mermaid
pie title ESP32-C6 Memory Allocation (Total: ~580KB RAM)
    "WiFi Stack" : 30
    "LWIP TCP/IP" : 20
    "Display Buffers" : 10
    "FreeRTOS Tasks" : 20
    "Free Heap" : 420
```

---

## Data Flow - Time Display

Shows the flow of time data from NTP server to display pixels.

```mermaid
flowchart LR
    NTP[NTP Server<br/>pool.ntp.org] -->|UDP Packet<br/>50 bytes| SNTP[SNTP Client<br/>LWIP Stack]
    
    SNTP -->|Unix Timestamp| SysTime[System Time<br/>time_t]
    
    SysTime -->|time & localtime_r| TZ[Timezone<br/>Conversion]
    
    TZ -->|struct tm| Format[Time Formatting<br/>strftime]
    
    Format -->|Date String<br/>MMM DD YYYY| Render1[Text Renderer]
    Format -->|Time String<br/>HH:MM:SS AP| Render2[Text Renderer]
    
    Render1 -->|Character Pixels| Font1[Font Lookup<br/>5x8 or 8x12]
    Render2 -->|Character Pixels| Font2[Font Lookup<br/>5x8 or 8x12]
    
    Font1 -->|Scaled Pixels<br/>x3 Zoom| Draw1[SPI Draw<br/>Operations]
    Font2 -->|Scaled Pixels<br/>x3 Zoom| Draw2[SPI Draw<br/>Operations]
    
    Draw1 -->|RGB565 Data| LCD[LCD Panel<br/>JD9853]
    Draw2 -->|RGB565 Data| LCD
    
    LCD -->|Display Output| Screen[Physical Screen<br/>172x320 pixels]
    
    style NTP fill:#DDA0DD
    style SysTime fill:#87CEEB
    style Screen fill:#90EE90
    style LCD fill:#FFD700
```

---

## Build and Deploy Process

Development workflow from source to running device.

```mermaid
flowchart TD
    Source[Source Code<br/>main.c + components] --> Config[Configure WiFi<br/>SSID + Password]
    Config --> Target[Set Target<br/>idf.py set-target esp32c6]
    Target --> Build[Build Project<br/>idf.py build]
    
    Build -->|Success| Flash[Flash to Device<br/>idf.py flash]
    Build -->|Error| BuildErr[Build Errors]
    BuildErr --> Fix[Fix Compilation Errors]
    Fix --> Build
    
    Flash --> Monitor[Serial Monitor<br/>idf.py monitor]
    
    Monitor --> Runtime{Runtime<br/>Behavior}
    
    Runtime -->|Success| Running[Device Running<br/>Display Time]
    Runtime -->|WiFi Error| Debug1[Debug WiFi<br/>Check Credentials]
    Runtime -->|SNTP Error| Debug2[Debug Network<br/>Check Internet]
    Runtime -->|Display Error| Debug3[Debug Hardware<br/>Check Connections]
    
    Debug1 --> Config
    Debug2 --> Config
    Debug3 --> Source
    
    Running --> Update[Monitor Operation<br/>Serial Logs]
    
    style Source fill:#90EE90
    style Running fill:#87CEEB
    style BuildErr fill:#FFB6C1
    style Debug1 fill:#FFD700
    style Debug2 fill:#FFD700
    style Debug3 fill:#FFD700
```

---

## Legend

### Node Colors
- 🟢 **Green**: Start/Success states
- 🔵 **Blue**: Normal operation states
- 🟡 **Yellow**: Important/Action states
- 🔴 **Red**: Error/Halt states
- 🟣 **Purple**: External services

### Connection Types
- **Solid Line** (→): Direct execution flow
- **Dashed Line** (-.->): Reference/Note connection
- **Thick Line** (⇒): Data flow

---

## Notes

1. All diagrams are rendered using Mermaid syntax compatible with GitHub, GitLab, and most documentation platforms.

2. The flowcharts show the actual code execution paths based on the `main.c` implementation.

3. Error handling is comprehensive but non-recoverable errors halt the system to prevent undefined behavior.

4. The display update task runs independently after initialization, ensuring smooth time updates without blocking.

5. WiFi reconnection is handled automatically by the ESP-IDF WiFi stack after initial connection.

---

**Document Version**: 1.0  
**Last Updated**: December 4, 2025  
**Project**: ESP32-C6 WiFi Internet Clock  
**License**: MIT
