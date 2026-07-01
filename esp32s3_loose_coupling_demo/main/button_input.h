#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include "app_event.h"
#include "event_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*button_event_callback_t)(app_event_t event, void *context);

/**
 * @brief Initializes the button input module.
 *
 * The module hides GPIO and interrupt details from the application. Events are
 * delivered through a callback registered by the application layer.
 */
void button_input_init(void);

/**
 * @brief Registers the application callback for button events.
 *
 * Args:
 *     callback: Function called when a button event occurs.
 *     context: User context passed back to the callback.
 */
void button_input_register_callback(button_event_callback_t callback, void *context);

#ifdef __cplusplus
}
#endif

#endif
