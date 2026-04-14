# Building a Networked Digital Scale with ESP32-S3, HX711, and a Live Web Dashboard

*A deep-dive into load-cell physics, 24-bit ADC communication, real-time SSE streaming, and calibration mathematics — using ESP-IDF v5.x.*

---

## Introduction

Weight measurement is one of the oldest engineering problems, yet it remains surprisingly nuanced in the embedded domain. Strain gauges measure deformations on the order of microns; the signals they produce are in the millivolt range; and the firmware that reads them must operate under hard real-time constraints while simultaneously serving a live browser dashboard over Wi-Fi.

This article walks through a complete implementation: four 50 kg half-bridge load cells wired as a full Wheatstone bridge, an HX711 24-bit ADC amplifier, and an ESP32-S3 running ESP-IDF. The finished system publishes weight data via a REST API and a Server-Sent Events (SSE) stream, with a single-page dashboard served directly from the microcontroller's flash — no cloud service, no external server, no additional hardware.

By the end you will understand:

- Why four half-bridge cells must be wired as a full bridge, and what you lose if you don't.
- How the HX711's custom serial protocol works at the signal level, and why interrupt disabling is non-negotiable.
- The mathematics of tare and calibration, and how to persist them to NVS.
- How to implement an SSE push server on a microcontroller without a file system.
- The specific lwIP socket API surface available inside `esp_http_server` handler tasks.

The full project is available on GitHub. All code excerpts below are taken directly from it.

---

## 1. Hardware Architecture

### 1.1 Load Cell Physics

A half-bridge load cell contains two strain-gauge resistors bonded to a spring element — one in a tensile position, one in a compressive position. Under load, the tensile gauge increases resistance by ΔR while the compressive gauge decreases by ΔR. Relative to a fixed excitation voltage V_EXC, the differential output is:

```
V_out = (V_EXC / 2) × (ΔR / R₀)
```

A single 50 kg half-bridge cell with a 2 mV/V sensitivity at full scale, driven by 3.3 V, produces a maximum signal of just 3.3 mV — well below the noise floor of any general-purpose ADC on the ESP32-S3. This is why a dedicated front-end amplifier is mandatory.

### 1.2 Full Wheatstone Bridge from Four Half-Bridge Cells

Combining four half-bridge cells into a full Wheatstone bridge doubles the output signal and dramatically improves common-mode noise rejection:

```
V_out = V_EXC × (ΔR / R₀)
```

The wiring rule is mechanical as well as electrical. In a typical body-scale frame, the four cells sit at the corners of a rectangular platform. Cells on one diagonal are in tension under a centred load; cells on the other diagonal are in compression. Wire them accordingly:

```
         E+ ──────────┬─────────────────┬─────────── E+
                      │                 │
              Cell 1  │  (tension)      │  Cell 2
              R + ΔR  │                 │  R + ΔR
                      │                 │
         A+ ──────────┤                 ├─────────── A+
                      │                 │
              Cell 3  │  (compression)  │  Cell 4
              R − ΔR  │                 │  R − ΔR
                      │                 │
         A- ──────────┴─────────────────┴─────────── A-
                      │
         E- ──────────┘
```

The practical connection to the HX711 module:

| HX711 Pin | Wire colour (typical) | Load cell wires |
|-----------|----------------------|-----------------|
| E+ | Red | Cells 1 & 3 red wires joined |
| E− | Black | Cells 1 & 3 black wires joined |
| A+ | White | Cells 2 & 4 white wires joined |
| A− | Green/Yellow | Cells 2 & 4 green/yellow wires joined |

> **Verification procedure:** With no load applied, measure resistance between each wire pair. The excitation pair (E+/E−) reads the bridge's nominal resistance (~380 Ω for 350 Ω cells in parallel). If you see open circuit on any pair, a cell has a broken gauge.

### 1.3 HX711 to ESP32-S3 Wiring

```
ESP32-S3           HX711 Module
─────────          ────────────
GPIO 4   ────────► DOUT  (data output)
GPIO 5   ────────► SCK   (serial clock / PD_SCK)
3V3      ────────► VCC
GND      ────────► GND
                   E+  ────► bridge E+
                   E−  ────► bridge E−
                   A+  ────► bridge A+
                   A−  ────► bridge A−
```

The HX711 module's on-board LDO regulator drives AVDD from VCC, so the bridge excitation voltage follows the supply: at 3.3 V, full-scale differential input to channel A at gain 128 is ±20 mV. This is comfortable for a 50 kg cell with 2 mV/V sensitivity driven at 3.3 V (6.6 mV full scale).

