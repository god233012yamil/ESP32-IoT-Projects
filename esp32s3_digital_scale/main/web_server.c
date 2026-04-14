/**
 * @file web_server.c
 * @brief HTTP server, REST API, and SSE implementation.
 *
 * The dashboard HTML/CSS/JS is embedded directly in flash as a C string
 * literal (no SPIFFS/LittleFS required).  The SSE stream is implemented
 * with chunked transfer encoding so the browser receives live weight
 * updates at CONFIG_SSE_PUSH_INTERVAL_MS intervals.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#include "web_server.h"
#include "scale_config.h"
#include "wifi_manager.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

static const char *TAG = "WEB_SERVER";

/* ── Internal state ───────────────────────────────────────────────── */

static httpd_handle_t  s_server     = NULL;
static hx711_dev_t    *s_dev        = NULL;
static float           s_last_weight_g = 0.0f;
static SemaphoreHandle_t s_weight_mutex = NULL;

/* Track open SSE socket descriptors (simple fixed array) */
#define MAX_SSE_CLIENTS 4
static int s_sse_fds[MAX_SSE_CLIENTS];
static int s_sse_count = 0;
static SemaphoreHandle_t s_sse_mutex = NULL;

/* ── Dashboard HTML (embedded) ────────────────────────────────────── */

/**
 * @brief Full single-page dashboard served from flash.
 *
 * Features:
 *   - Live weight display updated via EventSource (SSE).
 *   - Chart.js rolling 60-second history graph.
 *   - Tare and manual calibration controls.
 *   - Unit toggle (g / kg / lb).
 *   - Connection status indicator.
 *   - Responsive layout for desktop and mobile.
 */
