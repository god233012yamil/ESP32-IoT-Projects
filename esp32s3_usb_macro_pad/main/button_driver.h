#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Number of physical macro-pad buttons. */
#define BUTTON_DRIVER_BUTTON_COUNT 4U

/**
 * @brief Event generated when a debounced button is pressed.
 */
typedef struct
{
    uint8_t button_index;
} button_event_t;

/**
 * @brief Initializes the macro-pad button inputs.
 *
 * The four buttons are configured as active-low GPIO inputs with internal
 * pull-up resistors. Each switch must connect its GPIO pin to ground when
 * pressed.
 *
 * Returns:
 *     ESP_OK: The GPIO inputs were configured successfully.
 *     Other: An ESP-IDF GPIO driver error code.
 */
esp_err_t button_driver_init(void);

/**
 * @brief Runs the button scanner and publishes debounced press events.
 *
 * This function is intended to be used directly as a FreeRTOS task entry
 * point. It scans the inputs periodically, performs time-based software
 * debouncing, and sends one event for each released-to-pressed transition.
 *
 * Args:
 *     context: QueueHandle_t cast to void*. The queue receives button_event_t
 *         objects.
 */
void button_driver_task(void *context);

#ifdef __cplusplus
}
#endif

#endif