---

## 2. Project Structure

```
esp32s3_scale/
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── components/
│   └── hx711/                 ← reusable ESP-IDF component
│       ├── CMakeLists.txt
│       ├── hx711.c
│       └── include/hx711.h
└── main/
    ├── CMakeLists.txt
    ├── scale_config.h         ← all compile-time tunables
    ├── main.c                 ← app_main + measurement task
    ├── calibration.c / .h     ← NVS wizard
    ├── wifi_manager.c / .h    ← STA connection
    └── web_server.c / .h      ← HTTP + SSE + dashboard HTML
```

The HX711 driver lives in `components/hx711/` as a proper ESP-IDF component with its own `CMakeLists.txt`, making it reusable across projects with a single `REQUIRES hx711` declaration.

---

## 3. The HX711 Driver

### 3.1 Data Structures

```c
// components/hx711/include/hx711.h

typedef enum {
    HX711_GAIN_A_128 = 1,  // Channel A, gain 128 (1 extra SCK pulse)
    HX711_GAIN_B_32  = 2,  // Channel B, gain 32  (2 extra SCK pulses)
    HX711_GAIN_A_64  = 3,  // Channel A, gain 64  (3 extra SCK pulses)
} hx711_gain_t;

typedef struct {
    gpio_num_t   dout_pin;
    gpio_num_t   sck_pin;
    hx711_gain_t gain;
} hx711_config_t;

typedef struct {
    hx711_config_t cfg;   // hardware configuration
    int32_t        tare;  // raw ADC offset at zero weight
    float          scale; // raw counts per gram
} hx711_dev_t;
```

The device handle owns both hardware config and calibration state. This keeps all scale state in one place and makes it straightforward to pass a single pointer between tasks.

### 3.2 GPIO Initialisation

```c
// components/hx711/hx711.c

esp_err_t hx711_init(hx711_dev_t *dev, const hx711_config_t *cfg)
{
    if (!dev || !cfg) return ESP_ERR_INVALID_ARG;

    memcpy(&dev->cfg, cfg, sizeof(hx711_config_t));
    dev->tare  = 0;
    dev->scale = 1.0f;

    gpio_config_t sck_conf = {
        .pin_bit_mask = (1ULL << cfg->sck_pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sck_conf), TAG, "SCK config failed");
    gpio_set_level(cfg->sck_pin, 0);  // ensure SCK is LOW — HIGH > 60 µs = power-down

    gpio_config_t dout_conf = {
        .pin_bit_mask = (1ULL << cfg->dout_pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,   // HX711 drives DOUT actively; no pull needed
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    return gpio_config(&dout_conf);
}
```

Three details worth noting. First, `scale = 1.0f` at init means `hx711_get_weight()` returns raw counts before calibration — useful for the initial calibration measurement itself. Second, SCK is explicitly driven LOW after configuration; if it were floating HIGH on boot, the HX711 would enter power-down mode immediately. Third, both pins use `GPIO_INTR_DISABLE` — we do not use GPIO interrupts because the HX711's DOUT line changes state asynchronously relative to the FreeRTOS scheduler, and the bit-bang read sequence is far simpler with polling.

### 3.3 The Serial Protocol and Why It Cannot Be Interrupted

The HX711 uses a proprietary two-wire synchronous protocol that is neither SPI nor I2C. The master controls SCK; the slave drives DOUT. After a conversion completes, the HX711 pulls DOUT LOW. The master then clocks out 24 data bits MSB-first by toggling SCK, reading DOUT after each falling edge. After the 24th bit, the master sends 1, 2, or 3 additional pulses to select the gain for the next conversion.

```
                  ┌─────┐ ┌─────┐ ┌─────┐
SCK  ─────────────┘     └─┘     └─┘     └──  ... (×24, then gain pulses)

DOUT ───┐ ╔═════╗ ╔═════╗ ╔═════╗
        └─║ B23 ║─║ B22 ║─║ B21 ║─ ... (MSB first, sampled on falling edge)
          ╚═════╝ ╚═════╝ ╚═════╝

Timing: T1 (SCK HIGH) ≥ 0.2 µs, ≤ 50 µs
        T2 (SCK LOW)  ≥ 0.2 µs
        T3 (DOUT valid after SCK falls) ≤ 0.1 µs
```

