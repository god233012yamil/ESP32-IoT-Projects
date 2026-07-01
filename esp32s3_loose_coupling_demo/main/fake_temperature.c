#include "fake_temperature.h"

static int16_t s_temperature_c = 25;
static int16_t s_direction = 1;

/**
 * @brief Reads the temperature in Celsius.
 *
 * Args:
 *     temperature_c: Pointer to the variable to store the temperature.
 *
 * Returns:
 *     true if the temperature was read successfully, false otherwise.
 */
static bool fake_temperature_read_celsius(int16_t *temperature_c)
{
    if (temperature_c == 0)
    {
        return false;
    }

    *temperature_c = s_temperature_c;
    return true;
}

/**
 * @brief Gets the fake temperature source.
 *
 * Returns:
 *     Pointer to the fake temperature source.
 */
const temperature_source_t fake_temperature_source =
{
    .read_celsius = fake_temperature_read_celsius
};

/**
 * @brief Initializes the fake temperature source.
 *
 * Args:
 *     initial_temperature_c: Initial temperature in Celsius.
 */
void fake_temperature_init(int16_t initial_temperature_c)
{
    s_temperature_c = initial_temperature_c;
    s_direction = 1;
}

/**
 * @brief Updates the fake temperature.
 */
void fake_temperature_update(void)
{
    s_temperature_c = (int16_t)(s_temperature_c + s_direction);

    if (s_temperature_c >= 50)
    {
        s_temperature_c = 50;
        s_direction = -1;
    }
    else if (s_temperature_c <= 25)
    {
        s_temperature_c = 25;
        s_direction = 1;
    }
}