static const char DASHBOARD_HTML[] =
"<!DOCTYPE html>"
"<html lang=\"en\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>ESP32-S3 Digital Scale</title>"
"<script src=\"https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js\"></script>"
"<style>"
":root{"
"--bg:#0f172a;--surface:#1e293b;--surface2:#334155;--accent:#38bdf8;"
"--accent2:#818cf8;--green:#4ade80;--red:#f87171;--yellow:#fbbf24;"
"--text:#f1f5f9;--muted:#94a3b8;--border:#475569;"
"--radius:1rem;--shadow:0 4px 24px rgba(0,0,0,.4);"
"}"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;"
"min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:1.5rem}"
"h1{font-size:1.4rem;font-weight:700;letter-spacing:.05em;color:var(--accent);"
"margin-bottom:1.5rem;display:flex;align-items:center;gap:.6rem}"
"h1 svg{width:1.6rem;height:1.6rem}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));"
"gap:1rem;width:100%;max-width:900px}"
".card{background:var(--surface);border:1px solid var(--border);"
"border-radius:var(--radius);padding:1.4rem;box-shadow:var(--shadow)}"
".card-title{font-size:.75rem;font-weight:600;text-transform:uppercase;"
"letter-spacing:.1em;color:var(--muted);margin-bottom:.8rem}"
/* Weight display */
"#weight-display{font-size:4.5rem;font-weight:800;line-height:1;"
"letter-spacing:-.02em;color:var(--text);text-align:center;"
"transition:color .3s}"
"#weight-unit{font-size:1.4rem;font-weight:400;color:var(--muted);margin-left:.3rem}"
"#weight-card{text-align:center}"
/* Status dot */
".status-row{display:flex;align-items:center;gap:.5rem;margin-top:.8rem;"
"justify-content:center}"
".dot{width:.65rem;height:.65rem;border-radius:50%;background:var(--red);"
"transition:background .4s}"
".dot.online{background:var(--green);box-shadow:0 0 8px var(--green)}"
".status-label{font-size:.8rem;color:var(--muted)}"
/* Buttons */
".btn{display:inline-flex;align-items:center;justify-content:center;gap:.4rem;"
"padding:.55rem 1.1rem;border-radius:.6rem;border:none;cursor:pointer;"
"font-size:.85rem;font-weight:600;transition:opacity .2s,transform .1s}"
".btn:active{transform:scale(.97)}"
".btn-accent{background:var(--accent);color:#0f172a}"
".btn-outline{background:transparent;border:1px solid var(--border);color:var(--text)}"
".btn-danger{background:var(--red);color:#fff}"
".btn-row{display:flex;gap:.6rem;flex-wrap:wrap;margin-top:.6rem}"
/* Unit toggle */
".unit-toggle{display:flex;gap:.4rem;justify-content:center;margin-top:.8rem}"
".unit-btn{padding:.35rem .9rem;border-radius:.5rem;border:1px solid var(--border);"
"background:transparent;color:var(--muted);cursor:pointer;font-size:.8rem;font-weight:600;"
"transition:all .2s}"
".unit-btn.active{background:var(--accent2);color:#fff;border-color:var(--accent2)}"
/* Chart */
"#history-card{grid-column:1/-1}"
"canvas{max-height:220px}"
/* Stats row */
".stats{display:grid;grid-template-columns:repeat(3,1fr);gap:.6rem;margin-top:.4rem}"
".stat{background:var(--surface2);border-radius:.6rem;padding:.7rem;text-align:center}"
".stat-val{font-size:1.2rem;font-weight:700;color:var(--accent)}"
".stat-lbl{font-size:.7rem;color:var(--muted);margin-top:.2rem}"
/* Calibration */
"input[type=number]{background:var(--surface2);border:1px solid var(--border);"
"border-radius:.5rem;color:var(--text);padding:.45rem .7rem;font-size:.9rem;width:100%;"
"margin-bottom:.6rem}"
"input[type=number]:focus{outline:2px solid var(--accent);border-color:transparent}"
".notice{font-size:.75rem;color:var(--muted);margin-top:.4rem;line-height:1.5}"
/* Toast */
"#toast{position:fixed;bottom:1.5rem;right:1.5rem;background:var(--surface);"
"border:1px solid var(--border);border-radius:.7rem;padding:.7rem 1.1rem;"
"font-size:.85rem;box-shadow:var(--shadow);opacity:0;pointer-events:none;"
"transition:opacity .3s;z-index:99}"
"#toast.show{opacity:1}"
"@media(max-width:480px){#weight-display{font-size:3rem}}"
"</style>"
"</head>"
"<body>"
"<h1>"
"<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\">"
"<path d=\"M3 6l9-3 9 3v6c0 5-4 8.5-9 10C7 18.5 3 15 3 12V6z\"/>"
"<path d=\"M12 8v4l2 2\"/>"
"</svg>"
"ESP32-S3 Digital Scale"
"</h1>"

"<div class=\"grid\">"

/* ── Weight card ── */
"<div class=\"card\" id=\"weight-card\">"
"<div class=\"card-title\">Current Weight</div>"
"<div id=\"weight-display\">---<span id=\"weight-unit\"> g</span></div>"
"<div class=\"unit-toggle\">"
"<button class=\"unit-btn active\" onclick=\"setUnit('g')\">g</button>"
"<button class=\"unit-btn\" onclick=\"setUnit('kg')\">kg</button>"
"<button class=\"unit-btn\" onclick=\"setUnit('lb')\">lb</button>"
"</div>"
"<div class=\"status-row\">"
"<div class=\"dot\" id=\"dot\"></div>"
"<span class=\"status-label\" id=\"status-label\">Connecting…</span>"
"</div>"
"</div>"

/* ── Controls card ── */
"<div class=\"card\">"
"<div class=\"card-title\">Controls</div>"
"<div class=\"btn-row\">"
"<button class=\"btn btn-accent\" onclick=\"doTare()\">&#9654; Tare (Zero)</button>"
"<button class=\"btn btn-outline\" onclick=\"resetStats()\">Reset Stats</button>"
"</div>"
"<div class=\"notice\" id=\"tare-status\">Place nothing on the scale before taring.</div>"
"</div>"

