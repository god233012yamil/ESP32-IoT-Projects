#include "pwm_ledc_demo.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#define LED_GPIO                CONFIG_PWM_DEMO_LED_GPIO
#define MOTOR_GPIO              CONFIG_PWM_DEMO_MOTOR_GPIO
#define BUZZER_GPIO             CONFIG_PWM_DEMO_BUZZER_GPIO
#define SERVO_GPIO              CONFIG_PWM_DEMO_SERVO_GPIO

#define LED_PWM_MODE            LEDC_LOW_SPEED_MODE
#define LED_PWM_TIMER           LEDC_TIMER_0
#define LED_PWM_CHANNEL         LEDC_CHANNEL_0
#define LED_PWM_FREQ_HZ         5000U
#define LED_PWM_RESOLUTION      LEDC_TIMER_10_BIT
#define LED_PWM_RES_BITS        10U
#define LED_PWM_MAX_DUTY        ((1U << LED_PWM_RES_BITS) - 1U)

#define MOTOR_PWM_MODE          LEDC_LOW_SPEED_MODE
#define MOTOR_PWM_TIMER         LEDC_TIMER_1
#define MOTOR_PWM_CHANNEL       LEDC_CHANNEL_1
#define MOTOR_PWM_FREQ_HZ       20000U
#define MOTOR_PWM_RESOLUTION    LEDC_TIMER_10_BIT
#define MOTOR_PWM_RES_BITS      10U
#define MOTOR_PWM_MAX_DUTY      ((1U << MOTOR_PWM_RES_BITS) - 1U)

#define BUZZER_PWM_MODE         LEDC_LOW_SPEED_MODE
#define BUZZER_PWM_TIMER        LEDC_TIMER_2
#define BUZZER_PWM_CHANNEL      LEDC_CHANNEL_2
#define BUZZER_PWM_FREQ_HZ      1000U
#define BUZZER_PWM_RESOLUTION   LEDC_TIMER_10_BIT
#define BUZZER_PWM_RES_BITS     10U
#define BUZZER_PWM_MAX_DUTY     ((1U << BUZZER_PWM_RES_BITS) - 1U)

#define SERVO_PWM_MODE          LEDC_LOW_SPEED_MODE
#define SERVO_PWM_TIMER         LEDC_TIMER_3
#define SERVO_PWM_CHANNEL       LEDC_CHANNEL_3
#define SERVO_PWM_FREQ_HZ       50U
#define SERVO_PWM_RESOLUTION    LEDC_TIMER_14_BIT
#define SERVO_PWM_RES_BITS      14U
#define SERVO_PWM_MAX_DUTY      ((1U << SERVO_PWM_RES_BITS) - 1U)
#define SERVO_PERIOD_US         20000U
#define SERVO_MIN_PULSE_US      1000U
#define SERVO_CENTER_PULSE_US   1500U
#define SERVO_MAX_PULSE_US      2000U

#define DEMO_STEP_DELAY_MS      350U
#define SERVO_STEP_DELAY_MS     900U
#define LED_FADE_DELAY_MS       8U

static const char *TAG = "pwm_ledc_demo";

/**
 * @brief Describes one LEDC PWM output.
 */
typedef struct {
    ledc_mode_t speed_mode;
    ledc_timer_t timer;
    ledc_channel_t channel;
    ledc_timer_bit_t resolution;
    uint32_t resolution_bits;
    uint32_t frequency_hz;
    int gpio_num;
} pwm_output_t;

/**
 * @brief LEDC output description for the LED PWM channel.  
 * 
 */
static const pwm_output_t s_led_pwm = {
    .speed_mode = LED_PWM_MODE,
    .timer = LED_PWM_TIMER,
    .channel = LED_PWM_CHANNEL,
    .resolution = LED_PWM_RESOLUTION,
    .resolution_bits = LED_PWM_RES_BITS,
    .frequency_hz = LED_PWM_FREQ_HZ,
    .gpio_num = LED_GPIO,
};

#if CONFIG_PWM_DEMO_ENABLE_MOTOR
/**
 * @brief LEDC output description for the motor PWM channel.
 * 
 */
static const pwm_output_t s_motor_pwm = {
    .speed_mode = MOTOR_PWM_MODE,
    .timer = MOTOR_PWM_TIMER,
    .channel = MOTOR_PWM_CHANNEL,
    .resolution = MOTOR_PWM_RESOLUTION,
    .resolution_bits = MOTOR_PWM_RES_BITS,
    .frequency_hz = MOTOR_PWM_FREQ_HZ,
    .gpio_num = MOTOR_GPIO,
};
#endif