The critical constraint: if SCK is held HIGH for longer than 60 µs the HX711 enters power-down mode, aborting the transfer. On an ESP32-S3 running at 240 MHz, a FreeRTOS task switch, an ISR, or a flash cache miss can easily consume hundreds of microseconds. This is why the entire read sequence runs with interrupts disabled:

```c
// components/hx711/hx711.c

esp_err_t hx711_read_raw(hx711_dev_t *dev, int32_t *raw)
{
    esp_err_t err = hx711_wait_ready(dev);
    if (err != ESP_OK) return err;

    portDISABLE_INTERRUPTS();  // ← critical section begins

    uint32_t data = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(dev->cfg.sck_pin, 1);
        ets_delay_us(1);                                          // T1: SCK HIGH ≥ 0.2 µs
        data = (data << 1) | (uint32_t)gpio_get_level(dev->cfg.dout_pin);
        gpio_set_level(dev->cfg.sck_pin, 0);
        ets_delay_us(1);                                          // T2: SCK LOW ≥ 0.2 µs
    }

    // Gain-select pulses: 1=A/128, 2=B/32, 3=A/64
    for (int i = 0; i < (int)dev->cfg.gain; i++) {
        hx711_clock_pulse(dev);
    }

    portENABLE_INTERRUPTS();  // ← critical section ends

    // Sign-extend 24-bit two's complement to int32_t
    if (data & 0x800000U) data |= 0xFF000000U;
    *raw = (int32_t)data;

    return ESP_OK;
}
```

`ets_delay_us()` is a ROM-based busy-wait loop that provides microsecond accuracy without a context switch. `vTaskDelay(pdMS_TO_TICKS(1))` has a minimum resolution of one tick period (1 ms at 1000 Hz) — far too coarse for 1 µs timing.

The sign extension deserves a moment. The HX711 outputs a 24-bit two's complement value. After accumulating bits into a `uint32_t`, bit 23 is the sign bit. Without the sign extension step, a reading of −1 would arrive as `0x00FFFFFF` (16 777 215 decimal) rather than `0xFFFFFFFF` (−1 as int32_t), completely breaking the arithmetic in `hx711_get_weight()`.

### 3.4 IRAM Placement

The single-pulse helper is placed in IRAM:

```c
static IRAM_ATTR int hx711_clock_pulse(const hx711_dev_t *dev)
{
    gpio_set_level(dev->cfg.sck_pin, 1);
    ets_delay_us(1);
    gpio_set_level(dev->cfg.sck_pin, 0);
    ets_delay_us(1);
    return gpio_get_level(dev->cfg.dout_pin);
}
```

On the ESP32-S3 the instruction cache services flash reads. A cache miss takes 20–100 µs — enough to abort the HX711 transfer. `IRAM_ATTR` places the function's machine code in internal SRAM, which has zero-wait-state access regardless of flash activity. The main `hx711_read_raw()` body is also critical; marking the helper `IRAM_ATTR` is the minimal necessary change since `hx711_read_raw` is already in IRAM by virtue of calling an IRAM function during the interrupt-disabled window.

### 3.5 Noise Reduction via Averaging

```c
esp_err_t hx711_read_average(hx711_dev_t *dev, uint8_t samples, int32_t *avg)
{
    if (samples == 0 || samples > HX711_MAX_SAMPLES) return ESP_ERR_INVALID_ARG;

    int64_t sum   = 0;
    int     count = 0;

    for (uint8_t i = 0; i < samples; i++) {
        int32_t raw = 0;
        if (hx711_read_raw(dev, &raw) == ESP_OK) {
            sum += raw;   // int64_t prevents overflow: 64 × 2^23 < 2^63
            count++;
        }
    }

    if (count == 0) return ESP_FAIL;

    *avg = (int32_t)(sum / count);
    return ESP_OK;
}
```

Averaging N samples reduces white noise by √N. With 10 samples the noise floor drops by a factor of ~3.16. The accumulator is `int64_t` deliberately: `HX711_MAX_SAMPLES` (64) multiplied by the maximum raw value (8 388 607) is 536 870 848, which fits in int32_t but is close enough to the boundary that a defensive int64_t costs nothing and prevents subtle overflow bugs if someone raises the sample limit.

---

## 4. Calibration

### 4.1 The Mathematics

The HX711 outputs dimensionless counts. Mapping them to grams requires two parameters:

