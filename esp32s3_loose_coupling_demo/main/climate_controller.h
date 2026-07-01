#ifndef CLIMATE_CONTROLLER_H
#define CLIMATE_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "fan_output.h"
#include "temperature_source.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const temperature_source_t *temperature_source;
    const fan_output_t *fan_output;
    int16_t turn_on_threshold_c;
    int16_t turn_off_threshold_c;
    bool fan_enabled;
} climate_controller_t;

/**
 * @brief Initializes the climate controller with injected dependencies.
 *
 * Args:
 *     controller: Controller object to initialize.
 *     temperature_source: Temperature provider interface.
 *     fan_output: Fan output interface.
 *     turn_on_threshold_c: Temperature that enables the fan.
 *     turn_off_threshold_c: Temperature that disables the fan.
 */
void climate_controller_init(climate_controller_t *controller,
                             const temperature_source_t *temperature_source,
                             const fan_output_t *fan_output,
                             int16_t turn_on_threshold_c,
                             int16_t turn_off_threshold_c);

/**
 * @brief Updates the fan state using the current temperature sample.
 *
 * Args:
 *     controller: Controller object.
 *     temperature_c: Latest temperature sample in degrees Celsius.
 *
 * Returns:
 *     Fan output percentage after the update.
 */
uint8_t climate_controller_update(climate_controller_t *controller,
                                  int16_t temperature_c);

/**
 * @brief Reads the current fan enable state.
 *
 * Args:
 *     controller: Controller object.
 *
 * Returns:
 *     true if the fan is enabled, otherwise false.
 */
bool climate_controller_is_fan_enabled(const climate_controller_t *controller);

#ifdef __cplusplus
}
#endif

#endif
