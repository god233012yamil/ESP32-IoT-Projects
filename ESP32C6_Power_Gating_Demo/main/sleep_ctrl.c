/**
 * @file sleep_ctrl.c
 * @brief Deep sleep configuration for ESP32-C6 power gating demo.
 *
 * This module configures a timer wake and enters deep sleep. It also demonstrates
 * selective power domain shutdown using esp_sleep_pd_config().
 *
 * ESP32-C6 detail:
 * - GPIO behavior during deep sleep and reset can vary by board and strapping.
 * - Always enforce a hardware default OFF for any external power enable pin.
 */

#include "sleep_ctrl.h"

#include "esp_sleep.h"
#include "esp_log.h"

static const char *TAG = "sleep";

/**
 * @brief Convert seconds to microseconds safely.
 *
 * @param seconds Sleep interval in seconds.
 * @return uint64_t interval in microseconds.
 */
static uint64_t sleep_seconds_to_us(uint32_t seconds)
{
    return (uint64_t)seconds * 1000000ULL;
}

void sleep_ctrl_enter_deep_sleep(uint32_t wake_interval_s)
{
    ESP_LOGI(TAG, "Configuring deep sleep, wake in %u s", (unsigned)wake_interval_s);

    // Optional: power down RTC peripherals if you do not need RTC IO or certain wake sources.
    // Keep in mind: wake sources may require RTC peripherals.
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

    // Timer wakeup.
    esp_sleep_enable_timer_wakeup(sleep_seconds_to_us(wake_interval_s));

    // Enter deep sleep.
    ESP_LOGI(TAG, "Entering deep sleep now");
    esp_deep_sleep_start();
}