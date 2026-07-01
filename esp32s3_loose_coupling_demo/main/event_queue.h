#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdbool.h>

#include "app_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    QueueHandle_t handle;
} event_queue_t;

/**
 * @brief Creates the application event queue.
 *
 * Args:
 *     queue: Queue object to initialize.
 *     length: Maximum number of queued events.
 *
 * Returns:
 *     true if the queue was created successfully, otherwise false.
 */
bool event_queue_init(event_queue_t *queue, UBaseType_t length);

/**
 * @brief Pushes an event from task context.
 *
 * Args:
 *     queue: Queue object.
 *     event: Event to enqueue.
 *     timeout_ticks: Maximum time to wait for queue space.
 *
 * Returns:
 *     true if the event was queued, otherwise false.
 */
bool event_queue_push(event_queue_t *queue,
                      const app_event_t *event,
                      TickType_t timeout_ticks);

/**
 * @brief Pushes an event from interrupt context.
 *
 * Args:
 *     queue: Queue object.
 *     event: Event to enqueue.
 *     higher_priority_task_woken: FreeRTOS context switch flag.
 *
 * Returns:
 *     true if the event was queued, otherwise false.
 */
bool event_queue_push_from_isr(event_queue_t *queue,
                               const app_event_t *event,
                               BaseType_t *higher_priority_task_woken);

/**
 * @brief Receives an event from task context.
 *
 * Args:
 *     queue: Queue object.
 *     event: Destination event object.
 *     timeout_ticks: Maximum time to wait for an event.
 *
 * Returns:
 *     true if an event was received, otherwise false.
 */
bool event_queue_pop(event_queue_t *queue,
                     app_event_t *event,
                     TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif
