#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "driver/gpio.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"

#include "app_cfg.h"
#include "ota_manager.h"

static const char *TAG = "ota_mgr";

extern const char server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const char server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

/**
 * @brief Parse HTTPS host from URL
 * 
 * @param url URL string to parse
 * @param host_out Buffer to store extracted hostname
 * @param host_out_len Length of host_out buffer
 * @return int 0 on success, -1 on parse failure
 */
static int parse_https_host(const char *url, char *host_out, size_t host_out_len)
{
    // parse_https_host
    const char *p = strstr(url, "https://");
    if (!p) {
        return -1;
    }
    p += strlen("https://");

    const char *slash = strchr(p, '/');
    size_t host_len = slash ? (size_t)(slash - p) : strlen(p);
    if (host_len == 0 || host_len >= host_out_len) {
        return -1;
    }

    memcpy(host_out, p, host_len);
    host_out[host_len] = 0;
    return 0;
}

/**
 * @brief Check if system time is set 
 * 
 * @return true  if time is set, false otherwise
 */
static bool is_time_set(void)
{
    time_t now = 0;
    struct tm t = {0};

    // Set time structure to zero and get current time
    time(&now);  

    // Get local time
    localtime_r(&now, &t);

    // Return tru if year >= 2024
    return (t.tm_year + 1900) >= 2024;
}

/**
 * @brief Start SNTP client once 
 * 
 */
static void sntp_start_once(void)
{
    // Enable SNTP if not already enabled
    if (esp_sntp_enabled()) {
        return;
    }

    // Initialize SNTP with default config
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&cfg);
}

/** 
 * @brief Check if current time is within maintenance window
 */
static bool in_maintenance_window(void)
{
    // Check if time is set
    if (!is_time_set()) {
        return APP_OTA_ALLOW_NO_TIME ? true : false;
    }

    time_t now = 0;
    time(&now);
    struct tm t = {0};
    localtime_r(&now, &t);

    int start = APP_OTA_MAINT_START_HOUR;
    int end = APP_OTA_MAINT_END_HOUR;
    int h = t.tm_hour;

    if (start == end) {
        return true;
    }
    if (start < end) {
        return (h >= start) && (h < end);
    }
    return (h >= start) || (h < end);
}

/**
 * @brief Read battery voltage in millivolts 
 * 
 * @return int 
 */
static int read_battery_mv(void)
{
    /*
     * read_battery_mv
     *
     * Battery check stub. Replace with ADC logic for real hardware.
     */
    return APP_BATT_FAKE_MV;
}

/**
 * @brief Check if OTA button is pressed 
 * 
 * @return true if pressed, false otherwise 
 */
static bool ota_button_pressed(void)
{
    int level1 = gpio_get_level(APP_OTA_BUTTON_GPIO);
    vTaskDelay(pdMS_TO_TICKS(30));
    int level2 = gpio_get_level(APP_OTA_BUTTON_GPIO);
    return (level1 == 0) && (level2 == 0);
}

/** 
 * @brief Check if cloud trigger requests OTA 
 * 
 * @return true if requested, false otherwise 
 */
static bool cloud_trigger_requested(void)
{
    // Optional HTTPS trigger URL that returns '1' to request OTA.
    if (strlen(APP_OTA_TRIGGER_URL) == 0) {
        return false;
    }

    const char *cert_pem = server_root_cert_pem_start;
    esp_http_client_config_t cfg = {
        .url = APP_OTA_TRIGGER_URL,
        .cert_pem = cert_pem,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return false;
    }

    bool requested = false;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        char buf[8] = {0};
        int r = esp_http_client_read(client, buf, sizeof(buf) - 1);
        if (r > 0 && buf[0] == '1') {
            requested = true;
        }
        esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
    return requested;
}

/** 
 * @brief Check if network is ready for OTA 
 * 
 * @param https_url Firmware HTTPS URL
 * @return true if ready, false otherwise 
 */
static bool network_ready_check(const char *https_url)
{
    /*
     * network_ready_check
     *
     * Simple readiness check:
     * - Parse HTTPS host
     * - DNS resolve
     * - TCP connect to port 443
     */
    char host[128] = {0};
    if (parse_https_host(https_url, host, sizeof(host)) != 0) {
        return false;
    }

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;

    int err = getaddrinfo(host, "443", &hints, &res);
    if (err != 0 || !res) {
        return false;
    }

    int s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) {
        freeaddrinfo(res);
        return false;
    }

    bool ok = false;
    if (connect(s, res->ai_addr, res->ai_addrlen) == 0) {
        ok = true;
    }

    close(s);
    freeaddrinfo(res);
    return ok;
}

/**
 * @brief Perform HTTPS OTA update 
 * 
 * @param firmware_url Firmware HTTPS URL 
 * @return esp_err_t ESP_OK on success, error code otherwise  
 */
static esp_err_t https_ota_run(const char *firmware_url)
{
    // Performs HTTPS OTA with certificate pinning.
    const char *cert_pem = server_root_cert_pem_start;
    size_t cert_len = (size_t)(server_root_cert_pem_end - server_root_cert_pem_start);
    if (cert_len < 32) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Configure HTTPS OTA
    esp_http_client_config_t http_cfg = {
        .url = firmware_url,
        .cert_pem = cert_pem,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };

    // OTA configuration
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    // Start HTTPS OTA
    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    return err;
}

/** 
 * @brief OTA decision task 
 * 
 * @param arg Task argument (unused) 
 */
static void ota_decision_task(void *arg)
{
    // Gated OTA loop: user action, cloud command, maintenance window,
    // battery checks, and network readiness checks.

    (void)arg;

    // Start SNTP to get time
    sntp_start_once();

    // Configure OTA button GPIO
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << APP_OTA_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    while (1) {
        // Check for OTA request
        bool update_req = ota_button_pressed() || cloud_trigger_requested();
        if (!update_req) {
            vTaskDelay(pdMS_TO_TICKS(APP_OTA_POLL_PERIOD_MS));
            continue;
        }

        // Check maintenance window
        if (!in_maintenance_window()) {
            vTaskDelay(pdMS_TO_TICKS(APP_OTA_POLL_PERIOD_MS));
            continue;
        }

        // Check battery voltage
        int batt_mv = read_battery_mv();
        if (batt_mv < APP_BATT_MIN_MV) {
            vTaskDelay(pdMS_TO_TICKS(APP_OTA_POLL_PERIOD_MS));
            continue;
        }

        // Check network readiness
        if (!network_ready_check(APP_OTA_FIRMWARE_URL)) {
            vTaskDelay(pdMS_TO_TICKS(APP_OTA_POLL_PERIOD_MS));
            continue;
        }

        // All checks passed, perform HTTPS OTA
        (void)https_ota_run(APP_OTA_FIRMWARE_URL);
        vTaskDelay(pdMS_TO_TICKS(APP_OTA_POLL_PERIOD_MS));
    }
}

/**
 * @brief Start OTA manager 
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise 
 */
esp_err_t ota_manager_start(void)
{
    // Creates the OTA manager task.
    BaseType_t ok = xTaskCreate(ota_decision_task, "ota_decision", 8192, NULL, 5, NULL);
    return (ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}