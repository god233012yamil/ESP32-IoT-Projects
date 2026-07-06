#include "pid_f32.h"

#include <stddef.h>

/**
 * Clamps a float value between a minimum and maximum.
 *
 * @param value The value to clamp.
 * @param min_value The minimum value.
 * @param max_value The maximum value.
 * @return The clamped value.
 */
static float clamp_f32(float value, float min_value, float max_value)
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
 * Initializes a floating-point PID controller.
 *
 * @param pid The PID controller instance.
 * @param kp The proportional gain.
 * @param ki The integral gain.
 * @param kd The derivative gain.
 * @param sample_time_s The sampling time in seconds.
 * @param output_min The minimum output value.
 * @param output_max The maximum output value.
 */
void pid_f32_init(pid_f32_t *pid,
                  float kp,
                  float ki,
                  float kd,
                  float sample_time_s,
                  float output_min,
                  float output_max)
{
    if (pid == NULL)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->sample_time_s = sample_time_s;
    pid->output_min = output_min;
    pid->output_max = output_max;
    pid->integral_min = output_min;
    pid->integral_max = output_max;
    pid->slew_rate_limit_per_s = 0.0f;
    pid->derivative_filter_alpha = 0.15f;
    pid_f32_reset(pid);
}

/**
 * Sets the integral limits for the PID controller.
 *
 * @param pid The PID controller instance.
 * @param integral_min The minimum integral value.
 * @param integral_max The maximum integral value.
 */
void pid_f32_set_integral_limits(pid_f32_t *pid,
                                  float integral_min,
                                  float integral_max)
{
    if (pid == NULL)
    {
        return;
    }

    pid->integral_min = integral_min;
    pid->integral_max = integral_max;
    pid->integral = clamp_f32(pid->integral, integral_min, integral_max);
}

/**
 * Sets the derivative filter for the PID controller.
 *
 * @param pid The PID controller instance.
 * @param alpha The filter coefficient.
 */
void pid_f32_set_derivative_filter(pid_f32_t *pid, float alpha)
{
    if (pid == NULL)
    {
        return;
    }

    pid->derivative_filter_alpha = clamp_f32(alpha, 0.0f, 1.0f);
}

/**
 * Sets the slew rate limit for the PID controller.
 *
 * @param pid The PID controller instance.
 * @param slew_rate_limit_per_s The slew rate limit per second.
 */
void pid_f32_set_slew_rate_limit(pid_f32_t *pid, float slew_rate_limit_per_s)
{
    if (pid == NULL)
    {
        return;
    }

    pid->slew_rate_limit_per_s = slew_rate_limit_per_s < 0.0f ? 0.0f : slew_rate_limit_per_s;
}

/**
 * Sets the PID gains for the controller.
 *
 * @param pid The PID controller instance.
 * @param kp The proportional gain.
 * @param ki The integral gain.
 * @param kd The derivative gain.
 */
void pid_f32_set_gains(pid_f32_t *pid, float kp, float ki, float kd)
{
    if (pid == NULL)
    {
        return;
    }

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

/**
 * Sets the manual output for the PID controller.
 *
 * @param pid The PID controller instance.
 * @param current_output The current output value.
 * @param setpoint The setpoint value.
 * @param measurement The measurement value.
 */
void pid_f32_set_manual_output(pid_f32_t *pid,
                               float current_output,
                               float setpoint,
                               float measurement)
{
    float error;
    float proportional;

    if (pid == NULL)
    {
        return;
    }

    current_output = clamp_f32(current_output, pid->output_min, pid->output_max);
    error = setpoint - measurement;
    proportional = pid->kp * error;

    if (pid->ki != 0.0f)
    {
        pid->integral = (current_output - proportional) / pid->ki;
        pid->integral = clamp_f32(pid->integral, pid->integral_min, pid->integral_max);
    }
    else
    {
        pid->integral = 0.0f;
    }

    pid->previous_output = current_output;
    pid->previous_measurement = measurement;
    pid->filtered_derivative = 0.0f;
    pid->first_run = false;
}

/**
 * Resets the PID controller.
 *
 * @param pid The PID controller instance.
 */
void pid_f32_reset(pid_f32_t *pid)
{
    if (pid == NULL)
    {
        return;
    }

    pid->integral = 0.0f;
    pid->previous_measurement = 0.0f;
    pid->filtered_derivative = 0.0f;
    pid->previous_output = 0.0f;
    pid->first_run = true;
}

/**
 * Computes the PID output.
 *
 * @param pid The PID controller instance.
 * @param setpoint The setpoint value.
 * @param measurement The measurement value.
 * @return The computed PID output.
 */
float pid_f32_compute(pid_f32_t *pid, float setpoint, float measurement)
{
    float error;
    float proportional;
    float derivative_raw;
    float derivative;
    float output_unsaturated;
    float output;
    float max_delta;
    float output_delta;
    bool can_integrate;

    if (pid == NULL || pid->sample_time_s <= 0.0f)
    {
        return 0.0f;
    }

    if (pid->first_run)
    {
        pid->previous_measurement = measurement;
        pid->filtered_derivative = 0.0f;
        pid->previous_output = clamp_f32(pid->previous_output, pid->output_min, pid->output_max);
        pid->first_run = false;
    }

    error = setpoint - measurement;
    proportional = pid->kp * error;

    derivative_raw = -(measurement - pid->previous_measurement) / pid->sample_time_s;
    pid->filtered_derivative += pid->derivative_filter_alpha *
                                (derivative_raw - pid->filtered_derivative);
    derivative = pid->kd * pid->filtered_derivative;

    output_unsaturated = proportional + (pid->ki * pid->integral) + derivative;

    can_integrate = ((output_unsaturated < pid->output_max) && (output_unsaturated > pid->output_min)) ||
                    ((output_unsaturated >= pid->output_max) && (error < 0.0f)) ||
                    ((output_unsaturated <= pid->output_min) && (error > 0.0f));

    if (can_integrate)
    {
        pid->integral += error * pid->sample_time_s;
        pid->integral = clamp_f32(pid->integral, pid->integral_min, pid->integral_max);
    }

    output = proportional + (pid->ki * pid->integral) + derivative;
    output = clamp_f32(output, pid->output_min, pid->output_max);

    if (pid->slew_rate_limit_per_s > 0.0f)
    {
        max_delta = pid->slew_rate_limit_per_s * pid->sample_time_s;
        output_delta = output - pid->previous_output;

        if (output_delta > max_delta)
        {
            output = pid->previous_output + max_delta;
        }
        else if (output_delta < -max_delta)
        {
            output = pid->previous_output - max_delta;
        }
    }

    pid->previous_measurement = measurement;
    pid->previous_output = output;

    return output;
}
