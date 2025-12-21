# ESP32-C6 Touch Demo - Program Flowchart

## Complete System Flow

```mermaid
flowchart TD
    Start([System Start]) --> DisplayInit[Initialize Display]
    
    DisplayInit --> SPIBus[Configure SPI Bus]
    SPIBus --> SPIInit{SPI Init OK?}
    SPIInit -->|Error| Error1[Log Error & Stop]
    SPIInit -->|Success| PanelIO[Create Panel IO]
    
    PanelIO --> PanelCreate[Create JD9853 Panel]
    PanelCreate --> PanelReset[Reset Panel]
    PanelReset --> PanelInit[Initialize Panel]
    PanelInit --> PanelInvert[Enable Color Invert]
    PanelInvert --> PanelMirror[Configure Mirror/Swap]
    PanelMirror --> PanelOn[Turn Display On]
    PanelOn --> PanelGap[Set Gap Offset 34,0]
    PanelGap --> Backlight[Initialize Backlight PWM]
    
    Backlight --> I2CInit[Initialize I2C Bus]
    I2CInit --> I2CCfg[Configure I2C Master<br/>Port 0, 400kHz]
    I2CCfg --> I2CPullup[Enable Internal Pullups]
    I2CPullup --> I2CCheck{I2C Init OK?}
    I2CCheck -->|Error| Error2[Log Error & Stop]
    
    I2CCheck -->|Success| TouchInit[Initialize Touch Controller]
    TouchInit --> TouchDev[Add I2C Device<br/>Address 0x63]
    TouchDev --> TouchCfg[Configure AXS5106<br/>172x320, Mirror X]
    TouchCfg --> TouchRst[Initialize RST/INT Pins]
    TouchRst --> TouchCheck{Touch Init OK?}
    TouchCheck -->|Error| Error3[Log Error & Stop]
    
    TouchCheck -->|Success| TaskCreate[Create Touch Task<br/>Priority 5, 4KB Stack]
    TaskCreate --> MainLoop[Main Loop]
    
    MainLoop --> Wait[Wait 1 Second]
    Wait --> MainLoop
    
    TaskCreate --> TouchTask[Touch Task Start]
    TouchTask --> ShowTest[Display Touch Test Screen]
    ShowTest --> FillWhite[Fill Screen White]
    FillWhite --> DrawTitle[Draw 'Touch Test Mode']
    DrawTitle --> DrawInst[Draw Instructions]
    DrawInst --> InitVars[Initialize Variables<br/>last_x=-1, last_y=-1]
    
    InitVars --> TouchLoop[Touch Task Loop]
    TouchLoop --> ReadTouch[Read Touch Data from Controller]
    ReadTouch --> GetData[Get Touch Data Structure]
    GetData --> CheckRet{Return OK?}
    
    CheckRet -->|Error| Delay1[Delay 50ms]
    Delay1 --> TouchLoop
    
    CheckRet -->|OK| HasPoint{Has Touch Points?}
    HasPoint -->|No| Delay1
    
    HasPoint -->|Yes| GetXY[Extract X, Y from Data Structure]
    GetXY --> Changed{Position<br/>Changed?}
    
    Changed -->|No| Delay1
    
    Changed -->|Yes| LogTouch[Log Touch Event to Serial]
    LogTouch --> ClearScreen[Fill Screen White]
    ClearScreen --> DrawHeader[Draw 'Touch at:' Text]
    DrawHeader --> DrawX[Draw X Coordinate]
    DrawX --> DrawY[Draw Y Coordinate]
    DrawY --> DrawCircle[Draw Red Circle<br/>Radius 12px]
    DrawCircle --> UpdateLast[Update last_x, last_y]
    UpdateLast --> Delay2[Delay 50ms]
    Delay2 --> TouchLoop
    
    Error1 --> End([System Halt])
    Error2 --> End
    Error3 --> End

    style Start fill:#90EE90
    style End fill:#FFB6C1
    style Error1 fill:#FF6B6B
    style Error2 fill:#FF6B6B
    style Error3 fill:#FF6B6B
    style TouchLoop fill:#87CEEB
    style MainLoop fill:#DDA0DD
    style TouchTask fill:#F0E68C
```

