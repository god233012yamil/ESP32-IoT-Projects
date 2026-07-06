#ifndef SIMULATED_MOTOR_H
#define SIMULATED_MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the simulated motor plant.
 *
 * Args:
 *     initial_speed_rpm: Initial simulated motor speed in RPM.
 *
 * Returns:
 *     None.
 */
void simulated_motor_init(float initial_speed_rpm);

/**
 * Updates the simulated motor plant using a first-order dynamic model.
 *
 * Args:
 *     pwm_percent: Motor command from 0.0 to 100.0 percent.
 *     load_percent: Load disturbance from 0.0 to 100.0 percent.
 *     dt_s: Simulation update period in seconds.
 *
 * Returns:
 *     None.
 */
void simulated_motor_update(float pwm_percent, float load_percent, float dt_s);

/**
 * Reads the simulated motor speed.
 *
 * Returns:
 *     Current simulated motor speed in RPM.
 */
float simulated_motor_get_speed_rpm(void);

#ifdef __cplusplus
}
#endif

#endif
