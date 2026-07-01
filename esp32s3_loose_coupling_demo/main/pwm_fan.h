#ifndef PWM_FAN_H
#define PWM_FAN_H

#include "fan_output.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const fan_output_t pwm_fan_output;

/**
 * @brief Initializes the PWM fan output driver.
 *
 * The fan driver implements the generic fan_output_t interface, so the climate
 * controller does not need to know about LEDC, GPIO numbers, or timer details.
 */
void pwm_fan_init(void);

#ifdef __cplusplus
}
#endif

#endif
