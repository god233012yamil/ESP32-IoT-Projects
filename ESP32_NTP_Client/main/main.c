/*
 * @file main.c
 * @brief ESP32 NTP Client: connect to Wi-Fi, sync time via SNTP, and print every second.
 *
 * @details
 * This example demonstrates how to:
 *   1) Initialize NVS and the default event loop
 *   2) Bring up Wi-Fi in station mode and wait for an IP
 *   3) Configure and start SNTP to synchronize the system clock
 *   4) Spawn a FreeRTOS task that prints the current local time every second
 *
 * The code uses the ESP-IDF Wi-Fi station example pattern with retry logic, an
 * event group for synchronization, and the modern SNTP API (`esp_sntp.h`).
 * Once SNTP completes the first sync, we set/confirm the timezone (from Kconfig)
 * and keep printing human-readable timestamps.
 *
 * @section Build_Run Build & Run
 * - Set your Wi-Fi credentials and timezone in `menuconfig`:
 *     Example Configuration → WiFi SSID
 *     Example Configuration → WiFi Password
 *     Example Configuration → Timezone (POSIX format, e.g., "EST5EDT,M3.2.0/2,M11.1.0/2")
 * - Flash and monitor: `idf.py -p <PORT> flash monitor`
 *
 * @section Notes Notes
 * - Timezone must be a valid POSIX TZ string. For example:
 *     "UTC0" for UTC,
 *     "EST5EDT,M3.2.0/2,M11.1.0/2" for US Eastern with DST.
 * - The SNTP servers can be customized (e.g., regional pool servers or a local NTP).
 * - Printing uses `localtime_r()`; if you need UTC, use `gmtime_r()`.
 */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_err.h"

#include "esp_sntp.h"   // Modern SNTP API header in ESP-IDF

// ---------- Kconfig bindings ----------
#define WIFI_SSID       CONFIG_WIFI_SSID
#define WIFI_PASS       CONFIG_WIFI_PASSWORD
#define EXAMPLE_TZ      CONFIG_TIMEZONE
#define MAX_RETRY       10

// ---------- Event group bits ----------
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1

static const char *TAG = "NTP_APP";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/**
 * @brief Wi-Fi event/IP handlers for station mode connection management.
 *
 * @details
 * - On WIFI_EVENT_STA_START → esp_wifi_connect()
 * - On WIFI_EVENT_STA_DISCONNECTED → retry up to MAX_RETRY times
 * - On IP_EVENT_STA_GOT_IP → set WIFI_CONNECTED_BIT
 *
 * @param arg     User argument (unused)
 * @param event_base Event loop base (Wi-Fi/IP)
 * @param event_id   Event ID
 * @param event_data Opaque pointer to event-specific data
 */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying to connect to the AP... (%d/%d)", s_retry_num, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGW(TAG, "Disconnected from AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize Wi-Fi in station (STA) mode and block until IP acquired or fail.
 *
 * @details
 * Creates default netif, registers Wi-Fi/IP event handlers, initializes the Wi-Fi
 * driver, sets the SSID/PASS, starts STA, and waits on an event group for either
 * `WIFI_CONNECTED_BIT` or `WIFI_FAIL_BIT`. This function returns when the station
 * is connected (IP assigned) or when retries are exhausted.
 *
 * @return esp_err_t
 * - ESP_OK when connected and IP acquired
 * - ESP_FAIL if connection could not be established after retries
 */
static esp_err_t wifi_init_and_wait_ip(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;  // adjust if your AP differs
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;          // compatible setting

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi STA started, connecting to SSID:\"%s\"", WIFI_SSID);

    // Wait until either the connection is established (WIFI_CONNECTED_BIT) or connection failed
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to connect to AP after %d retries", MAX_RETRY);
        return ESP_FAIL;
    }
}

/**
 * @brief SNTP time sync callback for logging.
 *
 * @details
 * This callback is invoked by the SNTP client on time synchronization events,
 * providing visibility into when the system clock gets updated.
 *
 * @param tv   Pointer to timeval with the synchronized time
 */
