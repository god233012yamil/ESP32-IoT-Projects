# ESP32-S3 Digital Scale — Flowcharts

Complete system flowcharts for the ESP32-S3 Digital Scale project, written in
[Mermaid](https://mermaid.js.org/) syntax. GitHub renders these natively in
any Markdown file.

---

## Table of Contents

1. [System Boot Sequence](#1-system-boot-sequence)
2. [Wi-Fi Connection Manager](#2-wi-fi-connection-manager)
3. [HX711 Initialisation & Raw Read](#3-hx711-initialisation--raw-read)
4. [Calibration Flow](#4-calibration-flow)
5. [Measurement Task (FreeRTOS Loop)](#5-measurement-task-freertos-loop)
6. [Web Server & REST API Routing](#6-web-server--rest-api-routing)
7. [Server-Sent Events (SSE) Lifecycle](#7-server-sent-events-sse-lifecycle)
8. [NVS Calibration Persistence](#8-nvs-calibration-persistence)
9. [Full System Architecture](#9-full-system-architecture)

---

## 1. System Boot Sequence

`app_main()` orchestrates the entire startup in strict order before handing
control to the FreeRTOS scheduler.

```mermaid
flowchart TD
    A([Power On / Reset]) --> B[ESP-IDF Startup\nStack & heap init]
    B --> C[app_main]

    C --> D[nvs_flash_init]
    D --> E{NVS OK?}
    E -- No: truncated\nor new version --> F[nvs_flash_erase\nnvs_flash_init]
    F --> G
    E -- Yes --> G[wifi_manager_init\nTCP-IP stack · netif · driver]

    G --> H[wifi_manager_connect\nblocking until IP or fail]
    H --> I{Connected?}
    I -- No --> J[Log warning\nScale works offline\nno dashboard]
    I -- Yes --> K[Log IP address]
    J --> L
    K --> L[hx711_init\nConfigure GPIO 4 & 5]

    L --> M[calibration_load from NVS]
    M --> N{Saved data\nfound?}
    N -- Yes --> O[Apply saved\nscale factor & tare]
    N -- No --> P[Apply compile-time\nCONFIG_SCALE_FACTOR]
    O --> Q
    P --> Q[vTaskDelay 500 ms\nHX711 settle time]

    Q --> R[hx711_tare\nCapture zero offset\n20 samples]
    R --> S{Wi-Fi\nconnected?}
    S -- Yes --> T[web_server_start\nRegister URI handlers]
    S -- No --> U
    T --> U[xTaskCreate\ntask_measure]

    U --> V([FreeRTOS Scheduler\nRuns tasks concurrently])

    style A fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style V fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style J fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style F fill:#1e293b,stroke:#fbbf24,color:#f1f5f9
```

---

## 2. Wi-Fi Connection Manager

`wifi_manager_connect()` uses a FreeRTOS **EventGroup** to block the caller
until the station obtains an IP address or exhausts its retry budget.

```mermaid
flowchart TD
    A([wifi_manager_connect]) --> B[Create EventGroup\nWIFI_CONNECTED_BIT\nWIFI_FAIL_BIT]
    B --> C[Register event handlers\nWIFI_EVENT · IP_EVENT]
    C --> D[esp_wifi_set_mode STA\nesp_wifi_set_config\nesp_wifi_start]
    D --> E[WIFI_EVENT_STA_START\nfires automatically]
    E --> F[esp_wifi_connect]

    F --> G{Event received}

    G -- IP_EVENT_STA_GOT_IP --> H[Cache IP string\ns_connected = true\nSet WIFI_CONNECTED_BIT]
    G -- WIFI_EVENT_STA_DISCONNECTED --> I{retry_count <\nMAX_RETRIES?}
    I -- Yes --> J[retry_count++\nesp_wifi_connect]
    J --> G
    I -- No --> K[Set WIFI_FAIL_BIT]

    H --> L[xEventGroupWaitBits\nunblocks]
    K --> L

    L --> M{Which bit\nwas set?}
    M -- CONNECTED --> N[Unregister handlers\nDelete EventGroup\nreturn ESP_OK]
    M -- FAIL --> O[Unregister handlers\nDelete EventGroup\nreturn ESP_FAIL]

    N --> P([Caller continues])
    O --> P

    style A fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style P fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style K fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style O fill:#1e293b,stroke:#f87171,color:#f1f5f9
```

---

## 3. HX711 Initialisation & Raw Read

The driver configures GPIO then performs a time-critical bit-banged read
with interrupts disabled.

```mermaid
flowchart TD
    subgraph INIT ["hx711_init()"]
        A([Call hx711_init]) --> B{dev & cfg\nnot NULL?}
        B -- No --> C[return\nESP_ERR_INVALID_ARG]
        B -- Yes --> D[Copy config to dev\ntare=0 · scale=1.0]
        D --> E[gpio_config SCK\nOUTPUT · initial LOW]
        E --> F{GPIO OK?}
        F -- No --> G[return error]
        F -- Yes --> H[gpio_config DOUT\nINPUT · floating]
        H --> I{GPIO OK?}
        I -- No --> G
        I -- Yes --> J[return ESP_OK]
    end

    subgraph READ ["hx711_read_raw()"]
        K([Call hx711_read_raw]) --> L[hx711_wait_ready\npoll DOUT every 1 ms]
        L --> M{DOUT LOW\nwithin 1000 ms?}
        M -- No --> N[return ESP_ERR_TIMEOUT]
        M -- Yes --> O[portDISABLE_INTERRUPTS]
        O --> P["Clock 24 bits MSB-first\nSCK HIGH 1µs → sample DOUT\nSCK LOW 1µs → shift into data"]
        P --> Q["Send N gain-select pulses\n1=A/128 · 2=B/32 · 3=A/64"]
        Q --> R[portENABLE_INTERRUPTS]
        R --> S{Bit 23 set?\nsign bit check}
        S -- Yes --> T["data &#124;= 0xFF000000\nsign extend to int32"]
        S -- No --> U
        T --> U[return raw int32\nESP_OK]
    end

    style A fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style K fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style C fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style G fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style N fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style U fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style J fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style O fill:#1e293b,stroke:#fbbf24,color:#f1f5f9
    style R fill:#1e293b,stroke:#fbbf24,color:#f1f5f9
```

---

## 4. Calibration Flow

Two calibration paths exist: the **interactive UART wizard** (first-time setup)
and the **HTTP API** (remote / in-field updates). Both persist to NVS.

```mermaid
flowchart TD
    A([Calibration needed]) --> B{Method?}

    B -- UART Wizard --> C[calibration_run]
    B -- HTTP API --> D[POST /api/calibrate\nbody: JSON scale factor]
    B -- Compile default --> E[hx711_set_scale\nCONFIG_SCALE_FACTOR]

    subgraph WIZARD ["UART Wizard — calibration_run()"]
        C --> F[Print wizard header\nto UART console]
        F --> G[Prompt: clear platform\nPress ENTER]
        G --> H[hx711_tare\n20 samples → store offset]
        H --> I[Prompt: enter reference\nweight in grams]
        I --> J[Read float from stdin\nvalidate > 0]
        J --> K{Valid\nweight?}
        K -- No --> L[return\nESP_ERR_INVALID_ARG]
        K -- Yes --> M[Prompt: place weight\nPress ENTER]
        M --> N[hx711_read_average\n20 samples → avg_loaded]
        N --> O[delta = avg_loaded - tare]
        O --> P{delta == 0?}
        P -- Yes --> Q[return ESP_FAIL\nno change detected]
        P -- No --> R[scale = delta / ref_grams\nhx711_set_scale]
        R --> S[calibration_save to NVS]
        S --> T[Print success summary]
    end

    subgraph HTTP_CAL ["HTTP Calibration — handler_api_calibrate()"]
        D --> U[httpd_req_recv body]
        U --> V[cJSON_Parse body]
        V --> W{JSON valid &\nscale > 0?}
        W -- No --> X[HTTP 400\nBad Request]
        W -- Yes --> Y[hx711_set_scale\nnew factor]
        Y --> Z[HTTP 200\nstatus:ok]
    end

    E --> AA([Scale factor applied])
    T --> AA
    Z --> AA

    style A fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style AA fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style L fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style Q fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style X fill:#1e293b,stroke:#f87171,color:#f1f5f9
```

---

## 5. Measurement Task (FreeRTOS Loop)

`task_measure` runs at priority 5, waking every 500 ms via
`vTaskDelayUntil()` for deterministic timing.

```mermaid
flowchart TD
    A([xTaskCreate task_measure]) --> B[last_wake =\nxTaskGetTickCount]
    B --> C

    C --> D[hx711_get_weight\nCONFIG_SCALE_SAMPLES averages]
    D --> E{ESP_OK?}

    E -- No --> F[ESP_LOGW\nHX711 read error]
    F --> G

    E -- Yes --> H{abs grams <\nZERO_THRESHOLD_G?}
    H -- Yes --> I[grams = 0.0f\nclamp noise to zero]
    H -- No --> J
    I --> J[ESP_LOGI weight\nin g · kg · lb]
    J --> K[web_server_push_weight\ngrams]
    K --> L{SSE clients\nconnected?}
    L -- Yes --> M[lwip_send SSE frame\nto each fd]
    L -- No --> G
    M --> N{send\nsucceeded?}
    N -- No --> O[Remove dead client\nfrom fd array]
    N -- Yes --> G
    O --> G

    G[vTaskDelayUntil\n500 ms period] --> C

    style A fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style G fill:#1e293b,stroke:#818cf8,color:#f1f5f9
    style F fill:#1e293b,stroke:#fbbf24,color:#f1f5f9
    style O fill:#1e293b,stroke:#f87171,color:#f1f5f9
```

---

## 6. Web Server & REST API Routing

`web_server_start()` registers six URI handlers. Every incoming HTTP request
is dispatched by the esp_http_server task pool.

```mermaid
flowchart TD
    A([Incoming HTTP Request]) --> B[esp_http_server\ndispatches by method + URI]

    B --> C{URI match}

    C -- "GET /" --> D[handler_root\nSend DASHBOARD_HTML\nContent-Type: text/html]

    C -- "GET /api/weight" --> E[handler_api_weight\nRead s_last_weight_g\nJSON response]

    C -- "POST /api/tare" --> F[handler_api_tare\nhx711_tare 20 samples\nReset s_last_weight_g]
    F --> F1{HX711 OK?}
    F1 -- No --> F2[HTTP 500]
    F1 -- Yes --> F3[HTTP 200 status:ok]

    C -- "POST /api/calibrate" --> G[handler_api_calibrate\nhttpd_req_recv body]
    G --> G1[cJSON_Parse]
    G1 --> G2{scale field\nvalid?}
    G2 -- No --> G3[HTTP 400]
    G2 -- Yes --> G4[hx711_set_scale\nHTTP 200]

    C -- "GET /api/status" --> H[handler_api_status\nIP · uptime · heap\nscale · tare]

    C -- "GET /events" --> I[handler_sse\nSend SSE headers\nRegister socket fd\nBlock until disconnect]

    C -- No match --> J[HTTP 404]

    D --> K([Response sent])
    E --> K
    F2 --> K
    F3 --> K
    G3 --> K
    G4 --> K
    H --> K
    I --> K
    J --> K

    style A fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style K fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style F2 fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style G3 fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style J fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style I fill:#1e293b,stroke:#818cf8,color:#f1f5f9
```

---

## 7. Server-Sent Events (SSE) Lifecycle

SSE keeps a persistent HTTP connection open. The browser's `EventSource`
receives a `weight` event every 500 ms without polling.

```mermaid
sequenceDiagram
    participant B as Browser (EventSource)
    participant S as ESP32-S3 HTTP Server
    participant M as task_measure
    participant H as HX711

    B->>S: GET /events HTTP/1.1
    S->>B: 200 OK<br/>Content-Type: text/event-stream<br/>Connection: keep-alive
    S->>B: : connected (SSE comment / flush)
    S->>S: sse_add_client(fd)<br/>register socket in fd array

    loop Every 500 ms
        M->>H: hx711_get_weight()
        H-->>M: float grams
        M->>S: web_server_push_weight(grams)
        S->>B: event: weight<br/>data: {"weight_g": 123.45}
        B->>B: updateWeight(123.45)<br/>update chart + display
    end

    B->>S: TCP FIN (tab closed / navigate away)
    S->>S: lwip_recv returns 0<br/>sse_remove_client(fd)
    S->>S: handler_sse returns ESP_OK
```

---

## 8. NVS Calibration Persistence

Scale factor and tare offset survive power cycles by reading and writing to
ESP32 Non-Volatile Storage flash partition.

```mermaid
flowchart TD
    subgraph SAVE ["calibration_save()"]
        A([Call save]) --> B[nvs_open READWRITE\nnamespace: scale_cfg]
        B --> C{Open OK?}
        C -- No --> D[return NVS error]
        C -- Yes --> E[Bit-cast float scale\nto uint32_t]
        E --> F[nvs_set_u32\nkey: scale_factor]
        F --> G[nvs_set_i32\nkey: tare_offset]
        G --> H[nvs_commit]
        H --> I[nvs_close]
        I --> J[return ESP_OK]
    end

    subgraph LOAD ["calibration_load()"]
        K([Call load]) --> L[nvs_open READONLY\nnamespace: scale_cfg]
        L --> M{Namespace\nexists?}
        M -- No --> N[return\nESP_ERR_NVS_NOT_FOUND]
        M -- Yes --> O[nvs_get_u32\nkey: scale_factor]
        O --> P{Key found?}
        P -- Yes --> Q[Bit-cast uint32_t\nback to float\nhx711_set_scale]
        P -- No --> R[Skip — keep default]
        Q --> S[nvs_get_i32\nkey: tare_offset]
        R --> S
        S --> T{Key found?}
        T -- Yes --> U[dev->tare = value]
        T -- No --> V[Skip — tare stays 0]
        U --> W[nvs_close\nreturn ESP_OK]
        V --> W
    end

    subgraph ERASE ["calibration_erase()"]
        X([Call erase]) --> Y[nvs_open READWRITE]
        Y --> Z[nvs_erase_key scale_factor\nnvs_erase_key tare_offset]
        Z --> AA[nvs_commit · nvs_close]
        AA --> AB[return ESP_OK\nNext boot uses defaults]
    end

    style A fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style K fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style X fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style D fill:#1e293b,stroke:#f87171,color:#f1f5f9
    style N fill:#1e293b,stroke:#fbbf24,color:#f1f5f9
    style J fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style W fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style AB fill:#1e293b,stroke:#4ade80,color:#f1f5f9
```

---

## 9. Full System Architecture

End-to-end view of all hardware, firmware components, tasks, and the browser
dashboard communicating together.

```mermaid
flowchart LR
    subgraph HW ["Hardware"]
        LC1[Load Cell 1\n50 kg half-bridge]
        LC2[Load Cell 2\n50 kg half-bridge]
        LC3[Load Cell 3\n50 kg half-bridge]
        LC4[Load Cell 4\n50 kg half-bridge]
        HX[HX711 Module\n24-bit ADC\nGain 128 · Ch A]
        LC1 & LC2 & LC3 & LC4 -->|Full Wheatstone\nBridge E+/E-/A+/A-| HX
    end

    subgraph ESP ["ESP32-S3"]
        subgraph DRIVER ["HX711 Component"]
            DRV[hx711.c\nBit-bang driver\nIRAM-safe]
        end

        subgraph APP ["Main Application"]
            CAL[calibration.c\nUART wizard\nNVS save/load]
            MAIN[main.c\napp_main\nBoot sequence]
            TASK[task_measure\nFreeRTOS task\n500 ms period]
        end

        subgraph NET ["Network Layer"]
            WIFI[wifi_manager.c\nSTA · EventGroup\nRetry logic]
            WEB[web_server.c\nHTTP + REST\nSSE push]
        end

        NVS[(NVS Flash\nscale_factor\ntare_offset)]
    end

    subgraph CLIENT ["Browser"]
        DASH[Dashboard\nChart.js graph\nUnit toggle\nTare button\nSSE EventSource]
    end

    HX -->|GPIO 4 DOUT\nGPIO 5 SCK| DRV
    DRV --> TASK
    TASK -->|hx711_get_weight| DRV
    TASK -->|web_server_push_weight| WEB
    MAIN --> CAL
    MAIN --> WIFI
    MAIN --> WEB
    CAL <-->|read / write| NVS
    WIFI -->|STA connected\nIP assigned| WEB
    WEB -->|SSE event:weight\nevery 500 ms| DASH
    DASH -->|POST /api/tare\nPOST /api/calibrate\nGET /api/status| WEB
    WEB -->|hx711_tare\nhx711_set_scale| DRV

    style HW fill:#0f172a,stroke:#475569,color:#94a3b8
    style ESP fill:#0f172a,stroke:#38bdf8,color:#f1f5f9
    style CLIENT fill:#0f172a,stroke:#818cf8,color:#f1f5f9
    style DRIVER fill:#1e293b,stroke:#38bdf8,color:#f1f5f9
    style APP fill:#1e293b,stroke:#4ade80,color:#f1f5f9
    style NET fill:#1e293b,stroke:#818cf8,color:#f1f5f9
    style HX fill:#1e293b,stroke:#fbbf24,color:#f1f5f9
    style NVS fill:#1e293b,stroke:#fbbf24,color:#f1f5f9
    style DASH fill:#1e293b,stroke:#818cf8,color:#f1f5f9
```

---

*Generated for ESP32-S3 Digital Scale v1.0.0 — see [README.md](README.md) for full project documentation.*
