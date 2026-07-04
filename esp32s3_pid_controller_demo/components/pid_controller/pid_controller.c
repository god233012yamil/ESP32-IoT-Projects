#include "pid_controller.h"

/**
 * @brief Checks if the given limits are valid.
 *
 * Args:
 *     min_value: The minimum value.
 *     max_value: The maximum value.
 *
 * Returns:
 *     true if the limits are valid, false otherwise.
 */
static bool pid_limits_are_valid(float min_value, float max_value)
{
    return min_value < max_value;
}

/**
 * @brief Clamps a value within the specified limits.
 *
 * Args:
 *     value: The value to clamp.
 *     min_value: The minimum value.
 *     max_value: The maximum value.
 *
 * Returns:
 *     The clamped value.
 */
float pid_clamp(float value, float min_value, float max_value)
{
    if (value > max_value)
    {
        return max_value;
    }

    if (value < min_value)
    {
        return min_value;
    }

    return value;
}

/**
 * @brief Initializes the PID controller.
 *
 * Args:
 *     pid: A pointer to the PID controller instance.
 *     config: A pointer to the PID configuration structure.
 *
 * Returns:
 *     The status of the initialization operation.
 */
pid_status_t pid_init(pid_controller_t *pid, const pid_config_t *config)
{
    if ((pid == 0) || (config == 0))
    {
        return PID_STATUS_INVALID_ARG;
    }

    if (config->sample_time_s <= 0.0f)
    {
        return PID_STATUS_INVALID_DT;
    }

    if (!pid_limits_are_valid(config->output_min, config->output_max) ||
        !pid_limits_are_valid(config->integral_min, config->integral_max))
    {
        return PID_STATUS_INVALID_ARG;
    }

    pid->config = *config;
    pid->setpoint = 0.0f;
    pid->initialized = true;

    return pid_reset(pid);
}

/**
 * @brief Resets the PID controller.
 *
 * Args:
 *     pid: A pointer to the PID controller instance.
 *
 * Returns:
 *     The status of the reset operation.
 */
pid_status_t pid_reset(pid_controller_t *pid)
{
    if (pid == 0)
    {
        return PID_STATUS_INVALID_ARG;
    }

    pid->integral = 0.0f;
    pid->previous_measurement = 0.0f;
    pid->filtered_derivative = 0.0f;
    pid->output = 0.0f;
    pid->proportional_term = 0.0f;
    pid->integral_term = 0.0f;
    pid->derivative_term = 0.0f;

    return PID_STATUS_OK;
}

/**
 * @brief Sets the setpoint for the PID controller.
 *
 * Args:
 *     pid: A pointer to the PID controller instance.
 *     setpoint: The desired setpoint.
 *
 * Returns:
 *     The status of the operation.
 */
pid_status_t pid_set_setpoint(pid_controller_t *pid, float setpoint)
{
    if (pid == 0)
    {
        return PID_STATUS_INVALID_ARG;
    }

    pid->setpoint = setpoint;
    return PID_STATUS_OK;
}

/**
 * @brief Computes the PID output.
 *
 * Args:
 *     pid: A pointer to the PID controller instance.
 *     measurement: The current measurement.
 *     output: A pointer to the computed output.
 *
 * Returns:
 *     The status of the operation.
 */
pid_status_t pid_compute(pid_controller_t *pid, float measurement, float *output)
{
    if ((pid == 0) || (output == 0) || !pid->initialized)
    {
        return PID_STATUS_INVALID_ARG;
    }

    const float dt = pid->config.sample_time_s;
    const float error = pid->setpoint - measurement;

    pid->proportional_term = pid->config.kp * error;

    const float raw_derivative = -(measurement - pid->previous_measurement) / dt;
    float derivative_alpha = 1.0f;

    if (pid->config.derivative_filter_tau_s > 0.0f)
    {
        derivative_alpha = dt / (pid->config.derivative_filter_tau_s + dt);
    }

    pid->filtered_derivative += derivative_alpha * (raw_derivative - pid->filtered_derivative);
    pid->derivative_term = pid->config.kd * pid->filtered_derivative;

    const float candidate_integral = pid_clamp(pid->integral + (error * dt),
                                               pid->config.integral_min,
                                               pid->config.integral_max);
    const float candidate_integral_term = pid->config.ki * candidate_integral;
    const float candidate_output = pid->proportional_term + candidate_integral_term + pid->derivative_term;
    const float saturated_output = pid_clamp(candidate_output, pid->config.output_min, pid->config.output_max);

    const bool output_is_high = candidate_output > pid->config.output_max;
    const bool output_is_low = candidate_output < pid->config.output_min;
    const bool error_pushes_down = error < 0.0f;
    const bool error_pushes_up = error > 0.0f;
    const bool allow_integration = (!output_is_high && !output_is_low) ||
                                   (output_is_high && error_pushes_down) ||
                                   (output_is_low && error_pushes_up);

    if (allow_integration)
    {
        pid->integral = candidate_integral;
    }

    pid->integral_term = pid->config.ki * pid->integral;
    pid->output = pid_clamp(pid->proportional_term + pid->integral_term + pid->derivative_term,
                            pid->config.output_min,
                            pid->config.output_max);
    pid->previous_measurement = measurement;
    *output = pid->output;

    (void)saturated_output;

    return PID_STATUS_OK;
}
