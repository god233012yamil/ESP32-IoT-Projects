#include "power_manager.h"

#include <stdio.h>

#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "power";

/**
 * @brief Configures a timer wake source and enters deep sleep.
 *
 * Args:
 *     sleep_seconds: Requested sleep interval in seconds.
 *
 * Returns:
 *     ESP_OK before deep sleep starts, or an error code when configuration fails.
 */
esp_err_t power_manager_enter_deep_sleep(uint32_t sleep_seconds)
{
    if (sleep_seconds == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint64_t sleep_time_us = (uint64_t)sleep_seconds * 1000000ULL;
    esp_err_t err = esp_sleep_enable_timer_wakeup(sleep_time_us);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Entering deep sleep for %lu seconds",
             (unsigned long)sleep_seconds);

    // Flush the log output before powering down the CPU and UART.
    fflush(stdout);
    esp_deep_sleep_start();

    return ESP_OK;
}

/**
 * @brief Returns a human-readable description of the wake-up cause.
 *
 * Returns:
 *     Pointer to a constant wake-up cause string.
 */
const char *power_manager_wakeup_cause_string(void)
{
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER:
            return "RTC timer";
        case ESP_SLEEP_WAKEUP_EXT0:
            return "EXT0 GPIO";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "EXT1 GPIO";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "touch pad";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP coprocessor";
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return "power-on or reset";
        default:
            return "other";
    }
}
