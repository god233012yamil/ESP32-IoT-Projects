#include "ble_long_range.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "app_main";

/**
 * Application entry point.
 *
 * Initializes the BLE long-range demonstration. The Bluetooth stack handles
 * all subsequent activity through registered GAP and GATT callbacks.
 */
void app_main(void)
{
    ESP_ERROR_CHECK(ble_long_range_init());
    ESP_LOGI(TAG, "ESP32-S3 BLE long-range server initialized");
}
