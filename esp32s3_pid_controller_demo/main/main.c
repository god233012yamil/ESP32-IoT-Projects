#include <math.h>
#include <stdio.h>

#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pid_controller.h"

#define DEMO_PWM_GPIO              (48)
#define DEMO_PWM_TIMER             LEDC_TIMER_0
#define DEMO_PWM_MODE              LEDC_LOW_SPEED_MODE
#define DEMO_PWM_CHANNEL           LEDC_CHANNEL_0
#define DEMO_PWM_FREQUENCY_HZ      (5000)
#define DEMO_PWM_RESOLUTION        LEDC_TIMER_13_BIT
#define DEMO_PWM_MAX_DUTY          ((1U << 13U) - 1U)

#define CONTROL_PERIOD_MS          (10)
#define CONTROL_PERIOD_S           (0.010f)
#define LOG_PERIOD_TICKS           (25)

static const char *TAG = "pid_demo";

// Structure representing a first-order plant model
typedef struct
{
    float output;
    float time_constant_s;
    float gain;
} first_order_plant_t;

/**
 * @brief Configures the LEDC peripheral used as the actuator output.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     ESP_OK when the LEDC timer and channel are configured successfully.
 *     An ESP-IDF error code when configuration fails.
 */
static esp_err_t pwm_output_init(void)
{
    // Configure the LEDC timer for PWM generation
    const ledc_timer_config_t timer_config = {
        .speed_mode = DEMO_PWM_MODE,
        .duty_resolution = DEMO_PWM_RESOLUTION,
        .timer_num = DEMO_PWM_TIMER,
        .freq_hz = DEMO_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };

    // Configure the LEDC channel for PWM output
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "Failed to configure LEDC timer");

    // Configure the LEDC channel for PWM output
    const ledc_channel_config_t channel_config = {
        .gpio_num = DEMO_PWM_GPIO,
        .speed_mode = DEMO_PWM_MODE,
        .channel = DEMO_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = DEMO_PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 0,
    };

    // Configure the LEDC channel for PWM output
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "Failed to configure LEDC channel");

    return ESP_OK;
}

/**
 * @brief Updates the PWM duty cycle from a normalized controller output.
 *
 * Args:
 *     control_output: Controller output in the range 0.0 to 100.0.
 *
 * Returns:
 *     ESP_OK when the duty cycle is updated successfully.
 *     An ESP-IDF error code when the LEDC driver update fails.
 */
static esp_err_t pwm_output_write(float control_output)
{
    const float limited_output = pid_clamp(control_output, 0.0f, 100.0f);
    const uint32_t duty = (uint32_t)((limited_output / 100.0f) * (float)DEMO_PWM_MAX_DUTY);

    // Set the LEDC duty cycle based on the normalized controller output
    ESP_RETURN_ON_ERROR(ledc_set_duty(DEMO_PWM_MODE, DEMO_PWM_CHANNEL, duty), TAG, "Failed to set LEDC duty");
    
    // Update the LEDC duty cycle to apply the new value
    ESP_RETURN_ON_ERROR(ledc_update_duty(DEMO_PWM_MODE, DEMO_PWM_CHANNEL), TAG, "Failed to update LEDC duty");

    return ESP_OK;
}

/**
 * @brief Updates a first-order plant simulation.
 *
 * The model approximates slow physical behavior such as motor inertia or thermal
 * mass. The controller output is treated as actuator effort in percent.
 *
 * Args:
 *     plant: Pointer to the plant model.
 *     input: Actuator command in percent.
 *     dt_s: Simulation step time in seconds.
 *
 * Returns:
 *     The updated process variable.
 */
static float first_order_plant_update(first_order_plant_t *plant, float input, float dt_s)
{
    const float target = plant->gain * input;
    const float derivative = (target - plant->output) / plant->time_constant_s;

    plant->output += derivative * dt_s;
    return plant->output;
}