## Initialization Sequence

```mermaid
sequenceDiagram
    participant Main as app_main()
    participant Display as Display System
    participant SPI as SPI Bus
    participant Panel as JD9853 Panel
    participant BL as Backlight
    participant I2C as I2C Bus
    participant Touch as AXS5106 Touch
    participant Task as Touch Task

    Main->>Display: Initialize
    Display->>SPI: Configure Bus
    SPI-->>Display: Bus Ready
    Display->>Panel: Create & Configure
    Panel->>Panel: Reset
    Panel->>Panel: Initialize
    Panel->>Panel: Color Invert
    Panel->>Panel: Mirror/Swap
    Panel->>Panel: Display ON
    Panel->>Panel: Set Gap (34,0)
    Panel-->>Display: Ready
    
    Display->>BL: Initialize PWM
    BL-->>Display: Backlight ON
    Display-->>Main: Display Ready
    
    Main->>I2C: Initialize Bus
    I2C->>I2C: Configure Master
    I2C->>I2C: Enable Pullups
    I2C-->>Main: I2C Ready
    
    Main->>Touch: Initialize Controller
    Touch->>I2C: Add Device 0x63
    I2C-->>Touch: Device Added
    Touch->>Touch: Configure AXS5106
    Touch->>Touch: Setup RST/INT
    Touch-->>Main: Touch Ready
    
    Main->>Task: Create Touch Task
    Task->>Task: Display Test Screen
    Task->>Task: Start Touch Loop
    Task-->>Main: Task Running
    
    Main->>Main: Enter Main Loop
```

## Touch Processing Flow

```mermaid
flowchart LR
    subgraph Input["Touch Input"]
        A[User Touches Screen]
        B[AXS5106 Detects Touch]
        C[INT Pin Signals]
    end
    
    subgraph Read["Data Reading"]
        D[Touch Task Polls]
        E[Read I2C Data]
        F[Get Data Structure]
    end
    
    subgraph Process["Processing"]
        G{Data Valid?}
        H{Position Changed?}
        I[Extract X, Y]
    end
    
    subgraph Display["Display Update"]
        J[Clear Screen]
        K[Draw Text]
        L[Draw Coordinates]
        M[Draw Circle]
    end
    
    subgraph Output["Output"]
        N[Update Display]
        O[Log to Serial]
        P[Wait 50ms]
    end
    
    A --> B --> C
    C --> D --> E --> F
    F --> G
    G -->|No| P
    G -->|Yes| H
    H -->|No| P
    H -->|Yes| I
    I --> J --> K --> L --> M
    M --> N
    N --> O --> P
    P --> D

    style A fill:#90EE90
    style N fill:#FFD700
    style O fill:#87CEEB
```

## System Architecture

```mermaid
graph TB
    subgraph Hardware["Hardware Layer"]
        MCU[ESP32-C6<br/>RISC-V 160MHz]
        LCD[JD9853 Display<br/>172x320 SPI]
        TP[AXS5106 Touch<br/>I2C 0x63]
    end
    
    subgraph Drivers["Driver Layer"]
        SPID[SPI Driver<br/>80MHz]
        I2CD[I2C Master<br/>400kHz]
        LCDD[LCD Panel Driver]
        TPD[Touch Driver]
        PWM[PWM Backlight]
    end
    
    subgraph App["Application Layer"]
        INIT[Initialization]
        GRAPH[Graphics Functions]
        TOUCH[Touch Handler]
        TASK[FreeRTOS Task]
    end
    
    subgraph RTOS["FreeRTOS"]
        SCH[Scheduler]
        MEM[Memory Manager]
    end
    
    MCU --- SPID
    MCU --- I2CD
    MCU --- PWM
    
    LCD --- SPID
    TP --- I2CD
    
    SPID --- LCDD
    I2CD --- TPD
    
    LCDD --- GRAPH
    TPD --- TOUCH
    PWM --- INIT
    
    GRAPH --- APP[Main Application]
    TOUCH --- APP
    INIT --- APP
    
    APP --- TASK
    TASK --- SCH
    APP --- MEM
    
    SCH --- MCU
    MEM --- MCU

    style MCU fill:#FFB6C1
    style LCD fill:#87CEEB
    style TP fill:#90EE90
    style APP fill:#FFD700
```

