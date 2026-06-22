#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/**
 * @brief Initializes the shared I2C master bus.
 *
 * The bus settings are read from menuconfig options defined in
 * Kconfig.projbuild. External pull-up resistors should be fitted to SDA and
 * SCL for reliable operation.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t app_i2c_bus_init(void);

/**
 * @brief Deinitializes the shared I2C master bus.
 *
 * All device handles must be removed before this function is called.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t app_i2c_bus_deinit(void);

/**
 * @brief Returns the shared I2C master bus handle.
 *
 * @return Valid bus handle after successful initialization, or NULL when the
 *         bus has not been initialized.
 */
i2c_master_bus_handle_t app_i2c_bus_get_handle(void);

/**
 * @brief Adds a target device to the shared I2C bus.
 *
 * @param address Seven-bit I2C target address.
 * @param scl_speed_hz Device clock speed in hertz.
 * @param device_handle Output pointer that receives the new device handle.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t app_i2c_bus_add_device(uint16_t address,
                                 uint32_t scl_speed_hz,
                                 i2c_master_dev_handle_t *device_handle);

/**
 * @brief Removes a target device from the shared I2C bus.
 *
 * @param device_handle Device handle returned by app_i2c_bus_add_device().
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t app_i2c_bus_remove_device(i2c_master_dev_handle_t device_handle);

/**
 * @brief Probes one seven-bit I2C address.
 *
 * @param address Seven-bit I2C target address.
 * @param timeout_ms Probe timeout in milliseconds.
 *
 * @return ESP_OK when the target acknowledges. ESP_ERR_NOT_FOUND or a driver
 *         timeout indicates that no target responded.
 */
esp_err_t app_i2c_bus_probe(uint16_t address, int timeout_ms);

#endif