#if CONFIG_PWM_DEMO_ENABLE_BUZZER
/**
 * @brief LEDC output description for the buzzer PWM channel.
 * 
 */
static const pwm_output_t s_buzzer_pwm = {
    .speed_mode = BUZZER_PWM_MODE,
    .timer = BUZZER_PWM_TIMER,
    .channel = BUZZER_PWM_CHANNEL,
    .resolution = BUZZER_PWM_RESOLUTION,
    .resolution_bits = BUZZER_PWM_RES_BITS,
    .frequency_hz = BUZZER_PWM_FREQ_HZ,
    .gpio_num = BUZZER_GPIO,
};
#endif

#if CONFIG_PWM_DEMO_ENABLE_SERVO
/**
 * @brief LEDC output description for the servo PWM channel.
 * 
 */
static const pwm_output_t s_servo_pwm = {
    .speed_mode = SERVO_PWM_MODE,
    .timer = SERVO_PWM_TIMER,
    .channel = SERVO_PWM_CHANNEL,
    .resolution = SERVO_PWM_RESOLUTION,
    .resolution_bits = SERVO_PWM_RES_BITS,
    .frequency_hz = SERVO_PWM_FREQ_HZ,
    .gpio_num = SERVO_GPIO,
};
#endif

/**
 * @brief Returns the maximum duty value for a PWM output.
 *
 * Args:
 *     pwm: Pointer to the PWM output description.
 *
 * Returns:
 *     Maximum raw duty value supported by the output resolution.
 */
static uint32_t pwm_get_max_duty(const pwm_output_t *pwm)
{
    return (1U << pwm->resolution_bits) - 1U;
}

/**
 * @brief Converts a percentage value into a raw LEDC duty value.
 *
 * Args:
 *     percent: Duty cycle percentage from 0 to 100.
 *     max_duty: Maximum raw duty value for the configured PWM resolution.
 *
 * Returns:
 *     Raw LEDC duty value matching the requested percentage.
 */
static uint32_t pwm_percent_to_duty(uint32_t percent, uint32_t max_duty)
{
    if (percent > 100U) {
        percent = 100U;
    }

    return (percent * max_duty) / 100U;
}

/**
 * @brief Converts a servo pulse width into a raw LEDC duty value.
 *
 * Args:
 *     pulse_width_us: Pulse width in microseconds.
 *
 * Returns:
 *     Raw LEDC duty value that generates the requested pulse width.
 */
static uint32_t servo_pulse_us_to_duty(uint32_t pulse_width_us)
{
    return (pulse_width_us * SERVO_PWM_MAX_DUTY) / SERVO_PERIOD_US;
}

/**
 * @brief Configures one LEDC timer and one LEDC channel.
 *
 * Args:
 *     pwm: Pointer to the PWM output description.
 *     initial_duty: Initial raw duty value applied to the channel.
 *
 * Returns:
 *     ESP_OK when the output is configured successfully.
 *     ESP_ERR_INVALID_ARG when the configuration is invalid.
 */
static esp_err_t pwm_output_init(const pwm_output_t *pwm, uint32_t initial_duty)
{
    ESP_RETURN_ON_FALSE(pwm != NULL, ESP_ERR_INVALID_ARG, TAG, "pwm is NULL");

    // Configure the LEDC timer for the output channel. The timer controls the frequency and resolution of the PWM signal.
    ledc_timer_config_t timer_config = {
        .speed_mode = pwm->speed_mode,
        .timer_num = pwm->timer,
        .duty_resolution = pwm->resolution,
        .freq_hz = pwm->frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "Failed to configure LEDC timer");

    // Configure the LEDC channel with the timer and initial duty cycle. The channel controls the GPIO output.
    ledc_channel_config_t channel_config = {
        .gpio_num = pwm->gpio_num,
        .speed_mode = pwm->speed_mode,
        .channel = pwm->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = pwm->timer,
        .duty = initial_duty,
        .hpoint = 0,
        .flags.output_invert = 0,
    };

    // Set the initial duty cycle to ensure the output starts in a known state. 
    // The duty cycle can be changed later using the LEDC API.
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "Failed to configure LEDC channel");

    return ESP_OK;
}

/**
 * @brief Sets a raw duty value on one PWM output.
 *
 * Args:
 *     pwm: Pointer to the PWM output description.
 *     duty: Raw duty value to apply.
 *
 * Returns:
 *     ESP_OK when the duty cycle is updated successfully.
 *     ESP_ERR_INVALID_ARG when the configuration is invalid.
 */
