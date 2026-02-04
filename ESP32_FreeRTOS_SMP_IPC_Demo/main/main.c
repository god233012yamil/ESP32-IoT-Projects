/**
 * @file main.c
 * @brief ESP32 FreeRTOS SMP inter-core IPC demos (Queue and Task Notifications).
 *
 * This project demonstrates two practical inter-core communication patterns on ESP32:
 *   1) Queue-based producer/consumer where tasks are pinned to different cores.
 *   2) Task notification-based signaling where tasks are pinned to different cores.
 *
 * The goal is to provide a ready-to-build reference you can reuse in real projects:
 * - Pin tasks to specific cores to reduce jitter and isolate workloads.
 * - Use queues to move payloads safely across cores.
 * - Use task notifications for low-overhead "kick" signaling across cores.
 *
 * Build with ESP-IDF v5.x. Select the demo via menuconfig:
 *   Component config -> SMP IPC Demo -> Demo mode
 */

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "SMP_IPC";

/* -------------------------- Queue Demo -------------------------- */

static QueueHandle_t s_data_q = NULL;

/**
 * @brief Producer task (Queue demo).
 *
 * Sends incrementing integers into a queue. The task is intended to be pinned
 * to a specific core to demonstrate cross-core payload passing.
 *
 * Args:
 *   arg: Unused.
 */
static void producer_queue_task(void *arg)
{
    (void)arg;

    int value = 0;
    while (1) {
        if (s_data_q != NULL) {
            (void)xQueueSend(s_data_q, &value, portMAX_DELAY);
            ESP_LOGI(TAG, "Queue producer: sent %d (core %d)", value, xPortGetCoreID());
            value++;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief Consumer task (Queue demo).
 *
 * Receives integers from a queue and logs them. The task is intended to be pinned
 * to a different core from the producer to demonstrate inter-core IPC.
 *
 * Args:
 *   arg: Unused.
 */
static void consumer_queue_task(void *arg)
{
    (void)arg;

    int rx = 0;
    while (1) {
        if (s_data_q != NULL && xQueueReceive(s_data_q, &rx, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Queue consumer: got  %d (core %d)", rx, xPortGetCoreID());
        }
    }
}

/**
 * @brief Run the queue demo by creating a queue and two pinned tasks.
 *
 * Creates a small queue, then pins the producer and consumer tasks to different cores.
 */
static void run_queue_demo(void)
{
    // Create a queue to hold up to 8 integers
    s_data_q = xQueueCreate(8, sizeof(int));
    if (s_data_q == NULL) {
        ESP_LOGE(TAG, "Queue demo: failed to create queue");
        return;
    }

    const int producer_core = CONFIG_SMP_IPC_PRODUCER_CORE;
    const int consumer_core = CONFIG_SMP_IPC_CONSUMER_CORE;

    // Create producer task pinned to one core
    BaseType_t ok_p = xTaskCreatePinnedToCore(
        producer_queue_task,
        "producer_queue",
        4096,
        NULL,
        8,
        NULL,
        producer_core
    );

    // Create consumer task pinned to the other core
    BaseType_t ok_c = xTaskCreatePinnedToCore(
        consumer_queue_task,
        "consumer_queue",
        4096,
        NULL,
        8,
        NULL,
        consumer_core
    );

    if (ok_p != pdPASS || ok_c != pdPASS) {
        ESP_LOGE(TAG, "Queue demo: failed to create tasks (ok_p=%d ok_c=%d)", (int)ok_p, (int)ok_c);
    }
}

/* ---------------------- Notification Demo ---------------------- */

static TaskHandle_t s_consumer_notify_handle = NULL;

/**
 * @brief Producer task (Notification demo).
 *
 * Sends a lightweight signal to the consumer using task notifications.
 * Notifications are accumulated using eIncrement so bursts are not lost.
 *
 * Args:
 *   arg: Unused.
 */
static void producer_notify_task(void *arg)
{
    (void)arg;
    uint32_t seq = 0;
    
    while (1) {        
        // Send notification to consumer task
        if (s_consumer_notify_handle != NULL) {
            xTaskNotify(s_consumer_notify_handle, 0, eIncrement);
            ESP_LOGI(TAG, "Notify producer: notify seq=%lu (core %d)",
                     (unsigned long)seq, xPortGetCoreID());
            seq++;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief Consumer task (Notification demo).
 *
 * Blocks on ulTaskNotifyTake() until signaled by the producer. The return value
 * is the number of notifications received since the last take, which makes this
 * behave like a lightweight counting semaphore.
 *
 * Args:
 *   arg: Unused.
 */
static void consumer_notify_task(void *arg)
{
    (void)arg;

    // Store own task handle for the producer to use
    s_consumer_notify_handle = xTaskGetCurrentTaskHandle();

    while (1) {
        // Wait indefinitely for notification(s) from producer
        uint32_t n = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Notify consumer: got %lu notify(ies) (core %d)",
                 (unsigned long)n, xPortGetCoreID());
    }
}

/**
 * @brief Run the notification demo by creating two pinned tasks.
 *
 * Creates the consumer first so its task handle is available, then starts the producer.
 */
static void run_notify_demo(void)
{
    const int producer_core = CONFIG_SMP_IPC_PRODUCER_CORE;
    const int consumer_core = CONFIG_SMP_IPC_CONSUMER_CORE;

    // Create consumer task pinned to one core
    BaseType_t ok_c = xTaskCreatePinnedToCore(
        consumer_notify_task,
        "consumer_notify",
        4096,
        NULL,
        8,
        &s_consumer_notify_handle,
        consumer_core
    );

    // Create producer task pinned to the other core
    BaseType_t ok_p = xTaskCreatePinnedToCore(
        producer_notify_task,
        "producer_notify",
        4096,
        NULL,
        8,
        NULL,
        producer_core
    );

    if (ok_p != pdPASS || ok_c != pdPASS) {
        ESP_LOGE(TAG, "Notify demo: failed to create tasks (ok_p=%d ok_c=%d)", (int)ok_p, (int)ok_c);
    }
}

/* --------------------------- app_main --------------------------- */

/**
 * @brief ESP-IDF application entry point.
 *
 * Selects and runs one demo variant based on menuconfig.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 FreeRTOS SMP IPC demo starting (core %d)", xPortGetCoreID());
    ESP_LOGI(TAG, "Producer pinned core: %d, Consumer pinned core: %d",
             CONFIG_SMP_IPC_PRODUCER_CORE, CONFIG_SMP_IPC_CONSUMER_CORE);

#if CONFIG_SMP_IPC_DEMO_MODE_QUEUE
    ESP_LOGI(TAG, "Running demo mode: Queue");
    run_queue_demo();
#elif CONFIG_SMP_IPC_DEMO_MODE_NOTIFY
    ESP_LOGI(TAG, "Running demo mode: Task Notifications");
    run_notify_demo();
#else
    ESP_LOGW(TAG, "No demo mode selected");
#endif
}