## Memory Layout

```mermaid
graph LR
    subgraph Flash["Flash Memory ~ 218 KB"]
        A1[Display Driver<br/>80 KB]
        A2[Touch Driver<br/>10 KB]
        A3[I2C Driver<br/>10 KB]
        A4[Application<br/>58 KB]
        A5[System<br/>60 KB]
    end
    
    subgraph RAM["RAM ~ 59 KB"]
        B1[Display Buffers<br/>10 KB]
        B2[Touch Buffers<br/>1 KB]
        B3[Task Stacks<br/>10 KB]
        B4[System<br/>38 KB]
    end
    
    subgraph Heap["Free Heap ~ 441 KB"]
        C1[Available Memory]
    end

    style Flash fill:#FFE4B5
    style RAM fill:#ADD8E6
    style Heap fill:#90EE90
```

## State Machine

```mermaid
stateDiagram-v2
    [*] --> PowerOn
    PowerOn --> Initializing: System Start
    
    Initializing --> Display_Init: Start Init
    Display_Init --> I2C_Init: Display OK
    I2C_Init --> Touch_Init: I2C OK
    Touch_Init --> Ready: Touch OK
    
    Ready --> Idle: No Touch
    Idle --> TouchDetected: Touch Event
    TouchDetected --> Processing: Read Data
    Processing --> Displaying: Valid Data
    Displaying --> Idle: Update Complete
    
    Initializing --> Error: Init Failed
    Error --> [*]
    
    note right of Ready
        Touch task running
        Polling every 50ms
    end note
    
    note right of Processing
        Extract X, Y
        Check if changed
    end note
    
    note right of Displaying
        Clear screen
        Draw text
        Draw circle
        Log to serial
    end note
```

## Task Timing

```mermaid
gantt
    title Touch Task Timing Diagram
    dateFormat X
    axisFormat %L ms
    
    section Cycle
    Read Touch Data     :0, 5
    Get Data Structure  :5, 8
    Check Return        :8, 9
    Clear Screen        :9, 59
    Draw Text           :59, 79
    Draw Coordinates    :79, 89
    Draw Circle         :89, 99
    Log Serial          :99, 100
    Task Delay          :100, 150
    
    section Timing
    Touch Response      :crit, 0, 55
    Display Update      :0, 100
    Cycle Period        :0, 150
```

## Component Dependencies

```mermaid
graph TD
    A[main.c] --> B[esp_lcd_panel_io.h]
    A --> C[esp_lcd_panel_ops.h]
    A --> D[esp_lcd_jd9853.h]
    A --> E[esp_lcd_touch.h]
    A --> F[esp_lcd_touch_axs5106.h]
    A --> G[driver/spi_master.h]
    A --> H[driver/i2c_master.h]
    A --> I[driver/ledc.h]
    A --> J[freertos/FreeRTOS.h]
    
    D --> K[JD9853 Driver Component]
    F --> L[AXS5106 Driver Component]
    E --> M[esp_lcd_touch Registry Component]
    
    K --> N[esp_lcd Framework]
    L --> M
    M --> N
    
    style A fill:#FFD700
    style M fill:#90EE90
    style K fill:#87CEEB
    style L fill:#FFB6C1
```

## Build Process