/* ── Stats card ── */
"<div class=\"card\">"
"<div class=\"card-title\">Session Statistics</div>"
"<div class=\"stats\">"
"<div class=\"stat\"><div class=\"stat-val\" id=\"stat-min\">--</div><div class=\"stat-lbl\">Min</div></div>"
"<div class=\"stat\"><div class=\"stat-val\" id=\"stat-max\">--</div><div class=\"stat-lbl\">Max</div></div>"
"<div class=\"stat\"><div class=\"stat-val\" id=\"stat-avg\">--</div><div class=\"stat-lbl\">Avg</div></div>"
"</div>"
"</div>"

/* ── Calibration card ── */
"<div class=\"card\">"
"<div class=\"card-title\">Calibration</div>"
"<label style=\"font-size:.8rem;color:var(--muted);display:block;margin-bottom:.3rem\">"
"Scale factor (counts/g)</label>"
"<input type=\"number\" id=\"cal-input\" placeholder=\"e.g. 430.0\" step=\"0.1\">"
"<div class=\"btn-row\">"
"<button class=\"btn btn-outline\" onclick=\"applyCalibration()\">Apply</button>"
"</div>"
"<div class=\"notice\">Calibrate: place known weight, read raw, compute factor = raw/grams.</div>"
"</div>"

/* ── System card ── */
"<div class=\"card\">"
"<div class=\"card-title\">System Info</div>"
"<div id=\"sys-info\" style=\"font-size:.8rem;color:var(--muted);line-height:2\">Loading…</div>"
"</div>"

/* ── History chart ── */
"<div class=\"card\" id=\"history-card\">"
"<div class=\"card-title\">Weight History (last 60 s)</div>"
"<canvas id=\"chart\"></canvas>"
"</div>"

"</div>"

"<div id=\"toast\"></div>"

"<script>"
"/* ── State ── */"
"let unit='g',minW=Infinity,maxW=-Infinity,sumW=0,countW=0;"
"const history=[];"
"const MAX_HIST=120;" /* 60s @ 500ms */

"/* ── Unit conversion ── */"
"function toUnit(g){"
"if(unit==='kg')return(g/1000).toFixed(3);"
"if(unit==='lb')return(g/453.592).toFixed(3);"
"return g.toFixed(1);"
"}"
"function setUnit(u){"
"unit=u;"
"document.querySelectorAll('.unit-btn').forEach(b=>b.classList.toggle('active',b.textContent===u));"
"document.getElementById('weight-unit').textContent=' '+u;"
"updateStats();"
"}"

"/* ── Stats ── */"
"function updateStats(){"
"document.getElementById('stat-min').textContent=minW===Infinity?'--':toUnit(minW)+' '+unit;"
"document.getElementById('stat-max').textContent=maxW===-Infinity?'--':toUnit(maxW)+' '+unit;"
"document.getElementById('stat-avg').textContent=countW===0?'--':(toUnit(sumW/countW))+' '+unit;"
"}"
"function resetStats(){"
"minW=Infinity;maxW=-Infinity;sumW=0;countW=0;history.length=0;"
"updateStats();chart.data.labels=[];chart.data.datasets[0].data=[];chart.update();"
"toast('Stats reset');"
"}"

"/* ── Weight update ── */"
"function updateWeight(g){"
"const el=document.getElementById('weight-display');"
"const unit_el=document.getElementById('weight-unit');"
"el.textContent=toUnit(g);"
"unit_el.textContent=' '+unit;"
"el.style.color=Math.abs(g)<2?'var(--muted)':'var(--text)';"
"/* stats */"
"if(g<minW)minW=g;"
"if(g>maxW)maxW=g;"
"sumW+=g;countW++;"
"updateStats();"
"/* chart */"
"const now=new Date();"
"const label=now.getHours().toString().padStart(2,'0')+':'"
"+now.getMinutes().toString().padStart(2,'0')+':'"
"+now.getSeconds().toString().padStart(2,'0');"
"history.push({t:label,v:g});"
"if(history.length>MAX_HIST)history.shift();"
"chart.data.labels=history.map(h=>h.t);"
"chart.data.datasets[0].data=history.map(h=>h.v);"
"chart.update('none');"
"}"

