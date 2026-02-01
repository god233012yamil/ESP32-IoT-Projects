#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int i2c_scl_gpio;
    int i2c_sda_gpio;
} bus_safe_config_t;

/**
 * @brief Initialize bus-safe handling.
 *
 * This stores the bus GPIO numbers and sets them to a known safe state
 * early in boot to reduce risk of back-powering during reset/wake transitions.
 *
 * @param cfg Bus-safe configuration.
 */
void bus_safe_init(const bus_safe_config_t *cfg);

/**
 * @brief Apply a safe state to external bus pins before gating power off.
 *
 * This sets the selected bus GPIOs to INPUT and disables internal pulls.
 * This is a practical mitigation for phantom powering through ESD diodes.
 */
void bus_safe_apply_before_power_off(void);

#ifdef __cplusplus
}
#endif