**Tare** — the raw reading with nothing on the scale, capturing the platform's own weight, any zero-point offset of the bridge, and amplifier input bias:

```
tare = average_raw(empty_platform, N)
```

**Scale factor** — the ratio of count change to mass change, measured with a known reference weight W_ref:

```
raw_loaded = average_raw(W_ref, N)
scale      = (raw_loaded − tare) / W_ref    [units: counts per gram]
```

At runtime:

```
weight_g = (raw_reading − tare) / scale
```

This is a single-point linear calibration. It assumes the load cell is linear (which all quality strain-gauge cells are, to well within 0.05% over the rated range) and that the single reference weight was placed at the effective centre of the platform.

### 4.2 The UART Calibration Wizard

```c
// main/calibration.c

esp_err_t calibration_run(hx711_dev_t *dev)
{
    char buf[32];

    // Step 1: Tare with empty platform
    printf("[1/3] Remove ALL weight from the platform.\n");
    printf("      Press ENTER when ready...\n");
    read_line(buf, sizeof(buf));

    esp_err_t err = hx711_tare(dev, CONFIG_TARE_SAMPLES);
    if (err != ESP_OK) return err;
    printf("      Tare: %" PRId32 " raw counts\n\n", dev->tare);

    // Step 2: Measure known reference weight
    printf("[2/3] Enter reference weight in grams: ");
    fflush(stdout);
    read_line(buf, sizeof(buf));

    float ref_grams = 0.0f;
    if (sscanf(buf, "%f", &ref_grams) != 1 || ref_grams <= 0.0f)
        return ESP_ERR_INVALID_ARG;

    printf("      Place %.1f g on the platform, press ENTER...\n", ref_grams);
    read_line(buf, sizeof(buf));

    int32_t avg_loaded = 0;
    err = hx711_read_average(dev, CONFIG_TARE_SAMPLES, &avg_loaded);
    if (err != ESP_OK) return err;

    int32_t delta     = avg_loaded - dev->tare;
    float   new_scale = (float)delta / ref_grams;
    hx711_set_scale(dev, new_scale);

    printf("      delta=%"PRId32"  scale=%.4f counts/g\n\n", delta, new_scale);

    // Step 3: Persist to NVS
    return calibration_save(dev);
}
```

### 4.3 NVS Persistence

Floating-point values cannot be stored directly in NVS (which has no float key type), so the scale factor is bit-cast to `uint32_t` before writing:

```c
// main/calibration.c

esp_err_t calibration_save(const hx711_dev_t *dev)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(
        nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs),
        TAG, "nvs_open failed"
    );

    // Bit-cast float → uint32_t — safe as long as sizeof(float) == sizeof(uint32_t)
    uint32_t scale_bits;
    memcpy(&scale_bits, &dev->scale, sizeof(float));
    nvs_set_u32(nvs, CONFIG_NVS_KEY_SCALE, scale_bits);
    nvs_set_i32(nvs, CONFIG_NVS_KEY_TARE,  dev->tare);

    esp_err_t err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

esp_err_t calibration_load(hx711_dev_t *dev)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_ERR_NVS_NOT_FOUND;
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_open failed");

    uint32_t scale_bits = 0;
    if (nvs_get_u32(nvs, CONFIG_NVS_KEY_SCALE, &scale_bits) == ESP_OK) {
        float scale;
        memcpy(&scale, &scale_bits, sizeof(float));
        hx711_set_scale(dev, scale);
    }

    int32_t tare = 0;
    if (nvs_get_i32(nvs, CONFIG_NVS_KEY_TARE, &tare) == ESP_OK)
        dev->tare = tare;

    nvs_close(nvs);
    return ESP_OK;
}
```

Using `memcpy` for the bit-cast rather than a union or a pointer cast is the only strictly conforming approach in C — the others invoke undefined behaviour under strict aliasing rules even though they produce correct machine code on GCC/Xtensa with the default optimisation level.

---

## 5. Wi-Fi Connection with EventGroup Synchronisation

The Wi-Fi manager uses a FreeRTOS EventGroup to turn the asynchronous connection state machine into a blocking call, which simplifies `app_main()` considerably:

```c
// main/wifi_manager.c

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int                s_retry_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count++ < CONFIG_WIFI_MAX_RETRIES) {
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }

    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&e->ip_info.ip));
        s_connected   = true;
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_connect(void)
{
    s_wifi_event_group = xEventGroupCreate();

    // ... (register handlers, configure, start) ...

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,        // don't clear on exit
        pdFALSE,        // wait for any bit, not all
        portMAX_DELAY
    );

    vEventGroupDelete(s_wifi_event_group);

    return (bits & WIFI_CONNECTED_BIT) ? ESP_OK : ESP_FAIL;
}
```