"/* ── SSE ── */"
"function connectSSE(){"
"const src=new EventSource('/events');"
"src.onopen=()=>{"
"document.getElementById('dot').classList.add('online');"
"document.getElementById('status-label').textContent='Connected';"
"};"
"src.onerror=()=>{"
"document.getElementById('dot').classList.remove('online');"
"document.getElementById('status-label').textContent='Reconnecting…';"
"};"
"src.addEventListener('weight',e=>{"
"const d=JSON.parse(e.data);"
"updateWeight(parseFloat(d.weight_g));"
"});"
"}"

"/* ── REST helpers ── */"
"async function doTare(){"
"document.getElementById('tare-status').textContent='Taring…';"
"const r=await fetch('/api/tare',{method:'POST'});"
"if(r.ok){resetStats();document.getElementById('tare-status').textContent='Tare complete!';toast('Tare applied');}"
"else document.getElementById('tare-status').textContent='Tare failed!';"
"}"

"async function applyCalibration(){"
"const v=parseFloat(document.getElementById('cal-input').value);"
"if(!v||v<=0){toast('Enter a valid scale factor');return;}"
"const r=await fetch('/api/calibrate',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({scale:v})});"
"if(r.ok)toast('Scale factor '+v+' applied');else toast('Calibration failed');"
"}"

"async function loadStatus(){"
"try{"
"const r=await fetch('/api/status');"
"const d=await r.json();"
"document.getElementById('sys-info').innerHTML="
"'IP: <b>'+d.ip+'</b><br>'"
"+'Uptime: <b>'+d.uptime_s+' s</b><br>'"
"+'Scale factor: <b>'+d.scale_factor.toFixed(4)+'</b><br>'"
"+'Free heap: <b>'+d.free_heap+' B</b>';"
"}catch(e){}"
"setTimeout(loadStatus,10000);"
"}"

"/* ── Toast ── */"
"let toastTimer;"
"function toast(msg){"
"const el=document.getElementById('toast');"
"el.textContent=msg;el.classList.add('show');"
"clearTimeout(toastTimer);"
"toastTimer=setTimeout(()=>el.classList.remove('show'),2500);"
"}"

"/* ── Chart init ── */"
"const ctx=document.getElementById('chart').getContext('2d');"
"const chart=new Chart(ctx,{"
"type:'line',"
"data:{labels:[],datasets:[{label:'Weight',data:[],"
"borderColor:'#38bdf8',backgroundColor:'rgba(56,189,248,.08)',"
"borderWidth:2,pointRadius:0,fill:true,tension:.35}]},"
"options:{animation:false,responsive:true,maintainAspectRatio:true,"
"plugins:{legend:{display:false},tooltip:{mode:'index',intersect:false}},"
"scales:{"
"x:{ticks:{maxTicksLimit:8,color:'#94a3b8'},grid:{color:'#1e293b'}},"
"y:{ticks:{color:'#94a3b8'},grid:{color:'#334155'}}"
"}}});"

"/* ── Boot ── */"
"connectSSE();"
"loadStatus();"
"</script>"
"</body>"
"</html>";

/* ── SSE helpers ──────────────────────────────────────────────────── */

/**
 * @brief Register a new SSE client socket descriptor.
 *
 * Acquires s_sse_mutex, adds @p fd to the tracking array if a slot is
 * free, and releases the mutex.  Silently drops if the array is full.
 *
 * @param[in] fd Socket file descriptor of the new SSE client.
 */
static void sse_add_client(int fd)
{
    xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (s_sse_fds[i] < 0) {
            s_sse_fds[i] = fd;
            s_sse_count++;
            ESP_LOGI(TAG, "SSE client added fd=%d  total=%d", fd, s_sse_count);
            break;
        }
    }
    xSemaphoreGive(s_sse_mutex);
}

/**
 * @brief Remove a disconnected SSE client from the tracking array.
 *
 * @param[in] fd Socket file descriptor to remove.
 */
