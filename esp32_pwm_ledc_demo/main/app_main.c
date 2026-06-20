#include "pwm_ledc_demo.h"
#include "esp_err.h"

/**
 * @brief Application entry point.
 *
 * The ESP-IDF runtime calls this function after system initialization. The
 * function initializes the LEDC PWM examples and starts the repeating demo.
 */
void app_main(void)
{
    ESP_ERROR_CHECK(pwm_demo_init());
    pwm_demo_run();
}