The retry counter uses a pre-increment (`s_retry_count++`) so the first disconnect attempt always triggers a reconnect before checking the limit. This is intentional: the first disconnection often happens during the initial DHCP negotiation on congested networks.

---

## 6. HTTP Server and REST API

### 6.1 Server Configuration and URI Registration

```c
// main/web_server.c

esp_err_t web_server_start(hx711_dev_t *dev)
{
    s_dev          = dev;
    s_weight_mutex = xSemaphoreCreateMutex();
    s_sse_mutex    = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) s_sse_fds[i] = -1;

    httpd_config_t cfg   = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = CONFIG_WEBSERVER_PORT;
    // Each SSE connection occupies a socket for its entire lifetime.
    // Bump max_open_sockets to accommodate both REST and SSE clients.
    cfg.max_open_sockets = CONFIG_WEBSERVER_MAX_SOCKETS + MAX_SSE_CLIENTS;
    cfg.lru_purge_enable = true;  // reclaim stale sockets automatically

    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &cfg), TAG, "httpd_start failed");

    static const httpd_uri_t uris[] = {
        { .uri = "/",              .method = HTTP_GET,  .handler = handler_root        },
        { .uri = "/api/weight",    .method = HTTP_GET,  .handler = handler_api_weight  },
        { .uri = "/api/tare",      .method = HTTP_POST, .handler = handler_api_tare    },
        { .uri = "/api/calibrate", .method = HTTP_POST, .handler = handler_api_calibrate },
        { .uri = "/api/status",    .method = HTTP_GET,  .handler = handler_api_status  },
        { .uri = "/events",        .method = HTTP_GET,  .handler = handler_sse         },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++)
        httpd_register_uri_handler(s_server, &uris[i]);

    return ESP_OK;
}
```

`lru_purge_enable` is important in practice. HTTP/1.1 keep-alive connections from browsers that close a tab without sending a FIN will eventually consume all socket slots. The LRU purge policy evicts the least-recently-used connection when the server is at capacity.

### 6.2 JSON Request Parsing with cJSON

The calibrate endpoint accepts a JSON body. Using cJSON (bundled with ESP-IDF) keeps the parsing safe and readable:

```c
// main/web_server.c

static esp_err_t handler_api_calibrate(httpd_req_t *req)
{
    char body[64] = {0};
    int  len      = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }

    cJSON *json = cJSON_Parse(body);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *scale_item = cJSON_GetObjectItem(json, "scale");
    if (!cJSON_IsNumber(scale_item) || scale_item->valuedouble <= 0.0) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing/invalid 'scale'");
        return ESP_FAIL;
    }

    float new_scale = (float)scale_item->valuedouble;
    cJSON_Delete(json);  // always free before returning

    hx711_set_scale(s_dev, new_scale);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", 14);
    return ESP_OK;
}
```

Two things to enforce in every cJSON handler: always call `cJSON_Delete(json)` on all exit paths (including error paths), and always validate the type with `cJSON_IsNumber()` before accessing the value. Type confusion bugs in embedded JSON parsers tend to produce silent wrong values rather than crashes.

---

## 7. Server-Sent Events: Real-Time Data Without Polling

### 7.1 Why SSE Instead of WebSockets

Both SSE and WebSockets deliver server-initiated data. SSE is the better choice here for three reasons:

1. **Protocol simplicity.** SSE is plain HTTP/1.1 with a specific content type. No handshake upgrade, no frame parsing, no masking. The server side is a few dozen lines of C.
2. **Automatic reconnection.** The browser's `EventSource` API reconnects automatically with exponential backoff if the connection drops. WebSocket reconnection must be implemented manually.
3. **Firewall compatibility.** SSE is unidirectional (server → client) over a normal HTTP connection. Many corporate and home router firewalls that block WebSocket upgrades pass SSE connections cleanly.

The tradeoff: SSE is text-only and unidirectional. Since the dashboard only needs to receive weight updates, this is a non-issue.

### 7.2 SSE Handler Implementation

The SSE handler is a blocking URI handler that holds its HTTP connection open indefinitely and relies on `web_server_push_weight()` to inject data directly onto the socket:

