#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PID_STATUS_OK = 0,
    PID_STATUS_INVALID_ARG,
    PID_STATUS_INVALID_DT
} pid_status_t;

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
    float derivative_filter_tau_s;
} pid_config_t;

typedef struct
{
    pid_config_t config;
    float setpoint;
    float integral;
    float previous_measurement;
    float filtered_derivative;
    float output;
    float proportional_term;
    float integral_term;
    float derivative_term;
    bool initialized;
} pid_controller_t;

/**
 * @brief Initializes a PID controller instance.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     config: Pointer to the PID configuration values.
 *
 * Returns:
 *     PID_STATUS_OK when initialization succeeds.
 *     PID_STATUS_INVALID_ARG when an input pointer is NULL or the limits are invalid.
 *     PID_STATUS_INVALID_DT when the configured sample time is not greater than zero.
 */
pid_status_t pid_init(pid_controller_t *pid, const pid_config_t *config);

/**
 * @brief Resets the runtime state of a PID controller.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *
 * Returns:
 *     PID_STATUS_OK when the reset succeeds.
 *     PID_STATUS_INVALID_ARG when the PID pointer is NULL.
 */
pid_status_t pid_reset(pid_controller_t *pid);

/**
 * @brief Updates the desired setpoint of a PID controller.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     setpoint: Desired target value for the plant output.
 *
 * Returns:
 *     PID_STATUS_OK when the setpoint is updated.
 *     PID_STATUS_INVALID_ARG when the PID pointer is NULL.
 */
pid_status_t pid_set_setpoint(pid_controller_t *pid, float setpoint);

/**
 * @brief Computes one PID control step.
 *
 * The implementation uses derivative-on-measurement to avoid derivative kick when
 * the setpoint changes. It also applies conditional integration anti-windup so
 * the integral term does not continue accumulating in the wrong direction while
 * the output is saturated.
 *
 * Args:
 *     pid: Pointer to the PID controller instance.
 *     measurement: Current process variable measured from the plant.
 *     output: Pointer where the computed controller output will be stored.
 *
 * Returns:
 *     PID_STATUS_OK when the output is computed.
 *     PID_STATUS_INVALID_ARG when an input pointer is NULL or the controller is not initialized.
 */
pid_status_t pid_compute(pid_controller_t *pid, float measurement, float *output);

/**
 * @brief Clamps a floating-point value between a minimum and maximum limit.
 *
 * Args:
 *     value: Input value to clamp.
 *     min_value: Minimum allowed value.
 *     max_value: Maximum allowed value.
 *
 * Returns:
 *     The clamped value.
 */
float pid_clamp(float value, float min_value, float max_value);

#ifdef __cplusplus
}
#endif

#endif
