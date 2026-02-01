#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configure wake sources and enter deep sleep.
 *
 * This configures a timer wake and optionally powers down RTC peripherals where safe.
 * The exact domains you can power down depend on your required wake sources.
 *
 * @param wake_interval_s Sleep interval in seconds.
 */
void sleep_ctrl_enter_deep_sleep(uint32_t wake_interval_s);

#ifdef __cplusplus
}
#endif