static void sse_remove_client(int fd)
{
    xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (s_sse_fds[i] == fd) {
            s_sse_fds[i] = -1;
            s_sse_count--;
            ESP_LOGI(TAG, "SSE client removed fd=%d  total=%d", fd, s_sse_count);
            break;
        }
    }
    xSemaphoreGive(s_sse_mutex);
}

/* ── URI handlers ─────────────────────────────────────────────────── */

/**
 * @brief Serve the embedded dashboard HTML page.
 *
 * Responds to GET / with the DASHBOARD_HTML string, Content-Type
 * text/html, and HTTP 200.
 *
 * @param[in] req Incoming HTTP request handle.
 * @return ESP_OK always (httpd_resp_send handles errors internally).
 */
static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, DASHBOARD_HTML, strlen(DASHBOARD_HTML));
    return ESP_OK;
}

/**
 * @brief Return the latest weight as a JSON object.
 *
 * Response body: {"weight_g": 123.45, "unit": "g"}
 *
 * @param[in] req Incoming HTTP request handle.
 * @return ESP_OK on success.
 */
static esp_err_t handler_api_weight(httpd_req_t *req)
{
    float w = 0.0f;
    xSemaphoreTake(s_weight_mutex, portMAX_DELAY);
    w = s_last_weight_g;
    xSemaphoreGive(s_weight_mutex);

    char buf[64];
    snprintf(buf, sizeof(buf), "{\"weight_g\":%.2f,\"unit\":\"g\"}", w);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

/**
 * @brief Execute a tare operation and return {"status":"ok"}.
 *
 * Averages CONFIG_TARE_SAMPLES readings to establish the zero offset,
 * then resets the last weight to 0.
 *
 * @param[in] req Incoming HTTP request handle.
 * @return ESP_OK on success; HTTP 500 on HX711 failure.
 */
static esp_err_t handler_api_tare(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP POST /api/tare");
    esp_err_t err = hx711_tare(s_dev, CONFIG_TARE_SAMPLES);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Tare failed");
        return ESP_FAIL;
    }

    xSemaphoreTake(s_weight_mutex, portMAX_DELAY);
    s_last_weight_g = 0.0f;
    xSemaphoreGive(s_weight_mutex);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", 14);
    return ESP_OK;
}

/**
 * @brief Accept a new scale factor via JSON body and apply it.
 *
 * Expected request body: {"scale": 430.0}
 * Persists the new factor to NVS via the hx711 handle.
 *
 * @param[in] req Incoming HTTP request handle.
 * @return ESP_OK on success; HTTP 400/500 on parse or apply failure.
 */
static esp_err_t handler_api_calibrate(httpd_req_t *req)
{
    char body[64] = {0};
    int  recv_len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (recv_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    body[recv_len] = '\0';

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
    cJSON_Delete(json);

    esp_err_t err = hx711_set_scale(s_dev, new_scale);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Apply failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Scale factor updated via HTTP: %.4f", new_scale);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", 14);
    return ESP_OK;
}

/**
 * @brief Return system information as a JSON object.
 *
 * Fields: ip, uptime_s, scale_factor, tare, free_heap, ssid.
 *
 * @param[in] req Incoming HTTP request handle.
 * @return ESP_OK always.
 */