```c
// main/web_server.c

static esp_err_t handler_sse(httpd_req_t *req)
{
    // 1. Send SSE-required headers. text/event-stream with no-cache is mandatory.
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection",    "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // 2. Flush headers with an SSE comment line (lines starting with ':' are ignored by clients)
    const char *init = ": connected\n\n";
    httpd_resp_send_chunk(req, init, strlen(init));

    // 3. Register this socket in the push-broadcast array
    int fd = httpd_req_to_sockfd(req);
    sse_add_client(fd);

    // 4. Block until the client disconnects.
    //    Poll every 200 ms using lwIP select() + recv() with MSG_PEEK.
    //    MSG_PEEK reads without consuming bytes, so a return of 0 means TCP EOF.
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(200));

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = {0, 0};

        if (select(fd + 1, &rfds, NULL, NULL, &tv) > 0) {
            char tmp[4];
            if (lwip_recv(fd, tmp, sizeof(tmp), MSG_PEEK | MSG_DONTWAIT) == 0)
                break;  // TCP FIN received — client closed the connection
        }
    }

    sse_remove_client(fd);
    return ESP_OK;
}
```

`httpd_req_to_sockfd()` exposes the underlying lwIP socket descriptor so we can call `lwip_recv()` and `lwip_send()` directly. These explicit lwIP API calls — rather than the POSIX-compatible macros `recv()` and `send()` — are used deliberately to avoid depending on the `LWIP_COMPAT_SOCKETS` SDK config option, which may or may not be enabled in a given build environment.

### 7.3 Broadcasting to Connected Clients

```c
// main/web_server.c

esp_err_t web_server_push_weight(float weight_g)
{
    // Update the cached value for the REST /api/weight endpoint
    xSemaphoreTake(s_weight_mutex, portMAX_DELAY);
    s_last_weight_g = weight_g;
    xSemaphoreGive(s_weight_mutex);

    if (s_sse_count == 0) return ESP_ERR_NOT_FOUND;

    // SSE wire format:
    //   event: <name>\n
    //   data:  <payload>\n
    //   \n                  ← blank line terminates the event
    char msg[80];
    int  len = snprintf(msg, sizeof(msg),
                        "event: weight\ndata: {\"weight_g\":%.2f}\n\n",
                        weight_g);

    xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (s_sse_fds[i] < 0) continue;
        if (lwip_send(s_sse_fds[i], msg, len, MSG_DONTWAIT) < 0) {
            // Socket is dead. Mark it free; handler_sse() will call
            // sse_remove_client() when it unblocks from select().
            s_sse_fds[i] = -1;
            s_sse_count--;
        }
    }
    xSemaphoreGive(s_sse_mutex);

    return ESP_OK;
}
```

`MSG_DONTWAIT` is important here. This function is called from `task_measure`, not from the HTTP server's task pool. A blocking `send()` to a slow or stalled client would delay the measurement loop for all other clients. With `MSG_DONTWAIT`, a full send buffer causes an immediate `EAGAIN` error, which is treated identically to a dead socket: the client is removed.

### 7.4 Browser-Side EventSource

On the dashboard, the JavaScript side is equally concise:

```javascript
function connectSSE() {
    const src = new EventSource('/events');

    src.onopen = () => {
        document.getElementById('dot').classList.add('online');
        document.getElementById('status-label').textContent = 'Connected';
    };

    src.onerror = () => {
        document.getElementById('dot').classList.remove('online');
        document.getElementById('status-label').textContent = 'Reconnecting…';
        // EventSource reconnects automatically — no manual retry needed
    };

    src.addEventListener('weight', e => {
        const data = JSON.parse(e.data);
        updateWeight(parseFloat(data.weight_g));
    });
}
```

`addEventListener('weight', ...)` listens for the named `weight` event we defined on the server side. Using named events rather than the default `message` event makes it straightforward to add other event types (e.g., `status`, `alert`) without branching on the payload.

---

## 8. The Measurement Task

```c
// main/main.c

static void task_measure(void *pvParam)
{
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        float grams = 0.0f;
        esp_err_t err = hx711_get_weight(&s_hx711, CONFIG_SCALE_SAMPLES, &grams);

        if (err == ESP_OK) {
            // Zero-clamp: suppress sub-2 g fluctuations around tare
            if (grams > -CONFIG_ZERO_THRESHOLD_G && grams < CONFIG_ZERO_THRESHOLD_G)
                grams = 0.0f;

            ESP_LOGI(TAG, "Weight: %.2f g  (%.3f kg / %.3f lb)",
                     grams, grams / 1000.0f, grams / 453.592f);

            web_server_push_weight(grams);
        } else {
            ESP_LOGW(TAG, "HX711 error: %s", esp_err_to_name(err));
        }

        // vTaskDelayUntil gives deterministic period regardless of loop body duration
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_MEASURE_INTERVAL_MS));
    }
}
```

