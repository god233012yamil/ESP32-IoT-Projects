#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "eeprom_24lc256.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "i2c_scanner.h"
#include "sdkconfig.h"
#include "ssd1306.h"
#include "tmp102.h"

#define MAX_SCANNED_DEVICES 16
#define SENSOR_READ_INTERVAL_MS 2000

static const char *TAG = "app";

#if CONFIG_APP_ENABLE_TMP102_DEMO
static tmp102_t s_tmp102;
static bool s_tmp102_available;
#endif

#if CONFIG_APP_ENABLE_SSD1306_DEMO
static ssd1306_t s_ssd1306;
#endif

#if CONFIG_APP_ENABLE_EEPROM_DEMO
static eeprom_24lc256_t s_eeprom;
#endif

/**
 * @brief Checks whether an address was found by the bus scanner.
 *
 * @param addresses Array of detected seven-bit addresses.
 * @param address_count Number of valid entries in the array.
 * @param target_address Address to search for.
 *
 * @return True when the target address is present in the array.
 */
static bool address_was_detected(const uint8_t *addresses,
                                 size_t address_count,
                                 uint8_t target_address)
{
    if (addresses == NULL) {
        return false;
    }

    for (size_t index = 0; index < address_count; ++index) {
        if (addresses[index] == target_address) {
            return true;
        }
    }

    return false;
}

#if CONFIG_APP_ENABLE_TMP102_DEMO
/**
 * @brief Initializes and reads the optional TMP102 demonstration device.
 *
 * @param detected True when the configured TMP102 address acknowledged during
 *        the bus scan.
 */
static void run_tmp102_demo(bool detected)
{
    if (!detected) {
        ESP_LOGW(TAG,
                 "TMP102 demo skipped: no device at 0x%02X",
                 CONFIG_APP_TMP102_ADDRESS);
        return;
    }

    esp_err_t result = tmp102_init(&s_tmp102,
                                   CONFIG_APP_TMP102_ADDRESS,
                                   CONFIG_APP_I2C_CLOCK_HZ);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "TMP102 initialization failed: %s",
                 esp_err_to_name(result));
        return;
    }

    float temperature_c = 0.0f;
    result = tmp102_read_temperature_c(&s_tmp102, &temperature_c);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "TMP102 temperature: %.2f C", temperature_c);
        s_tmp102_available = true;
    } else {
        ESP_LOGE(TAG, "TMP102 read failed: %s", esp_err_to_name(result));
    }
}
#endif

#if CONFIG_APP_ENABLE_SSD1306_DEMO
/**
 * @brief Initializes the optional SSD1306 and displays a test pattern.
 *
 * @param detected True when the configured SSD1306 address acknowledged
 *        during the bus scan.
 */
static void run_ssd1306_demo(bool detected)
{
    if (!detected) {
        ESP_LOGW(TAG,
                 "SSD1306 demo skipped: no device at 0x%02X",
                 CONFIG_APP_SSD1306_ADDRESS);
        return;
    }

    esp_err_t result = ssd1306_init(&s_ssd1306,
                                    CONFIG_APP_SSD1306_ADDRESS,
                                    CONFIG_APP_I2C_CLOCK_HZ);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 initialization failed: %s",
                 esp_err_to_name(result));
        return;
    }

    ssd1306_draw_demo_pattern(&s_ssd1306);
    result = ssd1306_refresh(&s_ssd1306);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "SSD1306 demonstration pattern displayed");
    } else {
        ESP_LOGE(TAG, "SSD1306 refresh failed: %s",
                 esp_err_to_name(result));
    }
}
#endif

#if CONFIG_APP_ENABLE_EEPROM_DEMO
/**
 * @brief Performs a non-destructive EEPROM write and readback test.
 *
 * The function saves the original bytes, writes a known pattern, verifies the
 * pattern, and restores the original bytes before returning.
 *
 * @param detected True when the configured EEPROM address acknowledged during
 *        the bus scan.
 */
