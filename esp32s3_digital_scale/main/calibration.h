/**
 * @file calibration.h
 * @brief Calibration subsystem – NVS persistence and guided wizard.
 *
 * Provides an interactive UART-driven calibration flow:
 *   1. Tare (zero-point capture) with empty platform.
 *   2. Place known reference weight.
 *   3. Compute and store the scale factor.
 *
 * Results are persisted to NVS so they survive reboots.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "esp_err.h"
#include "hx711.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the interactive calibration wizard over the UART console.
 *
 * Walks the operator through taring and a known-weight measurement,
 * computes the scale factor, and saves it to NVS.
 *
 * @param[in] dev Initialised HX711 device handle.
 * @return ESP_OK on success; NVS or ADC error otherwise.
 */
esp_err_t calibration_run(hx711_dev_t *dev);

/**
 * @brief Load saved calibration data from NVS and apply it to the device.
 *
 * @param[in] dev Initialised HX711 device handle.
 * @return ESP_OK if data was found and applied.
 *         ESP_ERR_NVS_NOT_FOUND if no prior calibration exists.
 */
esp_err_t calibration_load(hx711_dev_t *dev);

/**
 * @brief Persist the device's current scale factor and tare to NVS.
 *
 * @param[in] dev Initialised HX711 device handle.
 * @return ESP_OK on success; NVS write error otherwise.
 */
esp_err_t calibration_save(const hx711_dev_t *dev);

/**
 * @brief Erase all stored calibration keys from NVS.
 *
 * After erasure the application will use compile-time defaults.
 *
 * @return ESP_OK on success; NVS error otherwise.
 */
esp_err_t calibration_erase(void);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_H */