static esp_err_t handler_api_status(httpd_req_t *req)
{
    float scale = 0.0f;
    hx711_get_scale(s_dev, &scale);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{"
             "\"ip\":\"%s\","
             "\"uptime_s\":%lld,"
             "\"scale_factor\":%.4f,"
             "\"tare\":%ld,"
             "\"free_heap\":%lu,"
             "\"ssid\":\"%s\""
             "}",
             wifi_manager_get_ip(),
             (long long)(esp_timer_get_time() / 1000000LL),
             scale,
             (long)s_dev->tare,
             (unsigned long)esp_get_free_heap_size(),
             CONFIG_WIFI_SSID);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

/**
 * @brief Handle an SSE subscription request (GET /events).
 *
 * Sends the HTTP/1.1 200 headers with Content-Type: text/event-stream
 * and keeps the connection open, registering the socket as an SSE
 * client.  The actual event data is sent by web_server_push_weight().
 *
 * This function blocks until the client disconnects.
 *
 * @param[in] req Incoming HTTP request handle.
 * @return ESP_OK when the client disconnects normally.
 */
static esp_err_t handler_sse(httpd_req_t *req)
{
    /* SSE headers */
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection",    "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    /* Send an initial comment to flush the headers */
    const char *init = ": connected\n\n";
    httpd_resp_send_chunk(req, init, strlen(init));

    int fd = httpd_req_to_sockfd(req);
    sse_add_client(fd);

    /* Keep alive: block in 200 ms heartbeat loop until client drops.
     * Use lwIP select() + recv() which are available via lwip/sockets.h. */
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(200));

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = {0, 0};
        int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (sel > 0) {
            char tmp[4];
            int n = lwip_recv(fd, tmp, sizeof(tmp), MSG_PEEK | MSG_DONTWAIT);
            if (n == 0) break; /* client closed connection */
        }
    }

    sse_remove_client(fd);
    return ESP_OK;
}

/* ── Public API ───────────────────────────────────────────────────── */

/**
 * @brief Start the HTTP server and register URI handlers.
 */
esp_err_t web_server_start(hx711_dev_t *dev)
{
    s_dev = dev;

    s_weight_mutex = xSemaphoreCreateMutex();
    s_sse_mutex    = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) s_sse_fds[i] = -1;

    httpd_config_t cfg  = HTTPD_DEFAULT_CONFIG();
    cfg.server_port     = CONFIG_WEBSERVER_PORT;
    cfg.max_open_sockets = CONFIG_WEBSERVER_MAX_SOCKETS + MAX_SSE_CLIENTS;
    cfg.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Register URI handlers */
    static const httpd_uri_t uris[] = {
        { .uri = "/",              .method = HTTP_GET,  .handler = handler_root },
        { .uri = "/api/weight",    .method = HTTP_GET,  .handler = handler_api_weight },
        { .uri = "/api/tare",      .method = HTTP_POST, .handler = handler_api_tare },
        { .uri = "/api/calibrate", .method = HTTP_POST, .handler = handler_api_calibrate },
        { .uri = "/api/status",    .method = HTTP_GET,  .handler = handler_api_status },
        { .uri = "/events",        .method = HTTP_GET,  .handler = handler_sse },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", CONFIG_WEBSERVER_PORT);
    ESP_LOGI(TAG, "Dashboard: http://%s/", wifi_manager_get_ip());
    return ESP_OK;
}

/**
 * @brief Stop the HTTP server.
 */
esp_err_t web_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    ESP_LOGI(TAG, "HTTP server stopped");
    return ESP_OK;
}

/**
 * @brief Push a weight reading to all active SSE clients.
 *
 * Formats the message as SSE with a named event type "weight":
 *   event: weight\ndata: {"weight_g":NNN.NN}\n\n
 * Failed sends on individual sockets remove those clients automatically.
 */
esp_err_t web_server_push_weight(float weight_g)
{
    /* Cache latest value for REST endpoint */
    xSemaphoreTake(s_weight_mutex, portMAX_DELAY);
    s_last_weight_g = weight_g;
    xSemaphoreGive(s_weight_mutex);

    if (s_sse_count == 0) return ESP_ERR_NOT_FOUND;

    char msg[80];
    int  msg_len = snprintf(msg, sizeof(msg),
                            "event: weight\ndata: {\"weight_g\":%.2f}\n\n",
                            weight_g);

    xSemaphoreTake(s_sse_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (s_sse_fds[i] < 0) continue;
        int sent = lwip_send(s_sse_fds[i], msg, msg_len, MSG_DONTWAIT);
        if (sent < 0) {
            ESP_LOGW(TAG, "SSE send failed fd=%d; removing", s_sse_fds[i]);
            s_sse_fds[i] = -1;
            s_sse_count--;
        }
    }
    xSemaphoreGive(s_sse_mutex);

    return ESP_OK;
}

/**
 * @brief Return the number of currently connected SSE clients.
 */
int web_server_sse_client_count(void)
{
    return s_sse_count;
}
