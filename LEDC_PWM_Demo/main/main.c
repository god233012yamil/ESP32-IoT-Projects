/**
 * @file main.c
 * @brief Generate a PWM signal on an ESP32 using the LEDC peripheral (ESP-IDF).
 *
 * Overview
 * -------
 * This example configures an LEDC timer and a single LEDC channel to output a PWM
 * waveform on a user-selected GPIO. It validates the duty-cycle range, sets the PWM
 * frequency and duty-resolution, computes the raw duty value, and starts the signal.
 *
 * Hardware Notes
 * --------------
 * - Ensure the selected GPIO supports output and LEDC function on your target.
 *   For classic ESP32, GPIO34–39 are input-only; do not select those for PWM.
 * - On ESP32-S2/S3/C3, only `LEDC_LOW_SPEED_MODE` is available.
 *
 * Key Parameters
 * --------------
 * - Frequency vs. resolution trade-off is determined by the timer configuration.
 * - Duty cycle is expressed as a percentage and converted into a raw value using
 *   the active resolution (e.g., 13-bit → 0..8191).
 *
 * Build & Run
 * -----------
 *   idf.py set-target esp32
 *   idf.py build flash monitor
 *
 * Limitations
 * -----------
 * - No dynamic changes (fade, duty updates) are demonstrated here.
 *   See `ledc_set_duty()` and `ledc_update_duty()` for runtime changes.
 */

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

// ===================== User Configuration =====================
#define PWM_GPIO            35                 ///< Output GPIO for PWM (verify it is output-capable on your board)
#define PWM_FREQ_HZ         5000               ///< PWM frequency in Hz
#define PWM_DUTY_PERCENT    75                 ///< Duty cycle in percent (expected: 20–90)

#define LEDC_MODE           LEDC_LOW_SPEED_MODE///< LEDC speed mode (S2/S3/C3: low-speed only)

static const ledc_timer_bit_t PWM_RES_BITS = LEDC_TIMER_13_BIT;  ///< Resolution (bits), sets duty granularity
// ===============================================================

static const char *TAG = "LEDC_PWM";

/**
 * @brief Validate a duty-cycle percentage against acceptable limits.
 *
 * Args:
 *   pct: Duty cycle in percent.
 *
 * Returns:
 *   true if the value is within [0, 100] and within the example’s recommended window
 *   (20–90 to avoid extremes on some loads); false otherwise.
 *
 * Notes:
 *   Adjust limits as needed for your application; 0–100 is electrically valid,
 *   but very low/high duties can be less illustrative on some hardware.
 */
static bool pwm_validate_duty_percent(int pct) {
    if (pct < 0 || pct > 100) {
        return false;
    }
    // Example-specific teaching window:
    return (pct >= 20 && pct <= 90);
}

/**
 * @brief Compute the raw LEDC duty value from a percentage and timer resolution.
 *
 * Args:
 *   res_bits: LEDC timer resolution (e.g., `LEDC_TIMER_13_BIT`).
 *   pct:      Duty cycle in percent [0..100].
 *
 * Returns:
 *   Raw duty value in the range [0 .. (2^res_bits - 1)].
 *
 * Example:
 *   With 13-bit resolution, 50% -> ~4095 (of 0..8191).
 */
static uint32_t pwm_compute_duty(ledc_timer_bit_t res_bits, int pct) {
    const uint32_t max_duty = (1U << res_bits) - 1U;
    return (uint32_t)((pct * (uint64_t)max_duty) / 100U);
}

/**
 * @brief Configure an LEDC timer that controls PWM frequency and duty resolution.
 *
 * Args:
 *   mode:     LEDC speed mode (usually `LEDC_LOW_SPEED_MODE`).
 *   timer:    LEDC timer index (e.g., `LEDC_TIMER_0`).
 *   res_bits: Duty resolution (e.g., `LEDC_TIMER_13_BIT`).
 *   freq_hz:  Target PWM frequency in Hz.
 *
 * Returns:
 *   ESP_OK on success; an error code from `ledc_timer_config()` otherwise.
 *
 * Notes:
 *   The achievable frequency depends on source clock and resolution. If the
 *   request cannot be met exactly, the driver may choose the closest value.
 */
static esp_err_t pwm_configure_timer(ledc_mode_t mode,
                                     ledc_timer_t timer,
                                     ledc_timer_bit_t res_bits,
                                     uint32_t freq_hz) {
    const ledc_timer_config_t tcfg = {
        .speed_mode       = mode,
        .timer_num        = timer,
        .duty_resolution  = res_bits,
        .freq_hz          = freq_hz,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    return ledc_timer_config(&tcfg);
}

/**
 * @brief Configure an LEDC channel to output PWM on a given GPIO.
 *
 * Args:
 *   mode:     LEDC speed mode used by the timer.
 *   channel:  LEDC channel index (e.g., `LEDC_CHANNEL_0`).
 *   timer:    LEDC timer previously configured for frequency/resolution.
 *   gpio:     Output GPIO number (must be output-capable and routed to LEDC).
 *   duty:     Initial raw duty value matching the timer resolution.
 *
 * Returns:
 *   ESP_OK on success; an error code from `ledc_channel_config()` otherwise.
 *
 * Notes:
 *   After configuration, call `ledc_set_duty()` + `ledc_update_duty()` to update duty at runtime.
 */
static esp_err_t pwm_configure_channel(ledc_mode_t mode,
                                       ledc_channel_t channel,
                                       ledc_timer_t timer,
                                       int gpio,
                                       uint32_t duty) {
    const ledc_channel_config_t ccfg = {
        .speed_mode     = mode,
        .channel        = channel,
        .timer_sel      = timer,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio,
        .duty           = duty,
        .hpoint         = 0
    };
    return ledc_channel_config(&ccfg);
}

/**
 * @brief Application entry point: validate input, configure LEDC timer/channel, start PWM.
 *
 * Behavior:
 *   1) Validates the duty-cycle percentage (`PWM_DUTY_PERCENT`) for this demo.
 *   2) Configures the LEDC timer with the selected frequency and resolution.
 *   3) Computes the raw duty from the percentage and resolution.
 *   4) Configures the LEDC channel to output PWM on `PWM_GPIO`.
 *   5) Logs the final parameters.
 *
 * Returns:
 *   void
 *
 * Errors:
 *   - If validation fails or any LEDC configuration call returns an error,
 *     the function logs a message and returns early without starting PWM.
 */
void app_main(void) {
    if (!pwm_validate_duty_percent(PWM_DUTY_PERCENT)) {
        ESP_LOGE(TAG, "Duty cycle must be between 20%% and 90%% (given: %d%%)", PWM_DUTY_PERCENT);
        return;
    }

    esp_err_t err = pwm_configure_timer(LEDC_MODE, LEDC_TIMER_0, PWM_RES_BITS, PWM_FREQ_HZ);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(err));
        return;
    }

    const uint32_t duty_raw = pwm_compute_duty(PWM_RES_BITS, PWM_DUTY_PERCENT);

    err = pwm_configure_channel(LEDC_MODE, LEDC_CHANNEL_0, LEDC_TIMER_0, PWM_GPIO, duty_raw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "PWM started on GPIO%d @ %u Hz, %d%% duty (raw=%lu, res=%u-bit)",
             PWM_GPIO, PWM_FREQ_HZ, PWM_DUTY_PERCENT,
             (unsigned long)duty_raw, (unsigned)PWM_RES_BITS);
}