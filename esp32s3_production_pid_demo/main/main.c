#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "pid_f32.h"
#include "pid_q16.h"
#include "simulated_motor.h"

#define DEMO_PWM_GPIO              2
#define DEMO_PWM_FREQ_HZ           20000
#define DEMO_PWM_RESOLUTION        LEDC_TIMER_10_BIT
#define DEMO_PWM_MAX_DUTY          1023
#define PID_SAMPLE_TIME_MS         10
#define PID_SAMPLE_TIME_S          0.010f
#define PID_CONTROL_TASK_STACK     4096
#define PID_CONTROL_TASK_PRIORITY  5

static const char *TAG = "pid_demo";
static pid_f32_t s_motor_pid;

/** Initializes the PWM output for the motor command.
 *
 * @return ESP_OK if PWM was configured successfully, otherwise an ESP-IDF error code.
 */
static esp_err_t pwm_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = DEMO_PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = DEMO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    ledc_channel_config_t channel_config = {
        .gpio_num = DEMO_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 0,
    };

    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "Failed to configure LEDC timer");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "Failed to configure LEDC channel");

    return ESP_OK;
}


/**
 * Sets the PWM duty cycle using a percentage command.
 *
 * @param duty_percent The requested duty cycle from 0.0 to 100.0 percent.
 */
static void pwm_set_duty_percent(float duty_percent)
{
    uint32_t duty_counts;

    if (duty_percent < 0.0f)
    {
        duty_percent = 0.0f;
    }
    else if (duty_percent > 100.0f)
    {
        duty_percent = 100.0f;
    }

    duty_counts = (uint32_t)((duty_percent / 100.0f) * (float)DEMO_PWM_MAX_DUTY);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_counts);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/** Runs a small fixed-point PID smoke test to demonstrate the Q16.16 API.
 *
 * @return None.
 */
static void run_fixed_point_pid_demo(void)
{
    pid_q16_controller_t pid_q16;
    pid_q16_t output;

    pid_q16_init(&pid_q16,
                 pid_q16_from_float(0.25f),
                 pid_q16_from_float(0.10f),
                 pid_q16_from_float(0.01f),
                 pid_q16_from_float(PID_SAMPLE_TIME_S),
                 pid_q16_from_float(0.0f),
                 pid_q16_from_float(100.0f));

    output = pid_q16_compute(&pid_q16,
                             pid_q16_from_float(1500.0f),
                             pid_q16_from_float(1200.0f));

    ESP_LOGI(TAG, "Q16.16 PID smoke test output: %.2f", pid_q16_to_float(output));
}

/** Periodic PID control task.
 *
 * This task controls a simulated motor plant and also updates a real PWM pin so
 * the controller output can be observed with an oscilloscope or logic analyzer.
 *
 * @param arg Unused task argument.
 * @return None. This task does not return.
 */
static void pid_control_task(void *arg)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t sample_count = 0;
    float setpoint_rpm = 1800.0f;
    float load_percent = 15.0f;
    float measurement_rpm;
    float pwm_percent;

    (void)arg;

    while (true)
    {
        measurement_rpm = simulated_motor_get_speed_rpm();
        pwm_percent = pid_f32_compute(&s_motor_pid, setpoint_rpm, measurement_rpm);

        pwm_set_duty_percent(pwm_percent);
        simulated_motor_update(pwm_percent, load_percent, PID_SAMPLE_TIME_S);

        sample_count++;

        // Step changes create visible responses for tuning and data logging.
        if (sample_count == 500U)
        {
            setpoint_rpm = 2400.0f;
        }
        else if (sample_count == 1000U)
        {
            load_percent = 45.0f;
        }
        else if (sample_count == 1500U)
        {
            setpoint_rpm = 1200.0f;
        }
        else if (sample_count == 2000U)
        {
            load_percent = 10.0f;
            sample_count = 0U;
        }

        if ((sample_count % 50U) == 0U)
        {
            ESP_LOGI(TAG,
                     "setpoint=%.1f rpm, speed=%.1f rpm, pwm=%.1f %%, load=%.1f %%",
                     setpoint_rpm,
                     measurement_rpm,
                     pwm_percent,
                     load_percent);
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(PID_SAMPLE_TIME_MS));
    }
}

/** Application entry point.
 *
 * @return None.
 */
void app_main(void)
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize the LEDC PWM output for the motor command.
    ESP_ERROR_CHECK(pwm_init());
    
    // Initialize the simulated motor plant and PID controller.
    simulated_motor_init(0.0f);

    // Initialize the floating-point PID controller with gains and limits.
    pid_f32_init(&s_motor_pid,
                 0.08f,
                 0.35f,
                 0.002f,
                 PID_SAMPLE_TIME_S,
                 0.0f,
                 100.0f);

    // Set the integral limits for the PID controller.
    pid_f32_set_integral_limits(&s_motor_pid, -200.0f, 200.0f);
    // Set the derivative filter for the PID controller.
    pid_f32_set_derivative_filter(&s_motor_pid, 0.12f);
    // Set the slew rate limit for the PID controller.
    pid_f32_set_slew_rate_limit(&s_motor_pid, 250.0f);
    // Set the manual output for the PID controller.
    pid_f32_set_manual_output(&s_motor_pid, 0.0f, 1800.0f, 0.0f);

    // Run a small fixed-point PID smoke test to demonstrate the Q16.16 API.
    run_fixed_point_pid_demo();

    ESP_LOGI(TAG, "Production-grade PID demo started");
    ESP_LOGI(TAG, "PWM output is available on GPIO%d", DEMO_PWM_GPIO);

    // Create the PID control task that runs periodically to control the simulated motor.
    xTaskCreate(pid_control_task,
                "pid_control_task",
                PID_CONTROL_TASK_STACK,
                NULL,
                PID_CONTROL_TASK_PRIORITY,
                NULL);
}