static void run_eeprom_demo(bool detected)
{
    static const uint8_t test_pattern[] = {
        'E', 'S', 'P', '3', '2', '-', 'I', '2', 'C', '-', 'O', 'K'
    };
    uint8_t original_data[sizeof(test_pattern)] = {0};
    uint8_t readback_data[sizeof(test_pattern)] = {0};

    if (!detected) {
        ESP_LOGW(TAG,
                 "EEPROM demo skipped: no device at 0x%02X",
                 CONFIG_APP_EEPROM_ADDRESS);
        return;
    }

    esp_err_t result = eeprom_24lc256_init(&s_eeprom,
                                           CONFIG_APP_EEPROM_ADDRESS,
                                           CONFIG_APP_I2C_CLOCK_HZ);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "EEPROM initialization failed: %s",
                 esp_err_to_name(result));
        return;
    }

    result = eeprom_24lc256_read(&s_eeprom,
                                 CONFIG_APP_EEPROM_TEST_ADDRESS,
                                 original_data,
                                 sizeof(original_data));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save EEPROM test region: %s",
                 esp_err_to_name(result));
        return;
    }

    result = eeprom_24lc256_write(&s_eeprom,
                                  CONFIG_APP_EEPROM_TEST_ADDRESS,
                                  test_pattern,
                                  sizeof(test_pattern));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "EEPROM test write failed: %s",
                 esp_err_to_name(result));
        return;
    }

    result = eeprom_24lc256_read(&s_eeprom,
                                 CONFIG_APP_EEPROM_TEST_ADDRESS,
                                 readback_data,
                                 sizeof(readback_data));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "EEPROM test readback failed: %s",
                 esp_err_to_name(result));
        return;
    }

    if (memcmp(test_pattern, readback_data, sizeof(test_pattern)) == 0) {
        ESP_LOGI(TAG, "24LC256 write and readback test passed");
    } else {
        ESP_LOGE(TAG, "24LC256 readback data did not match the test pattern");
    }

    // Restore the saved bytes so the demonstration is non-destructive.
    result = eeprom_24lc256_write(&s_eeprom,
                                  CONFIG_APP_EEPROM_TEST_ADDRESS,
                                  original_data,
                                  sizeof(original_data));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Original EEPROM bytes restored");
    } else {
        ESP_LOGE(TAG, "Failed to restore original EEPROM bytes: %s",
                 esp_err_to_name(result));
    }
}
#endif

/**
 * @brief Periodically reads the TMP102 after the startup demonstrations.
 */
static void run_periodic_tasks(void)
{
    while (true) {
#if CONFIG_APP_ENABLE_TMP102_DEMO
        if (s_tmp102_available) {
            float temperature_c = 0.0f;
            const esp_err_t result = tmp102_read_temperature_c(&s_tmp102,
                                                                &temperature_c);
            if (result == ESP_OK) {
                ESP_LOGI(TAG, "Temperature: %.2f C", temperature_c);
            } else {
                ESP_LOGE(TAG, "Periodic TMP102 read failed: %s",
                         esp_err_to_name(result));
            }
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

/**
 * @brief Application entry point.
 *
 * The application initializes the shared bus, scans all usable addresses, and
 * runs demonstrations for a TMP102 sensor, SSD1306 display, and 24LC256 EEPROM
 * when those devices are enabled and detected.
 */
void app_main(void)
{
    uint8_t detected_addresses[MAX_SCANNED_DEVICES] = {0};
    size_t detected_count = 0;

    // Initialize the shared I2C bus and scan for devices.
    ESP_ERROR_CHECK(app_i2c_bus_init());
    
    // Scan the bus and store the detected addresses in the array.
    ESP_ERROR_CHECK(i2c_scanner_scan(detected_addresses,
                                     MAX_SCANNED_DEVICES,
                                     &detected_count));

    const size_t stored_count = (detected_count < MAX_SCANNED_DEVICES)
                                    ? detected_count
                                    : MAX_SCANNED_DEVICES;

#if CONFIG_APP_ENABLE_TMP102_DEMO
    // Run the TMP102 demonstration if the device was detected.
    run_tmp102_demo(address_was_detected(detected_addresses,
                                         stored_count,
                                         CONFIG_APP_TMP102_ADDRESS));
#endif

#if CONFIG_APP_ENABLE_SSD1306_DEMO
    // Run the SSD1306 demonstration if the device was detected.
    run_ssd1306_demo(address_was_detected(detected_addresses,
                                          stored_count,
                                          CONFIG_APP_SSD1306_ADDRESS));
#endif

#if CONFIG_APP_ENABLE_EEPROM_DEMO
    // Run the 24LC256 demonstration if the device was detected.
    run_eeprom_demo(address_was_detected(detected_addresses,
                                         stored_count,
                                         CONFIG_APP_EEPROM_ADDRESS));
#endif

    ESP_LOGI(TAG, "Startup demonstrations complete");
    run_periodic_tasks();
}
