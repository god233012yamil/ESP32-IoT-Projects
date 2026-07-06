#include "pid_q16.h"

#include <stddef.h>

/**
 * Clamps a Q16 value to a specified range.
 *
 * @param value The value to clamp.
 * @param min_value The minimum value.
 * @param max_value The maximum value.
 * @return The clamped value.
 */
static pid_q16_t clamp_q16(pid_q16_t value, pid_q16_t min_value, pid_q16_t max_value)
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
 * Converts a float value to a Q16 value.
 *
 * @param value The float value to convert.
 * @return The converted Q16 value.
 */
pid_q16_t pid_q16_from_float(float value)
{
    return (pid_q16_t)(value * (float)PID_Q16_ONE);
}

/**
 * Converts a Q16 value to a float value.
 *
 * @param value The Q16 value to convert.
 * @return The converted float value.
 */
float pid_q16_to_float(pid_q16_t value)
{
    return (float)value / (float)PID_Q16_ONE;
}

/**
 * Multiplies two Q16 values.
 *
 * @param a The first Q16 value.
 * @param b The second Q16 value.
 * @return The product of the two values.
 */
pid_q16_t pid_q16_mul(pid_q16_t a, pid_q16_t b)
{
    return (pid_q16_t)(((int64_t)a * (int64_t)b) >> PID_Q16_SHIFT);
}

/**
 * Initializes the Q16 PID controller.
 *
 * @param pid The PID controller instance.
 * @param kp The proportional gain.
 * @param ki The integral gain.
 * @param kd The derivative gain.
 * @param sample_time_s The sample time in seconds.
 * @param output_min The minimum output value.
 * @param output_max The maximum output value.
 */
void pid_q16_init(pid_q16_controller_t *pid,
                  pid_q16_t kp,
                  pid_q16_t ki,
                  pid_q16_t kd,
                  pid_q16_t sample_time_s,
                  pid_q16_t output_min,
                  pid_q16_t output_max)
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
    pid->integral = 0;
    pid->previous_measurement = 0;
    pid->previous_output = 0;
    pid->first_run = true;
}

/**
 * Computes the Q16 PID output.
 *
 * @param pid The PID controller instance.
 * @param setpoint The setpoint value.
 * @param measurement The measurement value.
 * @return The computed PID output.
 */
pid_q16_t pid_q16_compute(pid_q16_controller_t *pid,
                           pid_q16_t setpoint,
                           pid_q16_t measurement)
{
    pid_q16_t error;
    pid_q16_t proportional;
    pid_q16_t derivative;
    pid_q16_t output_unsaturated;
    pid_q16_t output;
    bool can_integrate;

    if (pid == NULL || pid->sample_time_s == 0)
    {
        return 0;
    }

    if (pid->first_run)
    {
        pid->previous_measurement = measurement;
        pid->first_run = false;
    }

    error = setpoint - measurement;
    proportional = pid_q16_mul(pid->kp, error);

    derivative = -pid_q16_mul(pid->kd,
                              (pid_q16_t)(((int64_t)(measurement - pid->previous_measurement) << PID_Q16_SHIFT) /
                                          pid->sample_time_s));

    output_unsaturated = proportional + pid_q16_mul(pid->ki, pid->integral) + derivative;

    can_integrate = ((output_unsaturated < pid->output_max) && (output_unsaturated > pid->output_min)) ||
                    ((output_unsaturated >= pid->output_max) && (error < 0)) ||
                    ((output_unsaturated <= pid->output_min) && (error > 0));

    if (can_integrate)
    {
        pid->integral += pid_q16_mul(error, pid->sample_time_s);
        pid->integral = clamp_q16(pid->integral, pid->integral_min, pid->integral_max);
    }

    output = proportional + pid_q16_mul(pid->ki, pid->integral) + derivative;
    output = clamp_q16(output, pid->output_min, pid->output_max);

    pid->previous_measurement = measurement;
    pid->previous_output = output;

    return output;
}
