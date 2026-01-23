#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file wifi_manager.h
 * @brief Minimal Wi-Fi helper for a connect-send-disconnect workflow.
 *
 * The goal is to keep radio-on time short. Battery-powered designs should
 * avoid staying connected unless required. This helper:
 *   1) initializes Wi-Fi and the default netif
 *   2) connects with a timeout
 *   3) performs a simple TCP connect (demo transaction)
 *   4) shuts Wi-Fi down cleanly
 */

/**
 * @brief Connect to Wi-Fi using credentials from Kconfig.
 *
 * @param timeout_ms Maximum time to wait for connection.
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t wifi_manager_connect(uint32_t timeout_ms);

/**
 * @brief Perform a minimal TCP connect to prove connectivity.
 *
 * This is intentionally small: it opens a socket to (host, port) and closes it.
 * Replace it with your MQTT/HTTP/WebSocket transaction.
 *
 * @param host Hostname or IP address.
 * @param port TCP port.
 * @param timeout_ms Socket connect timeout.
 * @return ESP_OK on success, otherwise an ESP-IDF error code.
 */
esp_err_t wifi_manager_demo_tx(const char *host, uint16_t port, uint32_t timeout_ms);

/**
 * @brief Stop and deinitialize Wi-Fi.
 *
 * Call this before entering deep sleep to avoid leaving radio resources enabled.
 */
void wifi_manager_shutdown(void);

#ifdef __cplusplus
}
#endif
