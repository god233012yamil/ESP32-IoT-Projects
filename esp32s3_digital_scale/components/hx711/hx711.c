/**
 * @file hx711.c
 * @brief HX711 24-bit ADC driver implementation for ESP32-S3.
 *
 * Bit-banged SPI-like communication.  Critical timing sections are
 * placed in IRAM to avoid I-cache misses during the clock sequence.
 *
 * Timing requirements (HX711 datasheet):
 *   T1 (SCK HIGH) ≥ 0.2 µs, ≤ 50 µs
 *   T2 (SCK LOW)  ≥ 0.2 µs
 *   T3 (DOUT valid after falling edge) ≤ 0.1 µs
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 */

#include "hx711.h"

#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

static const char *TAG = "HX711";

/* ── Private helpers ──────────────────────────────────────────────── */

/**
 * @brief Issue one SCK clock pulse and sample DOUT on the falling edge.
 *
 * SCK goes HIGH for 1 µs, then LOW for 1 µs.  DOUT is sampled after
 * the falling edge, which satisfies the ≤ 0.1 µs T3 requirement.
 *
 * @param[in] dev Initialised device handle.
 * @return Bit value (0 or 1) read from DOUT.
 */
static IRAM_ATTR int hx711_clock_pulse(const hx711_dev_t *dev)
{
    gpio_set_level(dev->cfg.sck_pin, 1);
    ets_delay_us(1);
    gpio_set_level(dev->cfg.sck_pin, 0);
    ets_delay_us(1);
    return gpio_get_level(dev->cfg.dout_pin);
}

/* ── Public API ───────────────────────────────────────────────────── */

/**
 * @brief Initialise the HX711 and configure GPIO pins.
 */
