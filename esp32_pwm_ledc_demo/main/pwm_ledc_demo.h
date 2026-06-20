#ifndef PWM_LEDC_DEMO_H
#define PWM_LEDC_DEMO_H

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes every PWM output used by the demo application.
 *
 * This function configures one LEDC timer and channel for each enabled demo:
 * LED dimming, motor speed control, passive buzzer tone generation, and
 * servo-style pulse generation.
 *
 * @return ESP_OK when every enabled PWM output is configured successfully.
 * @return ESP_ERR_INVALID_ARG when one or more configuration parameters are invalid.
 */
esp_err_t pwm_demo_init(void);

/**
 * @brief Runs the complete PWM demonstration sequence forever.
 *
 * The sequence demonstrates LED dimming, DC motor duty control, passive buzzer
 * frequency changes, and servo-style pulse width generation.
 */
void pwm_demo_run(void);

#ifdef __cplusplus
}
#endif

#endif /* PWM_LEDC_DEMO_H */