```mermaid
flowchart TD
    Start([idf.py build]) --> Target{Target Set?}
    Target -->|No| SetTarget[idf.py set-target esp32c6]
    SetTarget --> Parse
    Target -->|Yes| Parse[Parse CMakeLists.txt]
    
    Parse --> Deps[Check Dependencies]
    Deps --> Registry{Components<br/>Downloaded?}
    Registry -->|No| Download[Download from<br/>ESP Component Registry]
    Download --> Cache[Cache in managed_components/]
    Cache --> Compile
    Registry -->|Yes| Compile[Compile Components]
    
    Compile --> C1[Compile Display Driver]
    C1 --> C2[Compile Touch Driver]
    C2 --> C3[Compile Application]
    C3 --> Link[Link Everything]
    
    Link --> Binary[Create Firmware Binary]
    Binary --> Success([Build Complete])
    
    style Start fill:#90EE90
    style Success fill:#FFD700
    style Download fill:#87CEEB
```

## Application Code Flow

```mermaid
flowchart TD
    Start([app_main Start]) --> Log[Log Startup Message]
    
    Log --> DispInit[Call display_init]
    DispInit --> DispCheck{Success?}
    DispCheck -->|No| Error1[Fatal Error]
    DispCheck -->|Yes| BLInit[Call backlight_init]
    
    BLInit --> I2CInit[Call i2c_init]
    I2CInit --> I2CCheck{Success?}
    I2CCheck -->|No| Error2[Fatal Error]
    I2CCheck -->|Yes| TouchInit[Call touch_init]
    
    TouchInit --> TouchCheck{Success?}
    TouchCheck -->|No| Error3[Fatal Error]
    TouchCheck -->|Yes| CreateTask[xTaskCreate touch_task]
    
    CreateTask --> MainLoop[Infinite Loop]
    MainLoop --> Delay[vTaskDelay 1000ms]
    Delay --> MainLoop
    
    Error1 --> Halt([System Halt])
    Error2 --> Halt
    Error3 --> Halt
    
    style Start fill:#90EE90
    style Halt fill:#FF6B6B
    style MainLoop fill:#87CEEB
```

## Touch Data Structure

```mermaid
classDiagram
    class esp_lcd_touch_point_data_t {
        +uint16_t x
        +uint16_t y
        +uint16_t strength
    }
    
    class TouchTask {
        +touchpad_data[1]
        +touchpad_cnt
        +last_x
        +last_y
        +touch_task()
    }
    
    class TouchAPI {
        +esp_lcd_touch_read_data()
        +esp_lcd_touch_get_data()
    }
    
    TouchTask --> esp_lcd_touch_point_data_t : uses
    TouchTask --> TouchAPI : calls
```

---

## Flowchart Legend

| Symbol | Meaning |
|--------|---------|
| 🟢 Green | Start/Entry Point |
| 🔵 Blue | Process/Action |
| 🟡 Yellow | Decision Point |
| 🔴 Red | Error State |
| 🟣 Purple | Loop |

## Usage

These Mermaid diagrams can be rendered on:
- GitHub (native support)
- GitLab (native support)
- VS Code (with Mermaid extension)
- [Mermaid Live Editor](https://mermaid.live/)

## Diagram Types Included

1. **Complete System Flow** - Full program execution
2. **Initialization Sequence** - Step-by-step startup
3. **Touch Processing Flow** - Touch event handling
4. **System Architecture** - Component relationships
5. **Memory Layout** - Flash/RAM allocation
6. **State Machine** - System states
7. **Task Timing** - Timing diagram
8. **Component Dependencies** - File relationships
9. **Build Process** - Build system flow
10. **Application Code Flow** - Main function flow
11. **Touch Data Structure** - Data structure diagram

## Key Changes from v6.0

- Removed NVS initialization from all flowcharts
- Updated memory usage (218 KB flash, 59 KB RAM)
- Updated initialization sequence (no NVS step)
- Updated system flow (starts directly with display init)
- Updated state machine (no NVS state)
- Updated touch API (uses new esp_lcd_touch_get_data)
- Added touch data structure diagram
- Simplified initialization process
