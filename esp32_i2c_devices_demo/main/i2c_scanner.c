#include "i2c_scanner.h"

#include "esp_check.h"
#include "esp_log.h"
#include "i2c_bus.h"

#define I2C_FIRST_USABLE_ADDRESS 0x08
#define I2C_LAST_USABLE_ADDRESS  0x77
#define I2C_PROBE_TIMEOUT_MS      30

static const char *TAG = "i2c_scanner";

/**
 * @brief Scans the I2C bus for devices.
 *
 * @param addresses Output array to store the detected addresses.
 * @param capacity The maximum number of addresses to store.
 * @param found_count Output pointer to the number of found devices.
 * @return ESP_OK if successful, otherwise an error code.
 */
esp_err_t i2c_scanner_scan(uint8_t *addresses,
                           size_t capacity,
                           size_t *found_count)
{
    ESP_RETURN_ON_FALSE(found_count != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "found_count is NULL");

    *found_count = 0;
    ESP_LOGI(TAG, "Scanning I2C addresses 0x08 through 0x77");

    for (uint16_t address = I2C_FIRST_USABLE_ADDRESS;
         address <= I2C_LAST_USABLE_ADDRESS;
         ++address) {
        const esp_err_t result = app_i2c_bus_probe(address,
                                                   I2C_PROBE_TIMEOUT_MS);

        if (result == ESP_OK) {
            if ((addresses != NULL) && (*found_count < capacity)) {
                addresses[*found_count] = (uint8_t)address;
            }

            ++(*found_count);
            ESP_LOGI(TAG, "Found device at 0x%02X", address);
        }
    }

    if (*found_count == 0) {
        ESP_LOGW(TAG, "No I2C devices responded");
    } else {
        ESP_LOGI(TAG, "Scan complete: %u device(s)", (unsigned)*found_count);
    }

    return ESP_OK;
}
