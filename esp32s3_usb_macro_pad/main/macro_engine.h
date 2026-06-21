#ifndef MACRO_ENGINE_H
#define MACRO_ENGINE_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Identifiers for the example macro assignments.
 */
typedef enum
{
    MACRO_ID_COPY = 0,
    MACRO_ID_PASTE,
    MACRO_ID_SAVE,
    MACRO_ID_TASK_MANAGER,
    MACRO_ID_COUNT
} macro_id_t;

/**
 * @brief Runs the macro execution service.
 *
 * This function is intended to be used directly as a FreeRTOS task entry
 * point. It receives button events, maps them to macros, and sends the
 * corresponding USB HID keyboard shortcut.
 *
 * Args:
 *     context: QueueHandle_t cast to void*. The queue must contain
 *         button_event_t objects.
 */
void macro_engine_task(void *context);

#ifdef __cplusplus
}
#endif

#endif
