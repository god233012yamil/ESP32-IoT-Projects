#ifndef PID_Q16_H
#define PID_Q16_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PID_Q16_SHIFT 16
#define PID_Q16_ONE   (1L << PID_Q16_SHIFT)

typedef int32_t pid_q16_t;

typedef struct
{
    pid_q16_t kp;
    pid_q16_t ki;
    pid_q16_t kd;
    pid_q16_t sample_time_s;
    pid_q16_t output_min;
    pid_q16_t output_max;
    pid_q16_t integral_min;
    pid_q16_t integral_max;
    pid_q16_t integral;
    pid_q16_t previous_measurement;
    pid_q16_t previous_output;
    bool first_run;
} pid_q16_controller_t;

/**
 * Converts a float to Q16.16 fixed-point format.
 *
 * Args:
 *     value: Floating-point value to convert.
 *
 * Returns:
 *     Q16.16 representation of the input value.
 */
pid_q16_t pid_q16_from_float(float value);

/**
 * Converts Q16.16 fixed-point data to float.
 *
 * Args:
 *     value: Q16.16 value to convert.
 *
 * Returns:
 *     Floating-point representation of the input value.
 */
float pid_q16_to_float(pid_q16_t value);

/**
 * Multiplies two Q16.16 values.
 *
 * Args:
 *     a: First Q16.16 operand.
 *     b: Second Q16.16 operand.
 *
 * Returns:
 *     Product in Q16.16 format.
 */
pid_q16_t pid_q16_mul(pid_q16_t a, pid_q16_t b);

/**
 * Initializes a Q16.16 fixed-point PID controller.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     kp: Proportional gain in Q16.16 format.
 *     ki: Integral gain in Q16.16 format.
 *     kd: Derivative gain in Q16.16 format.
 *     sample_time_s: Sample time in seconds, Q16.16 format.
 *     output_min: Minimum output in Q16.16 format.
 *     output_max: Maximum output in Q16.16 format.
 *
 * Returns:
 *     None.
 */
void pid_q16_init(pid_q16_controller_t *pid,
                  pid_q16_t kp,
                  pid_q16_t ki,
                  pid_q16_t kd,
                  pid_q16_t sample_time_s,
                  pid_q16_t output_min,
                  pid_q16_t output_max);

/**
 * Computes one Q16.16 fixed-point PID update.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     setpoint: Desired target value in Q16.16 format.
 *     measurement: Current process variable in Q16.16 format.
 *
 * Returns:
 *     Saturated controller output in Q16.16 format.
 */
pid_q16_t pid_q16_compute(pid_q16_controller_t *pid,
                           pid_q16_t setpoint,
                           pid_q16_t measurement);

#ifdef __cplusplus
}
#endif

#endif
