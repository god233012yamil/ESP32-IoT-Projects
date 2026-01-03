/**
 * @file main.c
 * @brief Side-by-side demo: Preemptive (FreeRTOS) vs Cooperative (run-to-completion) execution on ESP32.
 *
 * This project provides two selectable demo modes:
 * - Preemptive: Multiple FreeRTOS tasks with priorities and a mutex-protected shared counter.
 * - Cooperative: A single run-to-completion event loop with timer-posted events.
 *
 * Select mode in:
 *   idf.py menuconfig
 *   Component config -> Scheduling Model Demo -> Demo mode
 *
 * Build:
 *   idf.py set-target esp32
 *   idf.py build flash monitor
 */

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "esp_log.h"

#define TAG "sched_demo"

/**
 * @brief Burn CPU cycles to make scheduling effects visible in logs.
 *
 * This function intentionally avoids vTaskDelay(). It is for demonstration only.
 *
 * Args:
 *   iterations: Loop count to burn CPU cycles.
 *
 * Returns:
 *   None
 */
static void demo_cpu_work(uint32_t iterations)
{
    volatile uint32_t x = 0;
    for (uint32_t i = 0; i < iterations; i++) {
        x += (i ^ (x << 1));
    }
    (void)x;
}

/* ========================================================================= */
/*                         MODE 1: PREEMPTIVE (FreeRTOS)                     */
/* ========================================================================= */
#if CONFIG_DEMO_MODE_PREEMPTIVE

static SemaphoreHandle_t g_counter_mutex = NULL;
static uint32_t g_shared_counter = 0;

/**
 * @brief Safely add to a shared counter using a mutex.
 *
 * Args:
 *   delta: Increment amount.
 *
 * Returns:
 *   Updated counter value.
 */
static uint32_t counter_add(uint32_t delta)
{
    uint32_t v;

    // Take the mutex
    xSemaphoreTake(g_counter_mutex, portMAX_DELAY);
    
    // Critical section
    g_shared_counter += delta;
    v = g_shared_counter;
    
    // Give the mutex back
    xSemaphoreGive(g_counter_mutex);

    return v;
}

/**
 * @brief Periodic "sensor" task (medium priority).
 *
 * Args:
 *   arg: Unused.
 *
 * Returns:
 *   None
 */
