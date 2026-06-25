/**
 * @file main.c
 * @brief Demonstrates practical FreeRTOS task design patterns with ESP-IDF.
 *
 * This example shows periodic tasks, event-driven tasks, queues, mutexes,
 * task notifications, event groups, task monitoring, task priorities, and
 * stack high-water-mark inspection.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define STATUS_LED_GPIO GPIO_NUM_2
#define SENSOR_PERIOD_MS 1000U
#define LOGGER_QUEUE_LENGTH 8U
#define SYSTEM_READY_BIT BIT0
#define SENSOR_ACTIVE_BIT BIT1
#define SHUTDOWN_REQUEST_BIT BIT2
#define NOTIFY_SAMPLE_READY BIT0

static const char *TAG = "task_mastery";

typedef struct {
    uint32_t sample_number;
    float temperature_c;
    TickType_t timestamp_ticks;
} sensor_sample_t;

static QueueHandle_t s_sensor_queue = NULL;
static SemaphoreHandle_t s_console_mutex = NULL;
static EventGroupHandle_t s_system_events = NULL;
static TaskHandle_t s_sensor_task_handle = NULL;
static TaskHandle_t s_logger_task_handle = NULL;
static TaskHandle_t s_notification_task_handle = NULL;

/**
 * @brief Prints a formatted message while protecting the console output.
 *
 * Args:
 *     format: printf-compatible format string.
 *     ...: Values referenced by the format string.
 */
static void safe_printf(const char *format, ...)
{
    va_list arguments;

    if (s_console_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_console_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);

    xSemaphoreGive(s_console_mutex);
}

/**
 * @brief Configures the optional status LED GPIO.
 *
 * The GPIO number can be changed to match the selected development board.
 */
static void configure_status_led(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << STATUS_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, 0));
}

/**
 * @brief Generates a simulated temperature sample.
 *
 * Args:
 *     sample_number: Monotonically increasing sample index.
 *
 * Returns:
 *     Simulated temperature in degrees Celsius.
 */
static float read_fake_temperature(uint32_t sample_number)
{
    const uint32_t random_fraction = esp_random() % 100U;
    const float slow_variation = (float)(sample_number % 20U) * 0.05f;

    return 23.5f + slow_variation + ((float)random_fraction / 100.0f);
}

/**
 * @brief Periodically generates sensor data and sends it to a queue.
 *
 * Args:
 *     parameter: Unused task parameter.
 */
static void sensor_task(void *parameter)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SENSOR_PERIOD_MS);
    uint32_t sample_number = 0;

    (void)parameter;

    xEventGroupSetBits(s_system_events, SENSOR_ACTIVE_BIT);

    while (true) {
        const EventBits_t bits = xEventGroupGetBits(s_system_events);

        if ((bits & SHUTDOWN_REQUEST_BIT) != 0U) {
            break;
        }

        sensor_sample_t sample = {
            .sample_number = sample_number,
            .temperature_c = read_fake_temperature(sample_number),
            .timestamp_ticks = xTaskGetTickCount(),
        };

        if (xQueueSend(s_sensor_queue, &sample, pdMS_TO_TICKS(100)) != pdPASS) {
            safe_printf("[sensor] Queue full. Sample %" PRIu32 " dropped.\n",
                        sample.sample_number);
        } else if (s_notification_task_handle != NULL) {
            xTaskNotify(s_notification_task_handle,
                        NOTIFY_SAMPLE_READY,
                        eSetBits);
        }

        sample_number++;
        vTaskDelayUntil(&last_wake_time, period);
    }

    xEventGroupClearBits(s_system_events, SENSOR_ACTIVE_BIT);
    safe_printf("[sensor] Task shutting down cleanly.\n");
    s_sensor_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Receives queued sensor samples and logs them to the console.
 *
 * Args:
 *     parameter: Unused task parameter.
 */
static void logger_task(void *parameter)
{
    sensor_sample_t sample;

    (void)parameter;

    while (true) {
        if (xQueueReceive(s_sensor_queue, &sample, pdMS_TO_TICKS(250)) == pdPASS) {
            safe_printf("[logger] Sample %" PRIu32 ": %.2f C at tick %" PRIu32 "\n",
                        sample.sample_number,
                        (double)sample.temperature_c,
                        (uint32_t)sample.timestamp_ticks);
        }

        const EventBits_t bits = xEventGroupGetBits(s_system_events);
        if (((bits & SHUTDOWN_REQUEST_BIT) != 0U) &&
            (uxQueueMessagesWaiting(s_sensor_queue) == 0U)) {
            break;
        }
    }

    safe_printf("[logger] Queue drained. Task shutting down cleanly.\n");
    s_logger_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Demonstrates direct-to-task notifications and LED activity.
 *
 * Args:
 *     parameter: Unused task parameter.
 */
static void notification_task(void *parameter)
{
    bool led_state = false;

    (void)parameter;

    while (true) {
        uint32_t notification_value = 0;

        const BaseType_t notified = xTaskNotifyWait(
            0U,
            UINT32_MAX,
            &notification_value,
            pdMS_TO_TICKS(500));

        if ((notified == pdTRUE) &&
            ((notification_value & NOTIFY_SAMPLE_READY) != 0U)) {
            led_state = !led_state;
            ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, led_state));
        }

        if ((xEventGroupGetBits(s_system_events) & SHUTDOWN_REQUEST_BIT) != 0U) {
            break;
        }
    }

    ESP_ERROR_CHECK(gpio_set_level(STATUS_LED_GPIO, 0));
    safe_printf("[notify] Notification task shutting down cleanly.\n");
    s_notification_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief Prints task state, priority, and stack usage information.
 *
 * Args:
 *     name: Human-readable task name.
 *     handle: Handle of the task to inspect.
 */