`vTaskDelayUntil()` is preferred over `vTaskDelay()`. With `vTaskDelay()`, if the HX711 read takes 60 ms (10 samples × 10 Hz output rate), the actual loop period would be `60 ms + 500 ms = 560 ms`. `vTaskDelayUntil()` accounts for the time already elapsed in the loop body, keeping the period exactly at `CONFIG_MEASURE_INTERVAL_MS`.

---

## 9. The Embedded Web Dashboard

The dashboard is stored as a C string literal directly in flash — no SPIFFS, no LittleFS, no file system required. This eliminates an entire partition, reduces flash wear, and simplifies the build. The tradeoff is that updating the dashboard requires reflashing, which is acceptable for a production instrument.

The handler is trivial:

```c
static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, DASHBOARD_HTML, strlen(DASHBOARD_HTML));
    return ESP_OK;
}
```

`DASHBOARD_HTML` is a string literal defined earlier in the file. Chart.js is loaded from a CDN — the only external dependency — which means the dashboard works offline as long as the browser has cached the Chart.js bundle from a prior visit.

---

## 10. Boot Sequence

```c
// main/main.c

void app_main(void)
{
    // 1. NVS — must come first; Wi-Fi and calibration both need it
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. Wi-Fi — blocking until IP or max retries exhausted
    ESP_ERROR_CHECK(wifi_manager_init());
    wifi_manager_connect();  // non-fatal; scale works offline

    // 3. HX711 — configure GPIO and load calibration from NVS
    hx711_config_t hx_cfg = {
        .dout_pin = CONFIG_HX711_DOUT_GPIO,
        .sck_pin  = CONFIG_HX711_SCK_GPIO,
        .gain     = HX711_GAIN_A_128,
    };
    ESP_ERROR_CHECK(hx711_init(&s_hx711, &hx_cfg));

    if (calibration_load(&s_hx711) != ESP_OK)
        hx711_set_scale(&s_hx711, CONFIG_SCALE_FACTOR);  // compile-time default

    // 4. Initial tare — settle time then capture zero offset
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_ERROR_CHECK(hx711_tare(&s_hx711, CONFIG_TARE_SAMPLES));

    // 5. Web server — only if Wi-Fi is up
    if (wifi_manager_is_connected())
        ESP_ERROR_CHECK(web_server_start(&s_hx711));

    // 6. Measurement task — runs forever
    xTaskCreate(task_measure, "scale_measure",
                CONFIG_MEASURE_TASK_STACK, NULL,
                CONFIG_MEASURE_TASK_PRIORITY, NULL);
}
```

The 500 ms settle delay before the initial tare is not arbitrary. After `hx711_init()`, the HX711 may still be completing its first internal conversion (which takes up to 100 ms at 10 Hz). If `hx711_tare()` is called before the first conversion completes, `hx711_wait_ready()` will return immediately on a stale LOW from a previous read rather than waiting for fresh data.

---

## 11. Calibration: End-to-End Worked Example

Suppose the system reads the following with an empty platform:

```
tare = −47 382 raw counts
```

With a calibrated 1000 g test weight:

```
avg_loaded = 383 618 raw counts
delta      = 383 618 − (−47 382) = 431 000 counts
scale      = 431 000 / 1000 = 431.0 counts/g
```

To verify: what does the system report for an unknown mass that produces 450 000 raw counts?

```
weight = (450 000 − (−47 382)) / 431.0 = 497 382 / 431.0 ≈ 1153.9 g
```

The scale factor is the single most important tunable in the system. The compile-time default (`CONFIG_SCALE_FACTOR = 430.0f`) is only an approximation; every physical build will produce a slightly different value depending on the mechanical geometry of the frame and the characteristics of the specific cell batch. Always calibrate with the platform assembled in its final configuration.

---

## 12. Key Design Decisions and Tradeoffs

### Bit-bang vs Hardware SPI

