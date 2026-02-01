#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PG_TECH_REG_EN = 0,
    PG_TECH_LOAD_SWITCH = 1,
    PG_TECH_PFET_DRIVER = 2,
} pg_technique_t;

typedef struct {
    pg_technique_t technique;
    int enable_gpio;
    bool active_high;
    uint32_t stabilize_ms;
} pg_config_t;

/**
 * @brief Initialize the power gating driver.
 *
 * This configures the enable GPIO to a safe default state (OFF) and prepares
 * any technique-specific controls.
 *
 * @param cfg Power gating configuration.
 */
void pg_init(const pg_config_t *cfg);

/**
 * @brief Enable or disable the gated rail.
 *
 * This toggles the enable GPIO according to the selected technique.
 * Firmware must ensure external buses are in a safe state before disabling power.
 *
 * @param enable True to enable the rail, false to disable it.
 */
void pg_set_enabled(bool enable);

/**
 * @brief Get the current power gating configuration.
 *
 * @return Pointer to the active configuration.
 */
const pg_config_t *pg_get_config(void);

#ifdef __cplusplus
}
#endif