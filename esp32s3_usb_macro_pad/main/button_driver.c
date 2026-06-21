#include "button_driver.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/task.h"

#define BUTTON_SCAN_PERIOD_MS 5U
#define BUTTON_DEBOUNCE_TIME_MS 20U

static const char *TAG = "button_driver";

static const gpio_num_t s_button_pins[BUTTON_DRIVER_BUTTON_COUNT] =
{
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7,
};

typedef struct
{
    bool raw_pressed;
    bool stable_pressed;
    TickType_t raw_change_tick;
} button_state_t;

/**
 * @brief Reads one active-low button input.
 *
 * Args:
 *     button_index: Zero-based index into the configured button table.
 *
 * Returns:
 *     true: The switch is currently pressed.
 *     false: The switch is currently released.
 */
static bool button_driver_read_pressed(size_t button_index)
{
    return gpio_get_level(s_button_pins[button_index]) == 0;
}

/**
 * @brief Initializes the button driver.
 *
 * This function configures the GPIO pins for button inputs.
 *
 * Returns:
 *     ESP_OK: The button driver was initialized successfully.
 *     Other: Error returned by the GPIO configuration function.
 */
esp_err_t button_driver_init(void)
{
    uint64_t pin_mask = 0U;

    for (size_t index = 0; index < BUTTON_DRIVER_BUTTON_COUNT; ++index)
    {
        pin_mask |= (1ULL << s_button_pins[index]);
    }

    const gpio_config_t config =
    {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&config);
}

/**
 * @brief Task function for the button driver.
 *
 * This task periodically reads the state of the buttons and sends events to the macro engine task.
 *
 * Args:
 *     context: Pointer to the FreeRTOS queue handle.
 */
void button_driver_task(void *context)
{
    QueueHandle_t event_queue = (QueueHandle_t)context;
    button_state_t states[BUTTON_DRIVER_BUTTON_COUNT] = {0};
    const TickType_t scan_period = pdMS_TO_TICKS(BUTTON_SCAN_PERIOD_MS);
    const TickType_t debounce_ticks = pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS);

    configASSERT(event_queue != NULL);

    const TickType_t initial_tick = xTaskGetTickCount();

    for (size_t index = 0; index < BUTTON_DRIVER_BUTTON_COUNT; ++index)
    {
        const bool pressed = button_driver_read_pressed(index);
        states[index].raw_pressed = pressed;
        states[index].stable_pressed = pressed;
        states[index].raw_change_tick = initial_tick;
    }

    ESP_LOGI(TAG, "Button scanner started");

    while (true)
    {
        const TickType_t now = xTaskGetTickCount();

        for (size_t index = 0; index < BUTTON_DRIVER_BUTTON_COUNT; ++index)
        {
            const bool current_raw_pressed = button_driver_read_pressed(index);

            if (current_raw_pressed != states[index].raw_pressed)
            {
                states[index].raw_pressed = current_raw_pressed;
                states[index].raw_change_tick = now;
            }

            const bool debounce_elapsed =
                (now - states[index].raw_change_tick) >= debounce_ticks;

            if (debounce_elapsed &&
                (states[index].stable_pressed != states[index].raw_pressed))
            {
                states[index].stable_pressed = states[index].raw_pressed;

                // Only the press edge generates a macro event.
                if (states[index].stable_pressed)
                {
                    const button_event_t event =
                    {
                        .button_index = (uint8_t)index,
                    };

                    if (xQueueSend(event_queue, &event, 0) != pdPASS)
                    {
                        ESP_LOGW(TAG, "Macro queue full; button %u dropped",
                                 (unsigned int)(index + 1U));
                    }
                }
            }
        }

        vTaskDelay(scan_period);
    }
}
