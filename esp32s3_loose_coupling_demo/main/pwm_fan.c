#include "pwm_fan.h"

#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"

#define PWM_FAN_GPIO             2
#define PWM_FAN_TIMER            LEDC_TIMER_0
#define PWM_FAN_MODE             LEDC_LOW_SPEED_MODE
#define PWM_FAN_CHANNEL          LEDC_CHANNEL_0
#define PWM_FAN_DUTY_RESOLUTION  LEDC_TIMER_10_BIT
#define PWM_FAN_FREQUENCY_HZ     25000
#define PWM_FAN_MAX_DUTY         1023u

/**
 * @brief Sets the fan speed as a percentage.
 *
 * Args:
 *     percent: The fan speed as a percentage (0-100).
 */
static void pwm_fan_set_percent(uint8_t percent)
{
    if (percent > 100u)
    {
        percent = 100u;
    }

    uint32_t duty = ((uint32_t)percent * PWM_FAN_MAX_DUTY) / 100u;

    // Set the duty cycle for the PWM fan
    (void)ledc_set_duty(PWM_FAN_MODE, PWM_FAN_CHANNEL, duty);

    // Update the duty cycle to apply the new value
    (void)ledc_update_duty(PWM_FAN_MODE, PWM_FAN_CHANNEL);
}

/**
 * @brief Gets the PWM fan output.
 *
 * Returns:
 *     Pointer to the PWM fan output.
 */
const fan_output_t pwm_fan_output =
{
    .set_percent = pwm_fan_set_percent
};

/**
 * @brief Initializes the PWM fan.
 */
void pwm_fan_init(void)
{
    // Configure the LEDC timer for PWM
    ledc_timer_config_t timer_config =
    {
        .speed_mode = PWM_FAN_MODE,
        .duty_resolution = PWM_FAN_DUTY_RESOLUTION,
        .timer_num = PWM_FAN_TIMER,
        .freq_hz = PWM_FAN_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    (void)ledc_timer_config(&timer_config);

    // Configure the LEDC channel for the PWM fan
    ledc_channel_config_t channel_config =
    {
        .gpio_num = PWM_FAN_GPIO,
        .speed_mode = PWM_FAN_MODE,
        .channel = PWM_FAN_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_FAN_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    (void)ledc_channel_config(&channel_config);
}
