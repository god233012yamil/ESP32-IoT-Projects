#include "climate_controller.h"

#define FAN_ON_PERCENT   80u
#define FAN_OFF_PERCENT  0u

/**
 * @brief Initializes the climate controller.
 *
 * Args:
 *     controller: Pointer to the climate controller instance.
 *     temperature_source: Pointer to the temperature source.
 *     fan_output: Pointer to the fan output.
 *     turn_on_threshold_c: Temperature threshold to turn the fan on.
 *     turn_off_threshold_c: Temperature threshold to turn the fan off.
 */
void climate_controller_init(climate_controller_t *controller,
                             const temperature_source_t *temperature_source,
                             const fan_output_t *fan_output,
                             int16_t turn_on_threshold_c,
                             int16_t turn_off_threshold_c)
{
    controller->temperature_source = temperature_source;
    controller->fan_output = fan_output;
    controller->turn_on_threshold_c = turn_on_threshold_c;
    controller->turn_off_threshold_c = turn_off_threshold_c;
    controller->fan_enabled = false;

    controller->fan_output->set_percent(FAN_OFF_PERCENT);
}

/**
 * @brief Updates the climate controller based on the current temperature.
 *
 * Args:
 *     controller: Pointer to the climate controller instance.
 *     temperature_c: Current temperature in Celsius.
 *
 * Returns:
 *     The percentage at which the fan should run.
 */
uint8_t climate_controller_update(climate_controller_t *controller,
                                  int16_t temperature_c)
{
    if (temperature_c >= controller->turn_on_threshold_c)
    {
        controller->fan_enabled = true;
    }
    else if (temperature_c <= controller->turn_off_threshold_c)
    {
        controller->fan_enabled = false;
    }

    uint8_t fan_percent = controller->fan_enabled ? FAN_ON_PERCENT : FAN_OFF_PERCENT;
    controller->fan_output->set_percent(fan_percent);

    return fan_percent;
}

/**
 * @brief Checks if the fan is enabled.
 *
 * Args:
 *     controller: Pointer to the climate controller instance.
 *
 * Returns:
 *     true if the fan is enabled, false otherwise.
 */
bool climate_controller_is_fan_enabled(const climate_controller_t *controller)
{
    return controller->fan_enabled;
}
