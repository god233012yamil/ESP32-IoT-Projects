#ifndef PID_F32_H
#define PID_F32_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float kp;
    float ki;
    float kd;
    float sample_time_s;
    float output_min;
    float output_max;
    float integral_min;
    float integral_max;
    float slew_rate_limit_per_s;
    float derivative_filter_alpha;
    float integral;
    float previous_measurement;
    float filtered_derivative;
    float previous_output;
    bool first_run;
} pid_f32_t;

/**
 * Initializes a floating-point PID controller instance.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     kp: Proportional gain.
 *     ki: Integral gain.
 *     kd: Derivative gain.
 *     sample_time_s: Fixed controller sample time in seconds.
 *     output_min: Minimum allowed controller output.
 *     output_max: Maximum allowed controller output.
 *
 * Returns:
 *     None.
 */
void pid_f32_init(pid_f32_t *pid,
                  float kp,
                  float ki,
                  float kd,
                  float sample_time_s,
                  float output_min,
                  float output_max);

/**
 * Configures the allowed range of the integral accumulator.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     integral_min: Minimum allowed integral state.
 *     integral_max: Maximum allowed integral state.
 *
 * Returns:
 *     None.
 */
void pid_f32_set_integral_limits(pid_f32_t *pid,
                                  float integral_min,
                                  float integral_max);

/**
 * Configures derivative low-pass filtering.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     alpha: Filter coefficient from 0.0 to 1.0. Higher values react faster.
 *
 * Returns:
 *     None.
 */
void pid_f32_set_derivative_filter(pid_f32_t *pid, float alpha);

/**
 * Configures the maximum output change rate.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     slew_rate_limit_per_s: Maximum output units per second. Use 0.0 to disable.
 *
 * Returns:
 *     None.
 */
void pid_f32_set_slew_rate_limit(pid_f32_t *pid, float slew_rate_limit_per_s);

/**
 * Updates PID gains at runtime.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     kp: Proportional gain.
 *     ki: Integral gain.
 *     kd: Derivative gain.
 *
 * Returns:
 *     None.
 */
void pid_f32_set_gains(pid_f32_t *pid, float kp, float ki, float kd);

/**
 * Preloads the PID internal state for smooth manual-to-automatic transition.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     current_output: Current actuator output used in manual mode.
 *     setpoint: Current controller setpoint.
 *     measurement: Current measured process variable.
 *
 * Returns:
 *     None.
 */
void pid_f32_set_manual_output(pid_f32_t *pid,
                               float current_output,
                               float setpoint,
                               float measurement);

/**
 * Resets runtime controller state while keeping configuration unchanged.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *
 * Returns:
 *     None.
 */
void pid_f32_reset(pid_f32_t *pid);

/**
 * Computes one floating-point PID controller update.
 *
 * The implementation uses derivative-on-measurement, conditional integration
 * anti-windup, derivative low-pass filtering, output clamping, and output
 * slew-rate limiting.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     setpoint: Desired target value.
 *     measurement: Current measured process variable.
 *
 * Returns:
 *     Saturated and slew-limited controller output.
 */
float pid_f32_compute(pid_f32_t *pid, float setpoint, float measurement);

#ifdef __cplusplus
}
#endif

#endif
