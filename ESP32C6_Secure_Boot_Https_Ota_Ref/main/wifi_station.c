#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "app_cfg.h"
#include "wifi_station.h"

static const char *TAG = "wifi_station";

static EventGroupHandle_t s_wifi_evt;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;

static int s_retry_count;

/**
 * @brief Wi-Fi event handler 
 * 
 * @param arg The argument passed to the handler 
 * @param event_base The event base 
 * @param event_id The event ID 
 * @param event_data The event data 
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    // Handle Wi-Fi and IP events to start the Wi-Fi
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA start -> connect");
        esp_wifi_connect();
        return;
    }

    // Handle disconnection and retry logic
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < 10) {
            s_retry_count++;
            ESP_LOGW(TAG, "Disconnected, retry %d/10", s_retry_count);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Wi-Fi connect failed");
            xEventGroupSetBits(s_wifi_evt, WIFI_FAIL_BIT);
        }
        return;
    }

    // Handle successful IP acquisition
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        ESP_LOGI(TAG, "Got IP");
        xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED_BIT);
        return;
    }
}

/**
 * @brief Start Wi-Fi in station mode and connect to AP 
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise  
 */
esp_err_t wifi_station_start(void)
{
    // 
    if (strlen(APP_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "CONFIG_APP_WIFI_SSID is empty. Set it in menuconfig.");
        return ESP_ERR_INVALID_ARG;
    }

    // Create the event group to handle Wi-Fi events
    s_wifi_evt = xEventGroupCreate();
    if (!s_wifi_evt) {
        return ESP_ERR_NO_MEM;
    }

    // Initialize TCP/IP stack and Wi-Fi driver
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Configure Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Set Wi-Fi configuration and start
    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid, APP_WIFI_SSID, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, APP_WIFI_PASSWORD, sizeof(wifi_cfg.sta.password) - 1);

    // Set Wi-Fi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Set the Wi-Fi configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    
    // Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_evt,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(20000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Wi-Fi connection did not complete");
    return ESP_FAIL;
}