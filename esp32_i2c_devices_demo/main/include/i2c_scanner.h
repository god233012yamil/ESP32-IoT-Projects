#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Scans the usable seven-bit I2C address range.
 *
 * @param addresses Optional output array that receives discovered addresses.
 * @param capacity Number of entries available in the output array.
 * @param found_count Output pointer that receives the number of detected
 *        devices. The count can be larger than capacity when the output array
 *        is too small.
 *
 * @return ESP_OK after a completed scan. ESP_ERR_INVALID_ARG is returned when
 *         found_count is NULL.
 */
esp_err_t i2c_scanner_scan(uint8_t *addresses,
                           size_t capacity,
                           size_t *found_count);

#endif
