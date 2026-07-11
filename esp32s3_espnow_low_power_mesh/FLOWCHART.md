# Project Flowcharts

This document describes the firmware flow for the ESP-NOW low-power mesh project using Mermaid syntax. The diagrams mirror the implementation in `main/main.c`, `mesh_protocol.c`, and `power_manager.c`.

## System Topology

```mermaid
flowchart LR
    subgraph LeafLayer["Deep-sleep leaf nodes"]
        L1["Leaf 100<br/>wake, sample, send, sleep"]
        L2["Leaf 101<br/>wake, sample, send, sleep"]
    end

    subgraph RouterLayer["Always-awake routers"]
        R1["Router 10<br/>ACK child, dedupe, decrement TTL, forward"]
        R2["Router 11<br/>ACK child, dedupe, decrement TTL, forward"]
    end

    subgraph RootLayer["Always-awake root gateway"]
        G["Root 1<br/>broadcast beacons, ACK senders, log telemetry"]
    end

    L1 -->|"SENSOR packet"| R1
    L2 -->|"SENSOR packet"| R2
    R1 -->|"Forward SENSOR"| G
    R2 -->|"Forward SENSOR"| G
    G -.->|"Broadcast BEACON"| L1
    G -.->|"Broadcast BEACON"| L2
    R1 -->|"ACK previous hop"| L1
    R2 -->|"ACK previous hop"| L2
    G -->|"ACK previous hop"| R1
    G -->|"ACK previous hop"| R2
```

## Application Startup

```mermaid
flowchart TD
    A["app_main()"] --> B["Increment RTC boot counter"]
    B --> C["initialize_nvs()"]
    C --> D["Create RX queue"]
    D --> E["Create event group"]
    E --> F{"Queue and event group created?"}
    F -->|"No"| G["Log allocation failure and abort"]
    F -->|"Yes"| H["initialize_wifi()"]
    H --> I["Initialize esp_netif and default event loop"]
    I --> J["Initialize Wi-Fi in station mode"]
    J --> K["Start Wi-Fi"]
    K --> L["Set fixed ESP-NOW channel"]
    L --> M["initialize_espnow()"]
    M --> N["esp_now_init()"]
    N --> O["Register send callback"]
    O --> P["Register receive callback"]
    P --> Q{"Encryption enabled?"}
    Q -->|"Yes"| R["Set ESP-NOW PMK"]
    Q -->|"No"| S["Skip PMK setup"]
    R --> T["Add broadcast peer for beacons"]
    S --> T
    T --> U{"Node is root?"}
    U -->|"No"| V["Parse configured parent MAC"]
    V --> W["Add parent peer"]
    U -->|"Yes"| X["No parent peer needed"]
    W --> Y["Log role, node ID, MAC, channel, boot count, wake cause"]
    X --> Y
    Y --> Z{"Configured role"}
    Z -->|"Root"| ZA["Create root_beacon_task() and receiver_task()"]
    Z -->|"Router"| ZB["Create receiver_task()"]
    Z -->|"Leaf"| ZC["Run leaf wake/transmit/sleep cycle"]
```

## Role Runtime Flow

```mermaid
flowchart TD
    A{"Node role"} -->|"Root"| B["root_beacon_task()"]
    A -->|"Root or Router"| C["receiver_task()"]
    A -->|"Leaf"| D["run_leaf_cycle()"]

    B --> B1["Build BEACON packet"]
    B1 --> B2["Finalize packet CRC"]
    B2 --> B3["Send to broadcast MAC"]
    B3 --> B4["Delay APP_BEACON_INTERVAL_MS"]
    B4 --> B

    C --> C1["Wait for RX_EVENT_PACKET"]
    C1 --> C2["drain_receive_queue()"]
    C2 --> C3["process_received_packet()"]
    C3 --> C1

    D --> D1["Wait up to APP_BEACON_WAIT_MS"]
    D1 --> D2["Drain received packets while waiting"]
    D2 --> D3{"Beacon received or wait expired?"}
    D3 --> D4["transmit_sensor_packet_with_retry()"]
    D4 --> D5["esp_now_deinit()"]
    D5 --> D6["esp_wifi_stop()"]
    D6 --> D7["power_manager_enter_deep_sleep()"]
```

## ESP-NOW Receive Callback

```mermaid
flowchart TD
    A["espnow_receive_callback(info, data, length)"] --> B{"info, data valid and length == sizeof(mesh_packet_t)?"}
    B -->|"No"| C["Return"]
    B -->|"Yes"| D["mesh_packet_validate()"]
    D --> E{"Version, type, and CRC valid?"}
    E -->|"No"| C
    E -->|"Yes"| F["Copy source MAC and packet into rx_item_t"]
    F --> G["xQueueSend(s_rx_queue, item, 0)"]
    G --> H{"Queued?"}
    H -->|"No"| C
    H -->|"Yes"| I["Set RX_EVENT_PACKET"]
    I --> C
```

## Packet Processing

```mermaid
flowchart TD
    A["process_received_packet(item)"] --> B{"Packet type"}

    B -->|"BEACON"| C["Set RX_EVENT_BEACON"]
    C --> Z["Return"]

    B -->|"ACK"| D["Store acknowledged sequence"]
    D --> E["Set RX_EVENT_ACK"]
    E --> Z

    B -->|"Not SENSOR"| Z

    B -->|"SENSOR"| F["Send ACK to immediate sender"]
    F --> G{"Duplicate source ID and sequence?"}
    G -->|"Yes"| H["Log duplicate at debug level"]
    H --> Z

    G -->|"No"| I{"Current role"}
    I -->|"Root"| J["Log telemetry payload"]
    J --> Z

    I -->|"Router"| K{"TTL <= 1?"}
    K -->|"Yes"| L["Log TTL expired and drop"]
    L --> Z
    K -->|"No"| M["Copy packet and decrement TTL"]
    M --> N["Forward packet to configured parent MAC"]
    N --> O["Log forwarded packet"]
    O --> Z

    I -->|"Leaf"| P["Ignore sensor packet"]
    P --> Z
```

## Leaf Transmit, Retry, and ACK Flow

```mermaid
flowchart TD
    A["transmit_sensor_packet_with_retry()"] --> B["Build SENSOR packet"]
    B --> C["Increment RTC-retained sequence"]
    C --> D["populate_demo_sensor_data()"]
    D --> E["attempt = 0"]
    E --> F{"attempt <= APP_MAX_RETRIES?"}
    F -->|"No"| G["Return ESP_ERR_TIMEOUT"]
    F -->|"Yes"| H["Clear RX_EVENT_ACK and last ACK sequence"]
    H --> I["Finalize CRC and send packet to parent"]
    I --> J{"Send accepted by ESP-NOW?"}
    J -->|"No"| K["Return send error"]
    J -->|"Yes"| L["Wait up to APP_ACK_TIMEOUT_MS"]
    L --> M{"RX event while waiting?"}
    M -->|"Packet event"| N["Drain receive queue"]
    N --> O{"ACK event with matching sequence?"}
    M -->|"ACK event"| O
    M -->|"Timeout slice"| P{"Overall ACK timeout expired?"}
    O -->|"Yes"| Q["Return ESP_OK"]
    O -->|"No"| P
    P -->|"No"| L
    P -->|"Yes"| R["Delay small retry backoff"]
    R --> S["attempt++"]
    S --> F
```

## Deep-Sleep Entry Flow

```mermaid
flowchart TD
    A["run_leaf_cycle() complete"] --> B["esp_now_deinit()"]
    B --> C["esp_wifi_stop()"]
    C --> D["power_manager_enter_deep_sleep(APP_WAKE_INTERVAL_SEC)"]
    D --> E{"sleep_seconds == 0?"}
    E -->|"Yes"| F["Return ESP_ERR_INVALID_ARG"]
    E -->|"No"| G["Convert seconds to microseconds"]
    G --> H["esp_sleep_enable_timer_wakeup()"]
    H --> I{"Timer wake configured?"}
    I -->|"No"| J["Return ESP-IDF error"]
    I -->|"Yes"| K["Log sleep interval"]
    K --> L["fflush(stdout)"]
    L --> M["esp_deep_sleep_start()"]
    M --> N["CPU powers down until RTC timer wake"]
```

## Protocol Validation Flow

```mermaid
flowchart TD
    A["mesh_packet_validate(data, length)"] --> B{"data != NULL and length is exact packet size?"}
    B -->|"No"| C["Return ESP_ERR_INVALID_SIZE"]
    B -->|"Yes"| D["Copy bytes into mesh_packet_t"]
    D --> E{"version == MESH_PROTOCOL_VERSION?"}
    E -->|"No"| F["Return ESP_ERR_INVALID_VERSION"]
    E -->|"Yes"| G{"type is BEACON, SENSOR, or ACK?"}
    G -->|"No"| H["Return ESP_ERR_INVALID_ARG"]
    G -->|"Yes"| I["Store received CRC"]
    I --> J["Zero payload_crc field"]
    J --> K["Calculate CRC-16/CCITT-FALSE"]
    K --> L{"Calculated CRC matches received CRC?"}
    L -->|"No"| M["Return ESP_ERR_INVALID_CRC"]
    L -->|"Yes"| N["Return ESP_OK"]
```
