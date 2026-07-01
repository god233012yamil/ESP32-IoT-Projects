#include "event_queue.h"

/**
 * @brief Initializes the event queue.
 *
 * Args:
 *     queue: Pointer to the event queue instance.
 *     length: Number of events the queue can hold.
 *
 * Returns:
 *     true if the queue was initialized successfully, false otherwise.
 */
bool event_queue_init(event_queue_t *queue, UBaseType_t length)
{
    if (queue == 0)
    {
        return false;
    }

    queue->handle = xQueueCreate(length, sizeof(app_event_t));
    return queue->handle != 0;
}

/**
 * @brief Pushes an event to the queue.
 *
 * Args:
 *     queue: Pointer to the event queue instance.
 *     event: Pointer to the event to push.
 *     timeout_ticks: Timeout in ticks.
 *
 * Returns:
 *     true if the event was pushed successfully, false otherwise.
 */
bool event_queue_push(event_queue_t *queue,
                      const app_event_t *event,
                      TickType_t timeout_ticks)
{
    if ((queue == 0) || (queue->handle == 0) || (event == 0))
    {
        return false;
    }

    return xQueueSend(queue->handle, event, timeout_ticks) == pdTRUE;
}

/**
 * @brief Pushes an event to the queue from an interrupt service routine.
 *
 * Args:
 *     queue: Pointer to the event queue instance.
 *     event: Pointer to the event to push.
 *     higher_priority_task_woken: Pointer to a variable that will be set if a higher priority task is woken.
 *
 * Returns:
 *     true if the event was pushed successfully, false otherwise.
 */
bool event_queue_push_from_isr(event_queue_t *queue,
                               const app_event_t *event,
                               BaseType_t *higher_priority_task_woken)
{
    if ((queue == 0) || (queue->handle == 0) || (event == 0))
    {
        return false;
    }

    return xQueueSendFromISR(queue->handle,
                             event,
                             higher_priority_task_woken) == pdTRUE;
}

/**
 * @brief Pops an event from the queue.
 *
 * Args:
 *     queue: Pointer to the event queue instance.
 *     event: Pointer to the event to pop.
 *     timeout_ticks: Timeout in ticks.
 *
 * Returns:
 *     true if the event was popped successfully, false otherwise.
 */
bool event_queue_pop(event_queue_t *queue,
                     app_event_t *event,
                     TickType_t timeout_ticks)
{
    if ((queue == 0) || (queue->handle == 0) || (event == 0))
    {
        return false;
    }

    return xQueueReceive(queue->handle, event, timeout_ticks) == pdTRUE;
}
