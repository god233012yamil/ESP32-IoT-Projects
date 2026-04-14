/**
 * @file hx711.h
 * @brief HX711 24-bit ADC driver for weight-scale applications on ESP32-S3.
 *
 * Public API for bit-banged communication with the HX711 load-cell
 * amplifier. Supports channel/gain selection, tare calibration, and
 * multi-sample averaging.
 *
 * @author  ESP32-S3 Scale Project
 * @version 1.0.0
 * @date    2025
 *
 * Hardware: HX711 + 4 × 50 kg half-bridge load cells wired as a
 *           full Wheatstone bridge.
 */

#ifndef HX711_H
#define HX711_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ────────────────────────────────────────────────────── */

/** Default number of ADC samples averaged per weight reading. */
#define HX711_DEFAULT_SAMPLES     10

/** Hard upper limit on samples-per-average call. */
#define HX711_MAX_SAMPLES         64

/** Maximum milliseconds to wait for DOUT to go LOW. */
#define HX711_READY_TIMEOUT_MS    1000

/** Milliseconds between DOUT polling retries. */
#define HX711_READY_POLL_DELAY_MS 1

/* ── Types ────────────────────────────────────────────────────────── */

/**
 * @brief HX711 channel / gain selection.
 *
 * The number of extra SCK pulses after the 24 data bits sets the gain
 * for the *next* conversion cycle.
 */
typedef enum {
    HX711_GAIN_A_128 = 1, /**< Channel A, gain 128 (1 extra pulse)  */
    HX711_GAIN_B_32  = 2, /**< Channel B, gain 32  (2 extra pulses) */
    HX711_GAIN_A_64  = 3, /**< Channel A, gain 64  (3 extra pulses) */
} hx711_gain_t;

/** @brief Hardware configuration supplied by the application. */
typedef struct {
    gpio_num_t   dout_pin; /**< GPIO connected to HX711 DOUT */
    gpio_num_t   sck_pin;  /**< GPIO connected to HX711 SCK  */
    hx711_gain_t gain;     /**< Channel / gain setting        */
} hx711_config_t;

/** @brief Runtime state for one HX711 device instance. */
typedef struct {
    hx711_config_t cfg;   /**< Copy of user-supplied hardware config */
    int32_t        tare;  /**< Raw ADC offset captured during tare   */
    float          scale; /**< raw-count-per-gram conversion factor  */
} hx711_dev_t;

/* ── API ──────────────────────────────────────────────────────────── */

/**
 * @brief Initialise an HX711 instance and configure its GPIO pins.
 *
 * Configures SCK as a push-pull output (initially LOW) and DOUT as a
 * floating input. Sets tare = 0 and scale = 1.0 so the device returns
 * raw counts before calibration.
 *
 * @param[out] dev Uninitialised hx711_dev_t to populate.
 * @param[in]  cfg Pin and gain configuration.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG or GPIO error otherwise.
 */
esp_err_t hx711_init(hx711_dev_t *dev, const hx711_config_t *cfg);

/**
 * @brief Query whether a conversion result is available (DOUT == LOW).
 *
 * @param[in]  dev   Initialised device handle.
 * @param[out] ready Set to true when DOUT is LOW.
 * @return ESP_OK always.
 */
esp_err_t hx711_is_ready(const hx711_dev_t *dev, bool *ready);

/**
 * @brief Block until DOUT goes LOW or the timeout elapses.
 *
 * Uses vTaskDelay() between polls to yield CPU time to other tasks.
 *
 * @param[in] dev Initialised device handle.
 * @return ESP_OK when ready; ESP_ERR_TIMEOUT if the deadline is missed.
 */
esp_err_t hx711_wait_ready(const hx711_dev_t *dev);

/**
 * @brief Read one raw 24-bit two's-complement sample.
 *
 * Disables interrupts during the 24 + N clock-pulse sequence to prevent
 * timing glitches. The result is sign-extended to int32_t.
 *
 * @param[in]  dev Initialised device handle.
 * @param[out] raw Sign-extended 32-bit ADC value.
 * @return ESP_OK on success; ESP_ERR_TIMEOUT if not ready.
 */
esp_err_t hx711_read_raw(hx711_dev_t *dev, int32_t *raw);

/**
 * @brief Average multiple raw readings to reduce noise.
 *
 * Calls hx711_read_raw() @p samples times and returns the integer mean.
 * Failed individual reads are skipped; ESP_FAIL is returned only when
 * every sample fails.
 *
 * @param[in]  dev     Initialised device handle.
 * @param[in]  samples Number of samples (1 – HX711_MAX_SAMPLES).
 * @param[out] avg     Averaged raw ADC value.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG or ESP_FAIL otherwise.
 */
esp_err_t hx711_read_average(hx711_dev_t *dev, uint8_t samples, int32_t *avg);

/**
 * @brief Capture the zero-weight tare offset.
 *
 * Averages @p samples readings with an empty platform and stores the
 * result in dev->tare. Subsequent hx711_get_weight() calls subtract it.
 *
 * @param[in] dev     Initialised device handle.
 * @param[in] samples Number of samples to average.
 * @return ESP_OK on success; propagated error otherwise.
 */
esp_err_t hx711_tare(hx711_dev_t *dev, uint8_t samples);

/**
 * @brief Set the calibration scale factor (raw counts per gram).
 *
 * Determine this once during calibration:
 *   scale = (avg_with_known_weight − tare) / known_weight_grams
 *
 * @param[in] dev   Initialised device handle.
 * @param[in] scale Non-zero conversion factor.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG if scale == 0.
 */
esp_err_t hx711_set_scale(hx711_dev_t *dev, float scale);

/**
 * @brief Retrieve the currently active scale factor.
 *
 * @param[in]  dev   Initialised device handle.
 * @param[out] scale Current scale factor.
 * @return ESP_OK always.
 */
esp_err_t hx711_get_scale(const hx711_dev_t *dev, float *scale);

/**
 * @brief Read calibrated weight in grams.
 *
 * weight_g = (average_raw − tare) / scale
 *
 * @param[in]  dev     Initialised device handle.
 * @param[in]  samples Samples to average.
 * @param[out] grams   Calculated weight in grams.
 * @return ESP_OK on success; propagated error otherwise.
 */
esp_err_t hx711_get_weight(hx711_dev_t *dev, uint8_t samples, float *grams);

/**
 * @brief Power down the HX711 (SCK held HIGH > 60 µs, ≈1 µA standby).
 *
 * @param[in] dev Initialised device handle.
 * @return ESP_OK always.
 */
esp_err_t hx711_power_down(hx711_dev_t *dev);

/**
 * @brief Wake the HX711 from power-down by driving SCK LOW.
 *
 * Allow ~400 ms before the first reliable reading.
 *
 * @param[in] dev Initialised device handle.
 * @return ESP_OK always.
 */
esp_err_t hx711_power_up(hx711_dev_t *dev);

/**
 * @brief Reset software state (tare = 0, scale = 1.0) without touching GPIO.
 *
 * @param[in] dev Initialised device handle.
 * @return ESP_OK always.
 */
esp_err_t hx711_reset(hx711_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* HX711_H */