static void task_sensor(void *arg)
{
    (void)arg;

    while (true) {
        // Simulate sensor reading work
        demo_cpu_work(200000);
        ESP_LOGI(TAG, "[PREEMPT] sensor: counter=%u", (unsigned)counter_add(1));
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief Background "network" task (lower priority).
 *
 * Args:
 *   arg: Unused.
 *
 * Returns:
 *   None
 */
static void task_network(void *arg)
{
    (void)arg;

    while (true) {
        // Simulate network work
        demo_cpu_work(350000);
        ESP_LOGI(TAG, "[PREEMPT] net: counter=%u", (unsigned)counter_add(2));
        
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

/**
 * @brief High priority burst task to show preemption.
 *
 * Args:
 *   arg: Unused.
 *
 * Returns:
 *   None
 */
static void task_highprio(void *arg)
{
    (void)arg;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1500));
        
        // Simulate high-priority burst work
        demo_cpu_work(250000);
        ESP_LOGW(TAG, "[PREEMPT] HIGH: counter=%u (burst)", (unsigned)counter_add(10));
    }
}

/**
 * @brief Start the preemptive demo (mutex + 3 tasks).
 *
 * Returns:
 *   None
 */
static void start_preemptive_demo(void)
{
    // Create the mutex
    g_counter_mutex = xSemaphoreCreateMutex();
    if (g_counter_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    // Create tasks
    xTaskCreate(task_sensor, "sensor", 4096, NULL, 5, NULL);
    xTaskCreate(task_network, "network", 4096, NULL, 4, NULL);
    xTaskCreate(task_highprio, "highprio", 4096, NULL, 8, NULL);

    ESP_LOGI(TAG, "Preemptive demo started.");
}

#endif /* CONFIG_DEMO_MODE_PREEMPTIVE */

/* ========================================================================= */
/*                   MODE 2: COOPERATIVE (Run-to-completion)                 */
/* ========================================================================= */
#if CONFIG_DEMO_MODE_COOPERATIVE

// Event IDs
typedef enum {
    EVT_SENSOR = 1,
    EVT_NET    = 2,
    EVT_UI     = 3,
} demo_event_id_t;

// Event payload
typedef struct {
    demo_event_id_t id;
    uint32_t tick;
} demo_event_t;

// Global variables
static QueueHandle_t g_evt_q = NULL;
static TimerHandle_t g_evt_timer = NULL;
static uint32_t g_coop_counter = 0;

/**
 * @brief Post an event from a timer callback without blocking.
 *
 * Args:
 *   id: Event ID to post.
 *
 * Returns:
 *   None
 */
static void post_event_from_timer(demo_event_id_t id)
{
    demo_event_t e = {
        .id = id,
        .tick = (uint32_t)xTaskGetTickCount(),
    };

    if (xQueueSend(g_evt_q, &e, 0) != pdTRUE) {
        ESP_LOGW(TAG, "[COOP] queue full, drop id=%d", (int)id);
    }
}

/**
 * @brief Timer callback that feeds the cooperative event loop.
 *
 * Args:
 *   tmr: Timer handle.
 *
 * Returns:
 *   None
 */
static void coop_timer_cb(TimerHandle_t tmr)
{
    (void)tmr;

    static int phase = 0;
    phase = (phase + 1) % 3;

    if (phase == 0) post_event_from_timer(EVT_SENSOR);
    else if (phase == 1) post_event_from_timer(EVT_NET);
    else post_event_from_timer(EVT_UI);
}

/**
 * @brief Handle SENSOR event to completion.
 *
 * Args:
 *   e: Event payload.
 *
 * Returns:
 *   None
 */
static void handle_sensor_event(const demo_event_t *e)
{
    // Simulate sensor processing work
    demo_cpu_work(180000);
    g_coop_counter += 1;
    ESP_LOGI(TAG, "[COOP] SENSOR: tick=%u counter=%u", (unsigned)e->tick, (unsigned)g_coop_counter);
}

/**
 * @brief Handle NET event to completion.
 *
 * Args:
 *   e: Event payload.
 *
 * Returns:
 *   None
 */
static void handle_net_event(const demo_event_t *e)
{
    // Simulate network processing work
    demo_cpu_work(260000);
    g_coop_counter += 2;
    ESP_LOGI(TAG, "[COOP] NET: tick=%u counter=%u", (unsigned)e->tick, (unsigned)g_coop_counter);
}

/**
 * @brief Handle UI event to completion.
 *
 * Args:
 *   e: Event payload.
 *
 * Returns:
 *   None
 */
static void handle_ui_event(const demo_event_t *e)
{
    // Simulate UI processing work
    demo_cpu_work(120000);
    g_coop_counter += 3;
    ESP_LOGI(TAG, "[COOP] UI: tick=%u counter=%u", (unsigned)e->tick, (unsigned)g_coop_counter);
}

/**
 * @brief Cooperative main loop task: run handlers to completion.
 *
 * Args:
 *   arg: Unused.
 *
 * Returns:
 *   None
 */
static void coop_main_loop_task(void *arg)
{
    (void)arg;

    demo_event_t e;
    while (true) {
        // Wait indefinitely for the next event
        if (xQueueReceive(g_evt_q, &e, portMAX_DELAY) == pdTRUE) {
            switch (e.id) {
                case EVT_SENSOR: handle_sensor_event(&e); break;
                case EVT_NET:    handle_net_event(&e); break;
                case EVT_UI:     handle_ui_event(&e); break;
                default:
                    ESP_LOGW(TAG, "[COOP] unknown event id=%d", (int)e.id);
                    break;
            }
        }
    }
}

/**
 * @brief Start the cooperative demo (queue + timer + 1 task).
 *
 * Returns:
 *   None
 */
static void start_cooperative_demo(void)
{
    // Create the event queue
    g_evt_q = xQueueCreate(CONFIG_COOP_EVENT_QUEUE_LEN, sizeof(demo_event_t));
    if (g_evt_q == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    // Create the periodic timer
    g_evt_timer = xTimerCreate(
        "evt_timer",
        pdMS_TO_TICKS(CONFIG_COOP_TIMER_PERIOD_MS),
        pdTRUE,
        NULL,
        coop_timer_cb
    );

    if (g_evt_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create timer");
        return;
    }

    // Create the main loop task
    xTaskCreate(coop_main_loop_task, "coop_loop", 4096, NULL, 5, NULL);

    // Start the timer
    if (xTimerStart(g_evt_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start timer");
        return;
    }

    ESP_LOGI(TAG, "Cooperative demo started.");
}

#endif /* CONFIG_DEMO_MODE_COOPERATIVE */

/**
 * @brief ESP-IDF application entry point.
 *
 * Returns:
 *   None
 */
void app_main(void)
{
#if CONFIG_DEMO_MODE_PREEMPTIVE
    ESP_LOGI(TAG, "Mode: PREEMPTIVE (FreeRTOS tasks)");
    start_preemptive_demo();
#elif CONFIG_DEMO_MODE_COOPERATIVE
    ESP_LOGI(TAG, "Mode: COOPERATIVE (run-to-completion)");
    start_cooperative_demo();
#else
    ESP_LOGE(TAG, "No demo mode selected.");
#endif
}
