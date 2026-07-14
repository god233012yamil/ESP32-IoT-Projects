#ifndef BLE_LONG_RANGE_H
#define BLE_LONG_RANGE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the BLE long-range GATT server.
 *
 * The function initializes NVS, the Bluetooth controller, Bluedroid, the
 * GATT server, Coded PHY preferences, transmit power, and extended
 * advertising.
 *
 * Returns:
 *     ESP_OK: Initialization completed successfully.
 *     Other: An ESP-IDF error code identifying the failed operation.
 */
esp_err_t ble_long_range_init(void);

#ifdef __cplusplus
}
#endif

#endif