static void time_sync_notification_cb(struct timeval *tv)
{
    (void) tv;
    ESP_LOGI(TAG, "Time synchronization event received");
}

/**
 * @brief Configure and start SNTP, then wait until the system time is valid.
 *
 * @details
 * - Configures SNTP in polling mode with one or more servers (pool.ntp.org by default).
 * - Registers a notification callback (optional).
 * - Starts SNTP and waits until the time is synchronized or a timeout elapses.
 * - Sets the timezone (POSIX TZ string) from Kconfig, then calls tzset().
 *
 * @param wait_seconds  Maximum number of seconds to wait for first sync (e.g., 30).
 * @return esp_err_t
 * - ESP_OK on successful sync
 * - ESP_ERR_TIMEOUT if time not set within the wait window
 */
static esp_err_t sntp_start_and_wait(int wait_seconds)
{
    // Configure servers (you can add regional servers for faster response)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL); 
    
    // Set the primary SNTP server to pool.ntp.org (you can customize this to your preferred NTP server)
    esp_sntp_setservername(0, "pool.ntp.org"); 
    // Optionally set multiple:
    // sntp_setservername(1, "time.google.com");
    // sntp_setservername(2, "pool.ntp.org");

    // Register the time sync notification callback
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    
    // Set the sync mode to immediate for the first synchronization
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);   // immediate adjustment for first sync
    
    // Start SNTP
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP started, waiting for time sync...");

    // Wait until time is set (tm_year >= 2020 - 1900 is a common heuristic)
    const time_t start = time(NULL);
    while (true) {
        time_t now = time(NULL);
        struct tm timeinfo = {0};

        // Convert to local time (use gmtime_r() for UTC)
        localtime_r(&now, &timeinfo);
        // Check if the year is valid (2020 or later), indicating that time has been synchronized
        if (timeinfo.tm_year >= (2020 - 1900)) {
            ESP_LOGI(TAG, "Time is synchronized: %04d-%02d-%02d %02d:%02d:%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            break;
        }
        // Check for timeout condition
        if ((int)difftime(now, start) >= wait_seconds) {
            ESP_LOGE(TAG, "Timeout waiting for time sync");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Set timezone (POSIX TZ) and apply
    setenv("TZ", EXAMPLE_TZ, 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to: %s", EXAMPLE_TZ);

    return ESP_OK;
}

/**
 * @brief FreeRTOS task that prints the local time every second.
 *
 * @details
 * Loops forever, reading the current system time, converting to local time, and
 * printing in a human-readable ISO-like format. If you need UTC, switch to `gmtime_r()`.
 *
 * @param pvParameters Unused
 */
static void print_time_task(void *pvParameters)
{
    (void) pvParameters;
    char buf[64];

    while (true) {
        time_t now = time(NULL);
        struct tm local = {0};

        // Convert to local time (use gmtime_r() for UTC)
        localtime_r(&now, &local);
        // Format time as YYYY-MM-DD HH:MM:SS TZ. 24-hour format with AM/PM,
        // strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &local);
        // Format time as YYYY-MM-DD hh:MM:SS AM/PM TZ. 12-hour format with AM/PM,
        strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S %p %Z", &local);

        printf("[TIME] %s\n", buf);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Application entry point: NVS init, Wi-Fi connect, SNTP sync, spawn printing task.
 *
 * @details
 * - Initializes NVS (required by Wi-Fi)
 * - Connects to Wi-Fi and waits for IP
 * - Starts SNTP and waits for the first synchronization
 * - Creates the `print_time_task`
 */
void app_main(void)
{
    // Initialize NVS (Wi-Fi requires it)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Connect to Wi-Fi
    ESP_ERROR_CHECK(wifi_init_and_wait_ip());

    // Start SNTP and wait for time
    ESP_ERROR_CHECK(sntp_start_and_wait(30));  // wait up to 30s for first sync

    // Spawn the periodic printer
    xTaskCreate(print_time_task, "print_time_task", 3072, NULL, 5, NULL);
}