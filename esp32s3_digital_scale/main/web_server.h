/**
 * @file web_server.h
 * @brief HTTP server and Server-Sent Events (SSE) interface.
 *
 * Serves a single-page dashboard (HTML/CSS/JS embedded in flash) and
 * exposes a JSON REST API plus an SSE stream so the browser dashboard
 * receives live weight updates without polling.
 *
 * Endpoints:
 *   GET  /              – Dashboard HTML page
 *   GET  /api/weight    – Current weight JSON  {"weight_g":123.4,"unit":"g"}
 *   GET  /api/status    – System info JSON
 *   POST /api/tare      – Trigger tare;        {"status":"ok"}
 *   POST /api/calibrate – Set scale factor;    body: {"scale":430.0}
 *   GET  /events        – SSE stream of weight updates
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "hx711.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the HTTP server and register all URI handlers.
 *
 * Must be called after Wi-Fi is connected and an IP address is available.
 * The @p dev pointer is stored internally and used by API handlers.
 *
 * @param[in] dev Initialised and calibrated HX711 device handle.
 * @return ESP_OK on success; esp_http_server error otherwise.
 */
esp_err_t web_server_start(hx711_dev_t *dev);

/**
 * @brief Stop the HTTP server and free all resources.
 *
 * @return ESP_OK always.
 */
esp_err_t web_server_stop(void);

/**
 * @brief Push a weight sample to all connected SSE clients.
 *
 * Called periodically by the SSE push task. Formats the value as a
 * standard SSE message: "data: {\"weight_g\":NNN.NN}\n\n".
 *
 * @param[in] weight_g Current weight in grams.
 * @return ESP_OK if at least one client received the event;
 *         ESP_ERR_NOT_FOUND if no SSE clients are connected.
 */
esp_err_t web_server_push_weight(float weight_g);

/**
 * @brief Return the number of currently active SSE connections.
 *
 * @return Active SSE client count (0 if none).
 */
int web_server_sse_client_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_H */
