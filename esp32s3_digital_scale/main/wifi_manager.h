/**
 * @file wifi_manager.h
 * @brief Wi-Fi station (STA) connection manager for the ESP32-S3 Scale.
 *
 * Handles Wi-Fi initialisation, connection with retry logic, and
 * exposes a blocking helper that waits for an IP address before
 * the web server starts.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the TCP/IP stack, create the default STA interface,
 *        and start the Wi-Fi driver.
 *
 * Must be called once before wifi_manager_connect().
 *
 * @return ESP_OK on success; ESP_FAIL or nvs/netif error otherwise.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Connect to the SSID/password defined in scale_config.h.
 *
 * Blocks until an IP address is obtained or CONFIG_WIFI_MAX_RETRIES
 * consecutive connection attempts have failed.
 *
 * @return ESP_OK when connected and an IP is available.
 *         ESP_FAIL if the maximum retries were exhausted.
 */
esp_err_t wifi_manager_connect(void);

/**
 * @brief Query current connection status.
 *
 * @return true  when the station is associated and has an IP address.
 * @return false otherwise.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Return the assigned IPv4 address as a dotted-decimal string.
 *
 * The buffer is internally managed; the pointer is valid until the next
 * call to this function.
 *
 * @return Pointer to a static string such as "192.168.1.42", or
 *         "0.0.0.0" if not connected.
 */
const char *wifi_manager_get_ip(void);

/**
 * @brief Disconnect from the access point and stop the Wi-Fi driver.
 *
 * @return ESP_OK always.
 */
esp_err_t wifi_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
