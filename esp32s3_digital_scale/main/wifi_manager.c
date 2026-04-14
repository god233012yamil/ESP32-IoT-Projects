/**
 * @file wifi_manager.c
 * @brief Wi-Fi STA connection manager implementation.
 *
 * Uses the ESP-IDF event-loop and FreeRTOS EventGroup to provide a
 * blocking connect() call that waits for an IP address.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#include "wifi_manager.h"
#include "scale_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI";

/* EventGroup bits */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;
static int                s_retry_count       = 0;
static bool               s_connected         = false;
static char               s_ip_str[16]        = "0.0.0.0";
static esp_netif_t       *s_netif             = NULL;

/* ── Event handler ────────────────────────────────────────────────── */

/**
 * @brief Unified event handler for Wi-Fi and IP events.
 *
 * Handles WIFI_EVENT_STA_START (triggers connect), WIFI_EVENT_STA_DISCONNECTED
 * (triggers reconnect or sets fail bit), and IP_EVENT_STA_GOT_IP (sets the
 * connected bit and caches the IP string).
 *
 * @param arg       Unused user argument.
 * @param event_base Event base identifier (WIFI_EVENT or IP_EVENT).
 * @param event_id  Specific event within the base.
 * @param event_data Event-specific payload (e.g., ip_event_got_ip_t).
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        snprintf(s_ip_str, sizeof(s_ip_str), "0.0.0.0");

        if (s_retry_count < CONFIG_WIFI_MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGW(TAG, "Reconnecting (%d/%d)...", s_retry_count, CONFIG_WIFI_MAX_RETRIES);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Max retries reached – giving up");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_ip_str);
        s_retry_count = 0;
        s_connected   = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ── Public API ───────────────────────────────────────────────────── */

/**
 * @brief Initialise TCP/IP stack, default STA netif, and Wi-Fi driver.
 */
esp_err_t wifi_manager_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    err = esp_netif_init();
    if (err != ESP_OK) return err;

    err = esp_event_loop_create_default();
    if (err != ESP_OK) return err;

    s_netif = esp_netif_create_default_wifi_sta();
    if (!s_netif) return ESP_FAIL;

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Wi-Fi driver initialised");
    return ESP_OK;
}

/**
 * @brief Connect to the configured SSID (blocking until IP or failure).
 */
esp_err_t wifi_manager_connect(void)
{
    s_wifi_event_group = xEventGroupCreate();
    s_retry_count      = 0;

    esp_event_handler_instance_t h_wifi, h_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_event_handler, NULL, &h_wifi);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        wifi_event_handler, NULL, &h_ip);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "Connecting to SSID: %s", CONFIG_WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           portMAX_DELAY);

    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, h_wifi);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, h_ip);
    vEventGroupDelete(s_wifi_event_group);
    s_wifi_event_group = NULL;

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected. IP: %s", s_ip_str);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to Wi-Fi");
    return ESP_FAIL;
}

/**
 * @brief Return true when the station has an IP address.
 */
bool wifi_manager_is_connected(void)
{
    return s_connected;
}

/**
 * @brief Return the cached IPv4 address string.
 */
const char *wifi_manager_get_ip(void)
{
    return s_ip_str;
}

/**
 * @brief Disconnect and stop the Wi-Fi driver.
 */
esp_err_t wifi_manager_deinit(void)
{
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    s_connected = false;
    ESP_LOGI(TAG, "Wi-Fi stopped");
    return ESP_OK;
}
