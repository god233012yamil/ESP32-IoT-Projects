#pragma once
#include "esp_err.h"

/*
 * wifi_station_start
 *
 * Initializes Wi-Fi in station mode and connects using SSID/password
 * from Kconfig (menuconfig).
 *
 * Returns:
 *   ESP_OK on successful connection (IP acquired), otherwise an error code.
 */
esp_err_t wifi_station_start(void);