static esp_err_t pwm_set_duty_raw(const pwm_output_t *pwm, uint32_t duty)
{
    ESP_RETURN_ON_FALSE(pwm != NULL, ESP_ERR_INVALID_ARG, TAG, "pwm is NULL");

    const uint32_t max_duty = pwm_get_max_duty(pwm);

    if (duty > max_duty) {
        duty = max_duty;
    }

    // Update the LEDC channel with the new duty cycle. 
    // The LEDC API requires setting the duty and then updating it to apply the change.
    ESP_RETURN_ON_ERROR(ledc_set_duty(pwm->speed_mode, pwm->channel, duty), TAG, "Failed to set duty");
    
    // Update the LEDC channel to apply the new duty cycle. 
    // This step is required after changing the duty value.
    ESP_RETURN_ON_ERROR(ledc_update_duty(pwm->speed_mode, pwm->channel), TAG, "Failed to update duty");

    return ESP_OK;
}

/**
 * @brief Sets a duty cycle percentage on one PWM output.
 *
 * Args:
 *     pwm: Pointer to the PWM output description.
 *     percent: Duty cycle percentage from 0 to 100.
 *
 * Returns:
 *     ESP_OK when the duty cycle is updated successfully.
 *     ESP_ERR_INVALID_ARG when the configuration is invalid.
 */
static esp_err_t pwm_set_duty_percent(const pwm_output_t *pwm, uint32_t percent)
{
    ESP_RETURN_ON_FALSE(pwm != NULL, ESP_ERR_INVALID_ARG, TAG, "pwm is NULL");

    const uint32_t duty = pwm_percent_to_duty(percent, pwm_get_max_duty(pwm));

    return pwm_set_duty_raw(pwm, duty);
}

/**
 * @brief Changes the PWM frequency of a configured LEDC timer.
 *
 * Args:
 *     pwm: Pointer to the PWM output description.
 *     frequency_hz: New PWM frequency in hertz.
 *
 * Returns:
 *     ESP_OK when the frequency is updated successfully.
 *     ESP_ERR_INVALID_ARG when the configuration is invalid.
 */
static esp_err_t pwm_set_frequency(const pwm_output_t *pwm, uint32_t frequency_hz)
{
    ESP_RETURN_ON_FALSE(pwm != NULL, ESP_ERR_INVALID_ARG, TAG, "pwm is NULL");
    ESP_RETURN_ON_FALSE(frequency_hz > 0U, ESP_ERR_INVALID_ARG, TAG, "frequency is zero");

    ESP_RETURN_ON_ERROR(ledc_set_freq(pwm->speed_mode, pwm->timer, frequency_hz), TAG, "Failed to set frequency");

    return ESP_OK;
}

/**
 * @brief Runs a smooth LED fade demonstration.
 */
static void demo_led_dimming(void)
{
    ESP_LOGI(TAG, "LED dimming demo on GPIO %d", LED_GPIO);

    for (uint32_t duty = 0U; duty <= LED_PWM_MAX_DUTY; duty += 8U) {
        ESP_ERROR_CHECK(pwm_set_duty_raw(&s_led_pwm, duty));
        vTaskDelay(pdMS_TO_TICKS(LED_FADE_DELAY_MS));
    }

    for (int32_t duty = (int32_t)LED_PWM_MAX_DUTY; duty >= 0; duty -= 8) {
        ESP_ERROR_CHECK(pwm_set_duty_raw(&s_led_pwm, (uint32_t)duty));
        vTaskDelay(pdMS_TO_TICKS(LED_FADE_DELAY_MS));
    }

    ESP_ERROR_CHECK(pwm_set_duty_raw(&s_led_pwm, 0U));
}

#if CONFIG_PWM_DEMO_ENABLE_MOTOR
/**
 * @brief Runs a motor speed duty-cycle demonstration.
 *
 * The GPIO must drive an external motor driver, MOSFET stage, or H-bridge. The
 * ESP32 GPIO must not be connected directly to a motor.
 */
static void demo_motor_speed(void)
{
    const uint32_t speed_steps[] = {0U, 25U, 50U, 75U, 100U, 50U, 0U};

    ESP_LOGI(TAG, "Motor PWM demo on GPIO %d. Use an external motor driver.", MOTOR_GPIO);

    for (size_t index = 0U; index < (sizeof(speed_steps) / sizeof(speed_steps[0])); index++) {
        ESP_LOGI(TAG, "Motor duty: %lu%%", (unsigned long)speed_steps[index]);
        ESP_ERROR_CHECK(pwm_set_duty_percent(&s_motor_pwm, speed_steps[index]));
        vTaskDelay(pdMS_TO_TICKS(DEMO_STEP_DELAY_MS));
    }
}
#endif

#if CONFIG_PWM_DEMO_ENABLE_BUZZER
/**
 * @brief Runs a passive buzzer tone demonstration.
 *
 * The function changes the LEDC timer frequency to generate different tones.
 * A 50 percent duty cycle is used to create a square-wave-like signal.
 */