The HX711 protocol is not standard SPI: CPOL/CPHA are unusual, the clock frequency is constrained to < 1 MHz (to satisfy T1 ≤ 50 µs), the number of clock pulses per transaction varies (24+1, 24+2, or 24+3), and the CS concept does not apply. Configuring the ESP32-S3's SPI peripheral for these constraints would require manual CS and custom ISR work that is more complex than bit-banging and offers no practical benefit at 10–80 Hz data rates.

### Embedded HTML vs SPIFFS

Embedding the dashboard as a C string requires no file system, no partition tooling, and no runtime file I/O. The main cost is that the HTML is not human-readable in flash and must be recompiled to update. For a single-page instrument dashboard this is acceptable; for a multi-page web application with many assets, LittleFS with a proper HTTP file server would be the correct approach.

### SSE vs WebSockets vs Polling

Polling at 500 ms intervals would consume two HTTP connections per second per client. WebSockets would provide lower latency but require more server-side code and complicate connection lifecycle management. SSE is the best fit: one persistent connection per client, automatic reconnection, and a wire format simple enough to format with a single `snprintf()`.

### Single-Point vs Multi-Point Calibration

A single reference weight produces a linear calibration (one point plus the implicit zero at tare). Strain-gauge load cells are specified linear to < 0.05% over their rated range, so multi-point calibration is unnecessary for most applications. If you are measuring materials with strongly non-uniform density or using the scale near its mechanical limits, a two-point calibration (using weights at roughly 20% and 80% of full scale) would catch any residual non-linearity.

---

## 13. Troubleshooting Guide for Engineers

**`ESP_ERR_TIMEOUT` from `hx711_wait_ready()`**
The HX711 DOUT pin never went LOW. The most common causes are: VCC not connected or below 2.6 V; SCK accidentally held HIGH before init (triggering power-down); and reversed DOUT/SCK connections. Measure VCC with a multimeter first; then probe SCK with an oscilloscope to confirm it is LOW at rest.

**Reading is stable but offset from zero after tare**
Tare was captured while the scale was still settling. The HX711 needs ~400 ms after power-up before its first reliable reading, and the load cells themselves exhibit creep — a slow change in resistance after a step load change. Re-run the tare sequence at least 30 seconds after powering on.

**Reading is correct sign but systematically high or low**
The scale factor is wrong. Re-calibrate. The most common cause is calibrating with a weight that was not at the effective centre of the platform. In a four-cell corner-supported design, off-centre loads produce different readings on each cell, and the bridge output differs from a centred load of the same mass.

**SSE stream connects but no events arrive**
Check that `web_server_push_weight()` is being called: add an `ESP_LOGI` just before the `lwip_send()` loop. If the log appears but events do not arrive, the socket fd was removed from the array (a failed `lwip_send()` removes the client). Check free heap — the ESP32-S3's default heap is large but SSE connections consume lwIP TCP buffers.

**Build error: `recv` / `send` undeclared**
Add `#include "lwip/sockets.h"` and use the explicit `lwip_recv()` / `lwip_send()` forms rather than POSIX names. The POSIX-compatible aliases are only available when `CONFIG_LWIP_COMPAT_SOCKETS=y` is set in sdkconfig.

---

## Conclusion

Building a reliable digital scale on an embedded platform touches every layer of the stack: analogue hardware (Wheatstone bridge physics), low-level firmware (microsecond-accurate bit-banging with interrupt disabling), RTOS design (deterministic periodic task, mutex-protected shared state), network programming (HTTP server, SSE protocol), and calibration mathematics (linear regression from a single reference point).

The ESP32-S3 and ESP-IDF provide a surprisingly capable platform for all of this. The built-in `esp_http_server`, lwIP socket layer, NVS flash storage, and FreeRTOS make what would have been a significant engineering effort in a bare-metal environment manageable in a few thousand lines of well-structured C.

The complete project — driver, application, calibration wizard, web server, and embedded dashboard — is available on GitHub under the MIT license.

---

## References

- Avia Semiconductor. *HX711 24-Bit Analog-to-Digital Converter for Weigh Scales.* Datasheet, Rev 1.0.
- Espressif Systems. *ESP32-S3 Technical Reference Manual.* Rev 1.3. 2023.
- Espressif Systems. *ESP-IDF Programming Guide v5.x.* [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- W3C. *Server-Sent Events.* W3C Recommendation. [w3.org/TR/eventsource](https://www.w3.org/TR/eventsource/)
- National Instruments. *Strain Gauge Measurement – A Tutorial.* Application Note 078, 1998.
