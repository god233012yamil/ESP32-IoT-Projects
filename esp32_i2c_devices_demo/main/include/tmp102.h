#ifndef TMP102_H
#define TMP102_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/**
 * @brief TMP102 device context.
 */
typedef struct {
    i2c_master_dev_handle_t device_handle;
    uint8_t address;
    bool initialized;
} tmp102_t;

/**
 * @brief Initializes a TMP102 temperature sensor driver instance.
 *
 * @param sensor Driver context to initialize.
 * @param address Seven-bit TMP102 address, normally 0x48 through 0x4B.
 * @param scl_speed_hz I2C clock speed in hertz.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t tmp102_init(tmp102_t *sensor,
                      uint8_t address,
                      uint32_t scl_speed_hz);

/**
 * @brief Reads the TMP102 temperature register.
 *
 * @param sensor Initialized driver context.
 * @param temperature_c Output pointer that receives temperature in degrees C.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t tmp102_read_temperature_c(const tmp102_t *sensor,
                                    float *temperature_c);

/**
 * @brief Releases the TMP102 device handle.
 *
 * @param sensor Driver context to deinitialize.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t tmp102_deinit(tmp102_t *sensor);

#endif
