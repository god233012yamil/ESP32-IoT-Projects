#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configures a timer wake source and enters deep sleep.
 *
 * Args:
 *     sleep_seconds: Requested sleep interval in seconds.
 *
 * Returns:
 *     ESP_OK before deep sleep starts, or an error code when configuration fails.
 */
esp_err_t power_manager_enter_deep_sleep(uint32_t sleep_seconds);

/**
 * @brief Returns a human-readable description of the wake-up cause.
 *
 * Returns:
 *     Pointer to a constant wake-up cause string.
 */
const char *power_manager_wakeup_cause_string(void);

#ifdef __cplusplus
}
#endif

#endif
