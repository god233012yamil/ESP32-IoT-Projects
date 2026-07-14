# Project Flowchart

This document describes the ESP32-S3 BLE long-range firmware flow using Mermaid diagrams.

## System Overview

```mermaid
flowchart TD
    App["app_main"] --> Init["ble_long_range_init"]
    Init --> NVS["Initialize NVS"]
    Init --> BT["Initialize BLE controller and Bluedroid"]
    Init --> Callbacks["Register GATTS and GAP callbacks"]
    Init --> AppReg["Register GATT application"]
    Init --> MTU["Set local ATT MTU to 247"]
    Init --> DefaultPhy["Prefer LE Coded PHY by default"]
    Init --> TxPower["Set default and advertising TX power"]
    Init --> RssiTask["Start RSSI monitor task"]

    AppReg --> GattsReg["ESP_GATTS_REG_EVT"]
    GattsReg --> Name["Set device name: ESP32S3-LR-BLE"]
    Name --> AttrTable["Create GATT attribute table"]
    AttrTable --> Service["Start GATT service"]
    Service --> AdvParams["Configure extended advertising parameters"]
    AdvParams --> AdvData["Configure extended advertising data"]
    AdvData --> AdvStart["Start connectable LE Coded PHY advertising"]
```

## Advertising and Connection Flow

```mermaid
flowchart TD
    AdvStart["Extended Coded PHY advertising active"] --> Scan["Coded PHY capable central scans"]
    Scan --> Connect{"Central connects?"}
    Connect -- "No" --> AdvStart
    Connect -- "Yes" --> Connected["ESP_GATTS_CONNECT_EVT"]

    Connected --> SavePeer["Store connection ID, handle, and peer address"]
    SavePeer --> ResetState["Reset filtered RSSI and sample count"]
    ResetState --> MaxPower["Request ESP_PWR_LVL_P20 for connection"]
    MaxPower --> CodedS8["Request LE Coded PHY with S=8 preference"]
    CodedS8 --> Active["Connected telemetry and control session"]

    Active --> Disconnect{"Disconnect event?"}
    Disconnect -- "No" --> Active
    Disconnect -- "Yes" --> ClearState["Clear connection and notification state"]
    ClearState --> ResetPhy["Reset tracked PHY to 1M"]
    ResetPhy --> AdvStart
```

## RSSI Telemetry and Adaptive Policy

```mermaid
flowchart TD
    Task["RSSI monitor task"] --> IsConnected{"Connected?"}
    IsConnected -- "No" --> Delay["Delay and poll again"]
    Delay --> Task
    IsConnected -- "Yes" --> ReadRSSI["esp_ble_gap_read_rssi(peer)"]
    ReadRSSI --> GapEvent["ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT"]
    GapEvent --> ValidRSSI{"Read successful?"}
    ValidRSSI -- "No" --> Delay
    ValidRSSI -- "Yes" --> Filter["Update low-pass filtered RSSI"]
    Filter --> Telemetry["Publish telemetry characteristic"]
    Telemetry --> Notify{"Notifications enabled?"}
    Notify -- "Yes" --> SendNotify["Send telemetry notification"]
    Notify -- "No" --> Policy["Apply link policy"]
    SendNotify --> Policy

    Policy --> Strong{"filtered >= -67 dBm?"}
    Strong -- "Yes" --> StrongAction["Power P3 and request LE 1M"]
    Strong -- "No" --> Moderate{"filtered >= -82 dBm?"}
    Moderate -- "Yes" --> ModerateAction["Power P9 and request LE 1M"]
    Moderate -- "No" --> Weak{"filtered >= -92 dBm?"}
    Weak -- "Yes" --> WeakAction["Power P15 and request LE Coded S=2"]
    Weak -- "No" --> RangeAction["Power P20 and request LE Coded S=8"]

    StrongAction --> Delay
    ModerateAction --> Delay
    WeakAction --> Delay
    RangeAction --> Delay
```

## Control Characteristic Flow

```mermaid
flowchart TD
    Write["ESP_GATTS_WRITE_EVT"] --> Target{"Write target"}
    Target -- "Telemetry CCCD" --> CCCD["Parse CCCD value"]
    CCCD --> NotifyState["Enable or disable notifications"]

    Target -- "Control characteristic" --> Command["Normalize ASCII command"]
    Command --> IsAuto{"AUTO?"}
    IsAuto -- "Yes" --> Auto["Apply adaptive policy immediately"]
    IsAuto -- "No" --> Is1M{"1M?"}
    Is1M -- "Yes" --> Req1M["Request LE 1M PHY"]
    Is1M -- "No" --> IsS2{"S2?"}
    IsS2 -- "Yes" --> ReqS2["Request LE Coded PHY S=2"]
    IsS2 -- "No" --> IsS8{"S8?"}
    IsS8 -- "Yes" --> ReqS8["Request LE Coded PHY S=8"]
    IsS8 -- "No" --> IsLow{"LOW?"}
    IsLow -- "Yes" --> PowerLow["Set connection power P3"]
    IsLow -- "No" --> IsMedium{"MEDIUM?"}
    IsMedium -- "Yes" --> PowerMedium["Set connection power P9"]
    IsMedium -- "No" --> IsHigh{"HIGH?"}
    IsHigh -- "Yes" --> PowerHigh["Set connection power P15"]
    IsHigh -- "No" --> IsMax{"MAX?"}
    IsMax -- "Yes" --> PowerMax["Set connection power P20"]
    IsMax -- "No" --> Unknown["Ignore unknown command and log warning"]
```

## GAP and GATTS Event Responsibilities

```mermaid
flowchart LR
    subgraph GAP["GAP callback"]
        GapAdvParams["Advertising parameters complete"]
        GapAdvData["Advertising data complete"]
        GapAdvStart["Advertising start complete"]
        GapPhy["PHY update complete"]
        GapRSSI["RSSI read complete"]
    end

    subgraph GATTS["GATTS callback"]
        GattsReg["Application registered"]
        GattsAttr["Attribute table created"]
        GattsConnect["Peer connected"]
        GattsDisconnect["Peer disconnected"]
        GattsWrite["Characteristic or CCCD written"]
        GattsMtu["MTU updated"]
    end

    GapAdvParams --> GapAdvData
    GapAdvData --> GapAdvStart
    GapPhy --> TrackPhy["Update tracked current PHY"]
    GapRSSI --> TelemetryPolicy["Update telemetry and adaptive policy"]

    GattsReg --> CreateTable["Set name and create attribute table"]
    GattsAttr --> StartService["Start service and configure advertising"]
    GattsConnect --> InitialRangeMode["Request max power and Coded S=8"]
    GattsDisconnect --> RestartAdvertising["Restart extended advertising"]
    GattsWrite --> Controls["Handle notifications or commands"]
```

