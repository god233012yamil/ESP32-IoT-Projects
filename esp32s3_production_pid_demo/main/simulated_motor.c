#include "simulated_motor.h"

static float s_speed_rpm;

/**
 * Initializes the simulated motor.
 *
 * @param initial_speed_rpm The initial speed in RPM.
 */
void simulated_motor_init(float initial_speed_rpm)
{
    s_speed_rpm = initial_speed_rpm;
}

/**
 * Updates the simulated motor.
 *
 * @param pwm_percent The PWM percentage.
 * @param load_percent The load percentage.
 * @param dt_s The time step in seconds.
 */
void simulated_motor_update(float pwm_percent, float load_percent, float dt_s)
{
    const float max_speed_rpm = 3200.0f;
    const float time_constant_s = 0.35f;
    float load_drop_rpm;
    float target_speed_rpm;
    float speed_error_rpm;

    if (pwm_percent < 0.0f)
    {
        pwm_percent = 0.0f;
    }
    else if (pwm_percent > 100.0f)
    {
        pwm_percent = 100.0f;
    }

    if (load_percent < 0.0f)
    {
        load_percent = 0.0f;
    }
    else if (load_percent > 100.0f)
    {
        load_percent = 100.0f;
    }

    load_drop_rpm = load_percent * 8.0f;
    target_speed_rpm = ((pwm_percent / 100.0f) * max_speed_rpm) - load_drop_rpm;

    if (target_speed_rpm < 0.0f)
    {
        target_speed_rpm = 0.0f;
    }

    speed_error_rpm = target_speed_rpm - s_speed_rpm;
    s_speed_rpm += (dt_s / time_constant_s) * speed_error_rpm;
}

/**
 * Gets the speed of the simulated motor in RPM.
 *
 * @return The speed in RPM.
 */
float simulated_motor_get_speed_rpm(void)
{
    return s_speed_rpm;
}
