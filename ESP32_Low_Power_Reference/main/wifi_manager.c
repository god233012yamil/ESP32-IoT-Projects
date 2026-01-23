#include "wifi_manager.h"

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_mgr";

// Event bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif;
static int s_retry_num;

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 3) {
            s_retry_num++;
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_manager_init_once(void)
{
    static bool initialized = false;
    if (initialized) {
        return ESP_OK;
    }

    // NVS is required for Wi-Fi.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_flash_init failed");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    initialized = true;
    return ESP_OK;
}

esp_err_t wifi_manager_connect(uint32_t timeout_ms)
{
#ifndef CONFIG_LP_ENABLE_WIFI
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_ERROR(wifi_manager_init_once(), TAG, "wifi init failed");

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_retry_num = 0;

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, CONFIG_LP_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_LP_WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // For connected-but-idle periods, enable modem sleep.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));

    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected");
        return ESP_OK;
    }

    if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "connect failed");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "connect timeout");
    return ESP_ERR_TIMEOUT;
#endif
}

esp_err_t wifi_manager_demo_tx(const char *host, uint16_t port, uint32_t timeout_ms)
{
#ifndef CONFIG_LP_ENABLE_WIFI
    (void)host;
    (void)port;
    (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (host == NULL || host[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char port_str[8] = {0};
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGW(TAG, "getaddrinfo failed (%d)", err);
        return ESP_FAIL;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return ESP_FAIL;
    }

    // Set connect timeout.
    struct timeval tv = {0};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int rc = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (rc != 0) {
        close(sock);
        ESP_LOGW(TAG, "TCP connect failed");
        return ESP_FAIL;
    }

    close(sock);
    ESP_LOGI(TAG, "demo tx ok");
    return ESP_OK;
#endif
}

void wifi_manager_shutdown(void)
{
#ifndef CONFIG_LP_ENABLE_WIFI
    return;
#else
    // Best-effort shutdown; ignore errors to keep code simple.
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    // Leave event loop and netif in place for simplicity. In a strict
    // connect-once-per-boot deep sleep design, this is acceptable.
#endif
}
