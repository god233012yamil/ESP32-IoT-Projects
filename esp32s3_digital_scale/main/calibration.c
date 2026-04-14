/**
 * @file calibration.c
 * @brief Calibration subsystem implementation.
 *
 * Implements NVS-backed persistence and an interactive UART calibration
 * wizard for the HX711 load-cell interface.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#include "calibration.h"
#include "scale_config.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "CALIBRATION";

/* ── Private helpers ──────────────────────────────────────────────── */

/**
 * @brief Read a line of text from stdin (blocking, UART console).
 *
 * Accumulates characters until newline or the buffer is full.
 * The newline character is not stored.
 *
 * @param[out] buf    Destination buffer.
 * @param[in]  buflen Maximum number of characters to store (incl. NUL).
 */
static void read_line(char *buf, size_t buflen)
{
    size_t pos = 0;
    int c;
    while (pos < buflen - 1) {
        c = getchar();
        if (c == '\n' || c == '\r') break;
        if (c != EOF) buf[pos++] = (char)c;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    buf[pos] = '\0';
}

/* ── Public API ───────────────────────────────────────────────────── */

/**
 * @brief Run the interactive UART calibration wizard.
 *
 * Step 1 – Tare: prompts operator to clear the platform, then captures
 *           CONFIG_TARE_SAMPLES readings as the zero reference.
 * Step 2 – Scale: prompts operator to enter the reference weight in
 *           grams and place it on the platform, then measures and
 *           computes scale = (avg_loaded − tare) / reference_grams.
 * Step 3 – Save: calls calibration_save() to persist to NVS.
 */
esp_err_t calibration_run(hx711_dev_t *dev)
{
    char buf[32];

    printf("\n========================================\n");
    printf("      Digital Scale Calibration Wizard  \n");
    printf("========================================\n\n");

    /* ── Step 1: Tare ── */
    printf("[1/3] Remove ALL weight from the platform.\n");
    printf("      Press ENTER when ready...\n");
    read_line(buf, sizeof(buf));

    printf("      Capturing tare (%d samples)...\n", CONFIG_TARE_SAMPLES);
    esp_err_t err = hx711_tare(dev, CONFIG_TARE_SAMPLES);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Tare failed: %s", esp_err_to_name(err));
        return err;
    }
    printf("      Tare captured: %" PRId32 " raw counts\n\n", dev->tare);

    /* ── Step 2: Known weight ── */
    printf("[2/3] Enter the reference weight in grams (e.g. 1000): ");
    fflush(stdout);
    read_line(buf, sizeof(buf));

    float ref_grams = 0.0f;
    if (sscanf(buf, "%f", &ref_grams) != 1 || ref_grams <= 0.0f) {
        ESP_LOGE(TAG, "Invalid reference weight: '%s'", buf);
        return ESP_ERR_INVALID_ARG;
    }

    printf("      Place the %.1f g reference weight on the platform.\n", ref_grams);
    printf("      Press ENTER when ready...\n");
    read_line(buf, sizeof(buf));

    printf("      Measuring (%d samples)...\n", CONFIG_TARE_SAMPLES);
    int32_t avg_loaded = 0;
    err = hx711_read_average(dev, CONFIG_TARE_SAMPLES, &avg_loaded);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Measurement failed: %s", esp_err_to_name(err));
        return err;
    }

    int32_t delta = avg_loaded - dev->tare;
    if (delta == 0) {
        ESP_LOGE(TAG, "No change detected; check wiring.");
        return ESP_FAIL;
    }

    float new_scale = (float)delta / ref_grams;
    err = hx711_set_scale(dev, new_scale);
    if (err != ESP_OK) return err;

    printf("      Loaded avg : %" PRId32 " raw counts\n", avg_loaded);
    printf("      Delta      : %" PRId32 " raw counts\n", delta);
    printf("      Scale factor: %.4f counts/g\n\n", new_scale);

    /* ── Step 3: Save ── */
    printf("[3/3] Saving calibration to NVS...\n");
    err = calibration_save(dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
        return err;
    }

    printf("      Calibration saved successfully!\n");
    printf("========================================\n\n");
    return ESP_OK;
}

/**
 * @brief Load calibration from NVS and apply to the HX711 device.
 */
esp_err_t calibration_load(hx711_dev_t *dev)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No calibration namespace in NVS; using defaults.");
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (err != ESP_OK) return err;

    /* Scale factor stored as raw uint32 via bit-cast */
    uint32_t scale_bits = 0;
    err = nvs_get_u32(nvs, CONFIG_NVS_KEY_SCALE, &scale_bits);
    if (err == ESP_OK) {
        float scale;
        memcpy(&scale, &scale_bits, sizeof(float));
        hx711_set_scale(dev, scale);
    }

    int32_t tare = 0;
    esp_err_t err2 = nvs_get_i32(nvs, CONFIG_NVS_KEY_TARE, &tare);
    if (err2 == ESP_OK) {
        dev->tare = tare;
    }

    nvs_close(nvs);

    if (err == ESP_OK && err2 == ESP_OK) {
        ESP_LOGI(TAG, "Calibration loaded: scale=%.4f  tare=%" PRId32,
                 dev->scale, dev->tare);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Partial calibration data in NVS; using defaults for missing keys.");
    return ESP_ERR_NVS_NOT_FOUND;
}

/**
 * @brief Persist scale factor and tare offset to NVS.
 */
esp_err_t calibration_save(const hx711_dev_t *dev)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    uint32_t scale_bits;
    memcpy(&scale_bits, &dev->scale, sizeof(float));
    err = nvs_set_u32(nvs, CONFIG_NVS_KEY_SCALE, scale_bits);
    if (err != ESP_OK) { nvs_close(nvs); return err; }

    err = nvs_set_i32(nvs, CONFIG_NVS_KEY_TARE, dev->tare);
    if (err != ESP_OK) { nvs_close(nvs); return err; }

    err = nvs_commit(nvs);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Calibration saved: scale=%.4f  tare=%" PRId32,
                 dev->scale, dev->tare);
    }
    return err;
}

/**
 * @brief Erase calibration keys from NVS.
 */
esp_err_t calibration_erase(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    nvs_erase_key(nvs, CONFIG_NVS_KEY_SCALE);
    nvs_erase_key(nvs, CONFIG_NVS_KEY_TARE);
    err = nvs_commit(nvs);
    nvs_close(nvs);

    ESP_LOGI(TAG, "Calibration data erased from NVS");
    return err;
}