esp_err_t hx711_init(hx711_dev_t *dev, const hx711_config_t *cfg)
{
    if (!dev || !cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&dev->cfg, cfg, sizeof(hx711_config_t));
    dev->tare  = 0;
    dev->scale = 1.0f;

    gpio_config_t sck_conf = {
        .pin_bit_mask = (1ULL << cfg->sck_pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&sck_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SCK gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }
    gpio_set_level(cfg->sck_pin, 0);

    gpio_config_t dout_conf = {
        .pin_bit_mask = (1ULL << cfg->dout_pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&dout_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DOUT gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Init OK – DOUT:GPIO%d  SCK:GPIO%d  gain:%d",
             cfg->dout_pin, cfg->sck_pin, (int)cfg->gain);
    return ESP_OK;
}

/**
 * @brief Check whether a conversion result is ready (DOUT == LOW).
 */
esp_err_t hx711_is_ready(const hx711_dev_t *dev, bool *ready)
{
    *ready = (gpio_get_level(dev->cfg.dout_pin) == 0);
    return ESP_OK;
}

/**
 * @brief Block until DOUT is LOW or the timeout elapses.
 */
esp_err_t hx711_wait_ready(const hx711_dev_t *dev)
{
    const int64_t deadline = esp_timer_get_time()
                             + (int64_t)HX711_READY_TIMEOUT_MS * 1000LL;

    while (esp_timer_get_time() < deadline) {
        bool ready = false;
        hx711_is_ready(dev, &ready);
        if (ready) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(HX711_READY_POLL_DELAY_MS));
    }

    ESP_LOGW(TAG, "Timeout waiting for HX711 ready");
    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Read one raw 24-bit two's-complement sample.
 */
esp_err_t hx711_read_raw(hx711_dev_t *dev, int32_t *raw)
{
    esp_err_t err = hx711_wait_ready(dev);
    if (err != ESP_OK) return err;

    portDISABLE_INTERRUPTS();

    uint32_t data = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(dev->cfg.sck_pin, 1);
        ets_delay_us(1);
        data = (data << 1) | (uint32_t)gpio_get_level(dev->cfg.dout_pin);
        gpio_set_level(dev->cfg.sck_pin, 0);
        ets_delay_us(1);
    }

    /* Gain-select pulses */
    for (int i = 0; i < (int)dev->cfg.gain; i++) {
        hx711_clock_pulse(dev);
    }

    portENABLE_INTERRUPTS();

    /* Sign-extend 24-bit → int32_t */
    if (data & 0x800000U) data |= 0xFF000000U;
    *raw = (int32_t)data;

    ESP_LOGD(TAG, "raw=%" PRId32, *raw);
    return ESP_OK;
}

/**
 * @brief Average multiple raw readings.
 */
esp_err_t hx711_read_average(hx711_dev_t *dev, uint8_t samples, int32_t *avg)
{
    if (samples == 0 || samples > HX711_MAX_SAMPLES) return ESP_ERR_INVALID_ARG;

    int64_t sum   = 0;
    int     count = 0;

    for (uint8_t i = 0; i < samples; i++) {
        int32_t raw = 0;
        if (hx711_read_raw(dev, &raw) == ESP_OK) {
            sum += raw;
            count++;
        }
    }

    if (count == 0) {
        ESP_LOGE(TAG, "hx711_read_average: no valid samples");
        return ESP_FAIL;
    }

    *avg = (int32_t)(sum / count);
    ESP_LOGD(TAG, "avg=%" PRId32 " (%d/%d ok)", *avg, count, (int)samples);
    return ESP_OK;
}

/**
 * @brief Capture the zero-weight tare offset.
 */
esp_err_t hx711_tare(hx711_dev_t *dev, uint8_t samples)
{
    int32_t avg = 0;
    esp_err_t err = hx711_read_average(dev, samples, &avg);
    if (err == ESP_OK) {
        dev->tare = avg;
        ESP_LOGI(TAG, "Tare=%" PRId32, dev->tare);
    }
    return err;
}

/**
 * @brief Set the calibration scale factor.
 */
esp_err_t hx711_set_scale(hx711_dev_t *dev, float scale)
{
    if (scale == 0.0f) return ESP_ERR_INVALID_ARG;
    dev->scale = scale;
    ESP_LOGI(TAG, "Scale factor=%.4f", scale);
    return ESP_OK;
}

/**
 * @brief Retrieve the currently active scale factor.
 */
esp_err_t hx711_get_scale(const hx711_dev_t *dev, float *scale)
{
    *scale = dev->scale;
    return ESP_OK;
}

/**
 * @brief Read calibrated weight in grams.
 */
esp_err_t hx711_get_weight(hx711_dev_t *dev, uint8_t samples, float *grams)
{
    int32_t avg = 0;
    esp_err_t err = hx711_read_average(dev, samples, &avg);
    if (err != ESP_OK) return err;

    *grams = (float)(avg - dev->tare) / dev->scale;
    ESP_LOGD(TAG, "weight=%.2f g (avg=%" PRId32 " tare=%" PRId32 " scale=%.4f)",
             *grams, avg, dev->tare, dev->scale);
    return ESP_OK;
}

/**
 * @brief Put the HX711 into power-down mode (SCK held HIGH > 60 µs).
 */
esp_err_t hx711_power_down(hx711_dev_t *dev)
{
    gpio_set_level(dev->cfg.sck_pin, 0);
    gpio_set_level(dev->cfg.sck_pin, 1);
    ets_delay_us(65);
    ESP_LOGI(TAG, "Power down");
    return ESP_OK;
}

/**
 * @brief Wake the HX711 from power-down by pulling SCK LOW.
 */
esp_err_t hx711_power_up(hx711_dev_t *dev)
{
    gpio_set_level(dev->cfg.sck_pin, 0);
    ESP_LOGI(TAG, "Power up – wait 400 ms before first read");
    return ESP_OK;
}

/**
 * @brief Reset software state without touching GPIO.
 */
esp_err_t hx711_reset(hx711_dev_t *dev)
{
    dev->tare  = 0;
    dev->scale = 1.0f;
    ESP_LOGI(TAG, "Software reset");
    return ESP_OK;
}
