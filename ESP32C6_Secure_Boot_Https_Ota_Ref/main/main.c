#include "esp_log.h"
#include "esp_secure_boot.h"
#include "esp_flash_encrypt.h"
#include "nvs_flash.h"

#include "app_cfg.h"
#include "wifi_station.h"
#include "ota_manager.h"

static const char *TAG = "app_main";

/**
 * @brief Log Secure Boot and Flash Encryption state 
 * 
 */
static void log_security_state(void)
{    
    ESP_LOGI(TAG, "Secure Boot enabled: %s", esp_secure_boot_enabled() ? "YES" : "NO");
    ESP_LOGI(TAG, "Flash Encryption enabled: %s", esp_flash_encryption_enabled() ? "YES" : "NO");
}

/**
 * @brief Platform initialization
 * 
 */
static void platform_init(void)
{
    // Initialize NVS, required for Wi-Fi and other components 
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    // Start Wi-Fi station. Wi-Fi station connect (SSID/password from menuconfig)
    ESP_ERROR_CHECK(wifi_station_start());
}

/**
 * @brief Application main entry point
 * 
 */
void app_main(void)
{
    // Initialize platform (NVS, Wi-Fi)
    platform_init();

    // Log Secure Boot and Flash Encryption state
    log_security_state();

    // Start OTA manager task
    ESP_ERROR_CHECK(ota_manager_start());

    ESP_LOGI(TAG, "Ready. Press the OTA button (GPIO %d) or configure a trigger URL.", APP_OTA_BUTTON_GPIO);
}