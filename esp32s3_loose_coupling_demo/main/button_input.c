#include "button_input.h"

#include "driver/gpio.h"
#include "esp_attr.h"

#define BUTTON_GPIO  0

static button_event_callback_t s_callback = 0;
static void *s_context = 0;
static volatile int s_last_level = 1;

/**
 * @brief Interrupt service routine for button GPIO events.
 *
 * Args:
 *     argument: Pointer to the interrupt argument.
 */
static void IRAM_ATTR button_gpio_isr(void *argument)
{
    (void)argument;

    int level = gpio_get_level(BUTTON_GPIO);
    app_event_t event =
    {
        .type = (level == 0) ? APP_EVENT_BUTTON_PRESSED : APP_EVENT_BUTTON_RELEASED
    };

    s_last_level = level;

    if (s_callback != 0)
    {
        s_callback(event, s_context);
    }
}

/**
 * @brief Initializes the button input module.
 */
void button_input_init(void)
{
    gpio_config_t config =
    {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    (void)gpio_config(&config);
    (void)gpio_install_isr_service(0);
    (void)gpio_isr_handler_add(BUTTON_GPIO, button_gpio_isr, 0);
}

/**
 * @brief Registers a callback for button events.
 *
 * Args:
 *     callback: Pointer to the callback function.
 *     context: Pointer to the context for the callback function.
 */
void button_input_register_callback(button_event_callback_t callback, void *context)
{
    s_callback = callback;
    s_context = context;
}