static void print_task_status(const char *name, TaskHandle_t handle)
{
    if (handle == NULL) {
        safe_printf("[monitor] %-18s not running\n", name);
        return;
    }

    const UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(handle);
    const UBaseType_t priority = uxTaskPriorityGet(handle);
    const eTaskState state = eTaskGetState(handle);

    safe_printf("[monitor] %-18s state=%d priority=%u stack_hwm=%u bytes\n",
                name,
                (int)state,
                (unsigned int)priority,
                (unsigned int)high_water_mark);
}

/**
 * @brief Periodically reports system and task health information.
 *
 * Args:
 *     parameter: Unused task parameter.
 */
static void monitor_task(void *parameter)
{
    (void)parameter;

    while ((xEventGroupGetBits(s_system_events) & SHUTDOWN_REQUEST_BIT) == 0U) {
        safe_printf("\n[monitor] Free heap: %u bytes, queued samples: %u\n",
                    (unsigned int)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                    (unsigned int)uxQueueMessagesWaiting(s_sensor_queue));

        print_task_status("sensor_task", s_sensor_task_handle);
        print_task_status("logger_task", s_logger_task_handle);
        print_task_status("notification_task", s_notification_task_handle);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    safe_printf("[monitor] Monitor task shutting down.\n");
    vTaskDelete(NULL);
}

/**
 * @brief Creates all RTOS synchronization objects used by the application.
 *
 * Returns:
 *     true when every object is created successfully, otherwise false.
 */
static bool create_rtos_objects(void)
{
    s_console_mutex = xSemaphoreCreateMutex();
    s_sensor_queue = xQueueCreate(LOGGER_QUEUE_LENGTH, sizeof(sensor_sample_t));
    s_system_events = xEventGroupCreate();

    if ((s_console_mutex == NULL) ||
        (s_sensor_queue == NULL) ||
        (s_system_events == NULL)) {
        ESP_LOGE(TAG, "Failed to create one or more RTOS objects");
        return false;
    }

    return true;
}

/**
 * @brief Creates the application tasks and checks every return value.
 *
 * Returns:
 *     true when every task is created successfully, otherwise false.
 */
static bool create_application_tasks(void)
{
    BaseType_t result;

    // Create the sensor task with a higher priority than the logger task
    result = xTaskCreate(sensor_task,
                         "sensor_task",
                         3072,
                         NULL,
                         5,
                         &s_sensor_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor_task");
        return false;
    }

    // Create the logger task with a lower priority than the sensor task
    result = xTaskCreate(logger_task,
                         "logger_task",
                         3072,
                         NULL,
                         3,
                         &s_logger_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create logger_task");
        return false;
    }

    // Create the notification task with a priority between the sensor and logger tasks
    result = xTaskCreate(notification_task,
                         "notification_task",
                         2048,
                         NULL,
                         4,
                         &s_notification_task_handle);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create notification_task");
        return false;
    }

    // Create the monitor task with the lowest priority
    result = xTaskCreate(monitor_task,
                         "monitor_task",
                         3072,
                         NULL,
                         2,
                         NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor_task");
        return false;
    }

    return true;
}

/**
 * @brief ESP-IDF application entry point.
 */
void app_main(void)
{
    // Configure the optional status LED GPIO for visual feedback
    configure_status_led();

    // Create all RTOS objects before starting any tasks
    if (!create_rtos_objects()) {
        ESP_LOGE(TAG, "Application initialization aborted");
        return;
    }

    // Create the application tasks and check for errors
    if (!create_application_tasks()) {
        ESP_LOGE(TAG, "Task creation failed");
        xEventGroupSetBits(s_system_events, SHUTDOWN_REQUEST_BIT);
        return;
    }

    // Signal that the system is ready for operation    
    xEventGroupSetBits(s_system_events, SYSTEM_READY_BIT);
    ESP_LOGI(TAG, "FreeRTOS task mastery example started");
    ESP_LOGI(TAG, "Status LED GPIO: %d", STATUS_LED_GPIO);
}
