#ifndef FAKE_TEMPERATURE_H
#define FAKE_TEMPERATURE_H

#include <stdint.h>

#include "temperature_source.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const temperature_source_t fake_temperature_source;

/**
 * @brief Initializes the simulated temperature source.
 *
 * Args:
 *     initial_temperature_c: Initial simulated temperature in degrees Celsius.
 */
void fake_temperature_init(int16_t initial_temperature_c);

/**
 * @brief Updates the simulated temperature ramp.
 *
 * This function intentionally keeps the temperature module independent from
 * the climate controller. It only owns the simulated data source.
 */
void fake_temperature_update(void);

#ifdef __cplusplus
}
#endif

#endif