/**
 * @brief Returns a demonstration setpoint that changes over time.
 *
 * Args:
 *     elapsed_ticks: Number of control-loop iterations since startup.
 *
 * Returns:
 *     The active setpoint for the current time segment.
 */
static float demo_get_setpoint(uint32_t elapsed_ticks)
{
    const uint32_t elapsed_ms = elapsed_ticks * CONTROL_PERIOD_MS;
    const uint32_t phase_ms = elapsed_ms % 20000U;

    if (phase_ms < 5000U)
    {
        return 30.0f;
    }

    if (phase_ms < 10000U)
    {
        return 70.0f;
    }

    if (phase_ms < 15000U)
    {
        return 45.0f;
    }

    return 85.0f;
}

/**
 * @brief Periodic control task that runs the PID loop.
 *
 * Args:
 *     pvParameters: Unused FreeRTOS task parameter.
 *
 * Returns:
 *     This task does not return.
 */
static void pid_control_task(void *pvParameters)
{
    (void)pvParameters;

    // Create and configure the PID controller
    pid_controller_t pid;
    const pid_config_t pid_config = {
        .kp = 1.20f,
        .ki = 0.80f,
        .kd = 0.08f,
        .sample_time_s = CONTROL_PERIOD_S,
        .output_min = 0.0f,
        .output_max = 100.0f,
        .integral_min = -80.0f,
        .integral_max = 80.0f,
        .derivative_filter_tau_s = 0.050f,
    };

    // Create a first-order plant model to simulate the process variable
    first_order_plant_t plant = {
        .output = 0.0f,
        .time_constant_s = 1.20f,
        .gain = 1.0f,
    };

    // Initialize the PID controller
    ESP_ERROR_CHECK(pid_init(&pid, &pid_config));

    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t control_ticks = 0;

    while (true)
    {
        // Get the current setpoint and compute the PID output
        const float setpoint = demo_get_setpoint(control_ticks);
        float control_output = 0.0f;

        // Update the PID controller with the current setpoint
        ESP_ERROR_CHECK(pid_set_setpoint(&pid, setpoint));
        
        // Compute the PID output based on the current process variable (plant output)
        ESP_ERROR_CHECK(pid_compute(&pid, plant.output, &control_output));

        // Update the simulated plant with the computed control output
        const float process_variable = first_order_plant_update(&plant, control_output, CONTROL_PERIOD_S);
        
        // Write the control output to the PWM actuator
        ESP_ERROR_CHECK(pwm_output_write(control_output));

        if ((control_ticks % LOG_PERIOD_TICKS) == 0U)
        {
            const float error = setpoint - process_variable;
            const uint32_t duty = (uint32_t)((pid.output / 100.0f) * (float)DEMO_PWM_MAX_DUTY);

            ESP_LOGI(TAG,
                     "SP=%.2f PV=%.2f ERR=%.2f OUT=%.2f P=%.2f I=%.2f D=%.2f DUTY=%lu",
                     setpoint,
                     process_variable,
                     error,
                     pid.output,
                     pid.proportional_term,
                     pid.integral_term,
                     pid.derivative_term,
                     (unsigned long)duty);
        }

        control_ticks++;
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

/**
 * @brief Application entry point.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     None.
 */
void app_main(void)
{
    // Initialize the LEDC peripheral for PWM output
    ESP_ERROR_CHECK(pwm_output_init());

    ESP_LOGI(TAG, "ESP32-S3 PID controller demo started");
    ESP_LOGI(TAG, "Control period: %d ms", CONTROL_PERIOD_MS);

    // Create the PID control task pinned to core 0
    const BaseType_t task_created = xTaskCreatePinnedToCore(pid_control_task,
                                                            "pid_control",
                                                            4096,
                                                            NULL,
                                                            5,
                                                            NULL,
                                                            0);

    if (task_created != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create PID control task");
    }
}