static void demo_buzzer_tones(void)
{
    const uint32_t tones_hz[] = {523U, 659U, 784U, 1047U};
    const uint32_t half_duty = BUZZER_PWM_MAX_DUTY / 2U;

    ESP_LOGI(TAG, "Passive buzzer demo on GPIO %d", BUZZER_GPIO);

    for (size_t index = 0U; index < (sizeof(tones_hz) / sizeof(tones_hz[0])); index++) {
        ESP_LOGI(TAG, "Buzzer tone: %lu Hz", (unsigned long)tones_hz[index]);
        ESP_ERROR_CHECK(pwm_set_frequency(&s_buzzer_pwm, tones_hz[index]));
        ESP_ERROR_CHECK(pwm_set_duty_raw(&s_buzzer_pwm, half_duty));
        vTaskDelay(pdMS_TO_TICKS(DEMO_STEP_DELAY_MS));
    }

    ESP_ERROR_CHECK(pwm_set_duty_raw(&s_buzzer_pwm, 0U));
}
#endif

#if CONFIG_PWM_DEMO_ENABLE_SERVO
/**
 * @brief Sets a servo-style pulse width.
 *
 * Args:
 *     pulse_width_us: Pulse width in microseconds. Values are clamped to the
 *         configured safe pulse range.
 *
 * Returns:
 *     ESP_OK when the pulse width is updated successfully.
 */
static esp_err_t servo_set_pulse_us(uint32_t pulse_width_us)
{
    if (pulse_width_us < SERVO_MIN_PULSE_US) {
        pulse_width_us = SERVO_MIN_PULSE_US;
    }

    if (pulse_width_us > SERVO_MAX_PULSE_US) {
        pulse_width_us = SERVO_MAX_PULSE_US;
    }

    return pwm_set_duty_raw(&s_servo_pwm, servo_pulse_us_to_duty(pulse_width_us));
}

/**
 * @brief Runs a servo-style pulse-width demonstration.
 *
 * The GPIO must drive only the servo signal input. The servo power must come
 * from a suitable external supply with common ground connected to the ESP32.
 */
static void demo_servo_signal(void)
{
    const uint32_t pulse_steps_us[] = {
        SERVO_MIN_PULSE_US,
        SERVO_CENTER_PULSE_US,
        SERVO_MAX_PULSE_US,
        SERVO_CENTER_PULSE_US,
    };

    ESP_LOGI(TAG, "Servo-style PWM demo on GPIO %d. Use external servo power.", SERVO_GPIO);

    for (size_t index = 0U; index < (sizeof(pulse_steps_us) / sizeof(pulse_steps_us[0])); index++) {
        ESP_LOGI(TAG, "Servo pulse: %lu us", (unsigned long)pulse_steps_us[index]);
        ESP_ERROR_CHECK(servo_set_pulse_us(pulse_steps_us[index]));
        vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_DELAY_MS));
    }
}
#endif

/**
 * @brief Initializes the PWM demo.
 *
 * Returns:
 *     ESP_OK when the demo is initialized successfully.
 */
esp_err_t pwm_demo_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP32 PWM LEDC demo");

    ESP_RETURN_ON_ERROR(pwm_output_init(&s_led_pwm, 0U), TAG, "Failed to initialize LED PWM");

#if CONFIG_PWM_DEMO_ENABLE_MOTOR
    ESP_RETURN_ON_ERROR(pwm_output_init(&s_motor_pwm, 0U), TAG, "Failed to initialize motor PWM");
#endif

#if CONFIG_PWM_DEMO_ENABLE_BUZZER
    ESP_RETURN_ON_ERROR(pwm_output_init(&s_buzzer_pwm, 0U), TAG, "Failed to initialize buzzer PWM");
#endif

#if CONFIG_PWM_DEMO_ENABLE_SERVO
    ESP_RETURN_ON_ERROR(
        pwm_output_init(&s_servo_pwm, servo_pulse_us_to_duty(SERVO_CENTER_PULSE_US)),
        TAG,
        "Failed to initialize servo PWM"
    );
#endif

    ESP_LOGI(TAG, "PWM demo initialization complete");

    return ESP_OK;
}

void pwm_demo_run(void)
{
    while (true) {
        demo_led_dimming();

#if CONFIG_PWM_DEMO_ENABLE_MOTOR
        demo_motor_speed();
#endif

#if CONFIG_PWM_DEMO_ENABLE_BUZZER
        demo_buzzer_tones();
#endif

#if CONFIG_PWM_DEMO_ENABLE_SERVO
        demo_servo_signal();
#endif

        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}
