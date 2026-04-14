/**
 * @file main.c
 * @brief ESP32-S3 Digital Scale – application entry point.
 *
 * Boot sequence:
 *   1. Initialise NVS flash.
 *   2. Initialise and connect Wi-Fi.
 *   3. Initialise the HX711 driver and load saved calibration from NVS.
 *   4. Perform an initial tare.
 *   5. Start the HTTP / SSE web server.
 *   6. Launch the measurement task (reads HX711, pushes SSE events).
 *
 * Two FreeRTOS tasks run concurrently after boot:
 *   - task_measure  : reads weight and pushes SSE updates.
 *   - (HTTP server) : handled internally by esp_http_server on its own
 *                     task pool.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "scale_config.h"
#include "hx711.h"
#include "calibration.h"
#include "wifi_manager.h"
#include "web_server.h"

static const char *TAG = "MAIN";

/* Global HX711 device handle shared between tasks */
static hx711_dev_t s_hx711;

/* ── Tasks ────────────────────────────────────────────────────────── */

/**
 * @brief Weight measurement task.
 *
 * Runs at CONFIG_MEASURE_TASK_PRIORITY, waking every
 * CONFIG_MEASURE_INTERVAL_MS milliseconds.  Each iteration:
 *   1. Reads CONFIG_SCALE_SAMPLES averaged ADC counts.
 *   2. Converts to grams (calibration already applied).
 *   3. Clamps readings below CONFIG_ZERO_THRESHOLD_G to 0.
 *   4. Logs the value to the UART console.
 *   5. Pushes the value to any connected SSE browser clients.
 *
 * @param[in] pvParam Unused task parameter.
 */
static void task_measure(void *pvParam)
{
    ESP_LOGI(TAG, "Measurement task started");

    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        float grams = 0.0f;
        esp_err_t err = hx711_get_weight(&s_hx711, CONFIG_SCALE_SAMPLES, &grams);

        if (err == ESP_OK) {
            /* Zero-clamp noise near tare */
            if (grams < CONFIG_ZERO_THRESHOLD_G && grams > -CONFIG_ZERO_THRESHOLD_G) {
                grams = 0.0f;
            }

            ESP_LOGI(TAG, "Weight: %.2f g  (%.3f kg  /  %.3f lb)",
                     grams, grams / 1000.0f, grams / 453.592f);

            web_server_push_weight(grams);
        } else {
            ESP_LOGW(TAG, "HX711 read error: %s", esp_err_to_name(err));
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_MEASURE_INTERVAL_MS));
    }
}

/* ── app_main ─────────────────────────────────────────────────────── */

/**
 * @brief Application entry point called by the ESP-IDF startup code.
 *
 * Orchestrates the full system initialisation sequence described in the
 * file-level docstring, then launches the measurement task and returns.
 * The FreeRTOS scheduler continues running the tasks indefinitely.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3 Digital Scale v1.0.0 ===");

    /* ── 1. NVS ── */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated/upgraded – erasing");
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* ── 2. Wi-Fi ── */
    ESP_ERROR_CHECK(wifi_manager_init());
    err = wifi_manager_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed. Dashboard will not be available.");
        /* Continue without Wi-Fi so the scale still works locally */
    } else {
        ESP_LOGI(TAG, "Wi-Fi OK – IP: %s", wifi_manager_get_ip());
    }

    /* ── 3. HX711 ── */
    hx711_config_t hx_cfg = {
        .dout_pin = CONFIG_HX711_DOUT_GPIO,
        .sck_pin  = CONFIG_HX711_SCK_GPIO,
        .gain     = HX711_GAIN_A_128,
    };
    ESP_ERROR_CHECK(hx711_init(&s_hx711, &hx_cfg));

    /* Load calibration from NVS, fall back to compile-time defaults */
    err = calibration_load(&s_hx711);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No saved calibration – applying compile-time defaults");
        hx711_set_scale(&s_hx711, CONFIG_SCALE_FACTOR);
    }

    /* ── 4. Initial tare ── */
    ESP_LOGI(TAG, "Performing initial tare (%d samples)…", CONFIG_TARE_SAMPLES);
    vTaskDelay(pdMS_TO_TICKS(500)); /* let HX711 settle */
    ESP_ERROR_CHECK(hx711_tare(&s_hx711, CONFIG_TARE_SAMPLES));

    /* ── 5. Web server ── */
    if (wifi_manager_is_connected()) {
        ESP_ERROR_CHECK(web_server_start(&s_hx711));
        ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        ESP_LOGI(TAG, "  Dashboard: http://%s/", wifi_manager_get_ip());
        ESP_LOGI(TAG, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    }

    /* ── 6. Measurement task ── */
    xTaskCreate(task_measure, "scale_measure",
                CONFIG_MEASURE_TASK_STACK,
                NULL,
                CONFIG_MEASURE_TASK_PRIORITY,
                NULL);

    ESP_LOGI(TAG, "System ready.");
}
