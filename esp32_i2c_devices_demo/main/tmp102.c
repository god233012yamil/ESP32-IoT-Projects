#include "tmp102.h"

#include <string.h>

#include "esp_check.h"
#include "i2c_bus.h"

#define TMP102_TEMPERATURE_REGISTER 0x00
#define TMP102_TRANSACTION_TIMEOUT_MS 100

static const char *TAG = "tmp102";

/**
 * @brief Initializes the TMP102 temperature sensor.
 *
 * @param sensor Initialized sensor context.
 * @param address The 7-bit I2C address of the sensor.
 * @param scl_speed_hz The SCL speed in Hertz.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t tmp102_init(tmp102_t *sensor,
                      uint8_t address,
                      uint32_t scl_speed_hz)
{
    ESP_RETURN_ON_FALSE(sensor != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Sensor context is NULL");

    memset(sensor, 0, sizeof(*sensor));
    ESP_RETURN_ON_ERROR(app_i2c_bus_add_device(address,
                                                scl_speed_hz,
                                                &sensor->device_handle),
                        TAG,
                        "Failed to add TMP102 device");

    sensor->address = address;
    sensor->initialized = true;
    return ESP_OK;
}

/**
 * @brief Reads the temperature from the TMP102 sensor.
 *
 * @param sensor Initialized sensor context.
 * @param temperature_c Output pointer to the temperature in Celsius.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t tmp102_read_temperature_c(const tmp102_t *sensor,
                                    float *temperature_c)
{
    ESP_RETURN_ON_FALSE(sensor != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Sensor context is NULL");
    ESP_RETURN_ON_FALSE(sensor->initialized,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "TMP102 is not initialized");
    ESP_RETURN_ON_FALSE(temperature_c != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Temperature output pointer is NULL");

    const uint8_t register_address = TMP102_TEMPERATURE_REGISTER;
    uint8_t raw_bytes[2] = {0};

    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(sensor->device_handle,
                                                     &register_address,
                                                     sizeof(register_address),
                                                     raw_bytes,
                                                     sizeof(raw_bytes),
                                                     TMP102_TRANSACTION_TIMEOUT_MS),
                        TAG,
                        "Failed to read TMP102 temperature register");

    // The normal-mode result is a signed 12-bit value in bits 15 through 4.
    int16_t raw_temperature = (int16_t)(((uint16_t)raw_bytes[0] << 8) |
                                        raw_bytes[1]);
    raw_temperature >>= 4;

    // Sign-extend bit 11 because the value was shifted inside a 16-bit type.
    if ((raw_temperature & 0x0800) != 0) {
        raw_temperature |= (int16_t)0xF000;
    }

    *temperature_c = (float)raw_temperature * 0.0625f;
    return ESP_OK;
}

/**
 * @brief Deinitializes the TMP102 temperature sensor.
 *
 * @param sensor Initialized sensor context.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t tmp102_deinit(tmp102_t *sensor)
{
    ESP_RETURN_ON_FALSE(sensor != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Sensor context is NULL");

    if (!sensor->initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(app_i2c_bus_remove_device(sensor->device_handle),
                        TAG,
                        "Failed to remove TMP102 device");

    memset(sensor, 0, sizeof(*sensor));
    return ESP_OK;
}
