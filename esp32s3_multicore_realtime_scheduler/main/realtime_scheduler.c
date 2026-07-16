#include "realtime_scheduler.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lockfree_ring.h"

#define CONTROL_CORE                 0
#define SERVICE_CORE                 1

#define CONTROL_TASK_PRIORITY        22
#define TELEMETRY_TASK_PRIORITY      8
#define STRESS_TASK_PRIORITY         5

#define CONTROL_TASK_STACK_SIZE      4096
#define TELEMETRY_TASK_STACK_SIZE    4096
#define STRESS_TASK_STACK_SIZE       4096

#define CONTROL_PERIOD_US            1000U
#define CONTROL_DEADLINE_US          800U
#define CONTROL_WORK_ITERATIONS      220U

#define PROFILE_GPIO                 GPIO_NUM_2
#define DEADLINE_MISS_GPIO           GPIO_NUM_3

#define TELEMETRY_REPORT_MS          1000U
#define STRESS_DELAY_MS              1U

static const char *TAG = "realtime_sched";

static TaskHandle_t s_control_task_handle;
static esp_timer_handle_t s_release_timer;
static lockfree_ring_t s_timing_ring;
static portMUX_TYPE s_stats_lock = portMUX_INITIALIZER_UNLOCKED;

static atomic_uint_fast32_t s_release_sequence;
static atomic_uint_fast32_t s_notification_overruns;

static volatile uint32_t s_shared_guarded_value;

typedef struct {
    uint64_t samples;
    uint64_t deadline_misses;
    uint32_t min_execution_cycles;
    uint32_t max_execution_cycles;
    uint32_t max_release_jitter_us;
} timing_stats_t;

static timing_stats_t s_timing_stats;

/**
 * @brief Configures profiling GPIO outputs.
 */
static void configure_profile_gpios(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << PROFILE_GPIO) | (1ULL << DEADLINE_MISS_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(gpio_set_level(PROFILE_GPIO, 0));
    ESP_ERROR_CHECK(gpio_set_level(DEADLINE_MISS_GPIO, 0));
}

/**
 * @brief Performs deterministic synthetic control-loop work.
 *
 * The arithmetic is intentionally deterministic and avoids dynamic memory,
 * logging, and blocking calls inside the real-time path.
 *
 * @param input Input sample.
 * @return Simulated actuator command.
 */
static uint32_t execute_control_algorithm(uint32_t input)
{
    uint32_t accumulator = input ^ 0xA5A55A5AU;

    for (uint32_t index = 0U; index < CONTROL_WORK_ITERATIONS; ++index) {
        accumulator = (accumulator << 5U) | (accumulator >> 27U);
        accumulator ^= (index * 2654435761U);
        accumulator += 0x9E3779B9U;
    }

    return accumulator;
}

/**
 * @brief Updates timing statistics from the control task.
 *
 * @param execution_cycles Measured control execution time in CPU cycles.
 * @param release_jitter_us Measured release jitter in microseconds.
 * @param deadline_missed true when the deadline was missed.
 */
static void update_timing_statistics(
    uint32_t execution_cycles,
    uint32_t release_jitter_us,
    bool deadline_missed)
{
    portENTER_CRITICAL(&s_stats_lock);

    if (s_timing_stats.samples == 0U) {
        s_timing_stats.min_execution_cycles = execution_cycles;
        s_timing_stats.max_execution_cycles = execution_cycles;
    } else {
        if (execution_cycles < s_timing_stats.min_execution_cycles) {
            s_timing_stats.min_execution_cycles = execution_cycles;
        }
        if (execution_cycles > s_timing_stats.max_execution_cycles) {
            s_timing_stats.max_execution_cycles = execution_cycles;
        }
    }

    if (release_jitter_us > s_timing_stats.max_release_jitter_us) {
        s_timing_stats.max_release_jitter_us = release_jitter_us;
    }

    s_timing_stats.samples++;
    if (deadline_missed) {
        s_timing_stats.deadline_misses++;
    }

    portEXIT_CRITICAL(&s_stats_lock);
}

/**
 * @brief Copies timing statistics for noncritical reporting.
 *
 * @return Snapshot of the current timing statistics.
 */
static timing_stats_t get_timing_statistics_snapshot(void)
{
    timing_stats_t snapshot;

    portENTER_CRITICAL(&s_stats_lock);
    snapshot = s_timing_stats;
    portEXIT_CRITICAL(&s_stats_lock);

    return snapshot;
}

/**
 * @brief Releases the control task from the periodic high-resolution timer.
 *
 * @param argument Optional callback argument. Not used.
 */
static void control_release_timer_callback(void *argument)
{
    (void)argument;

    const uint32_t previous = atomic_fetch_add_explicit(
        &s_release_sequence,
        1U,
        memory_order_relaxed);

    if ((previous > 0U) && (eTaskGetState(s_control_task_handle) != eBlocked)) {
        atomic_fetch_add_explicit(
            &s_notification_overruns,
            1U,
            memory_order_relaxed);
    }

    xTaskNotifyGive(s_control_task_handle);
}

/**
 * @brief Executes the pinned hard real-time control task.
 *
 * @param argument Optional task argument. Not used.
 */
static void control_task(void *argument)
{
    (void)argument;

    uint32_t expected_sequence = 0U;
    int64_t expected_release_us = esp_timer_get_time() + CONTROL_PERIOD_US;
    volatile uint32_t actuator_sink = 0U;

    while (true) {
        const uint32_t notifications = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const int64_t release_time_us = esp_timer_get_time();

        if (notifications > 1U) {
            atomic_fetch_add_explicit(
                &s_notification_overruns,
                notifications - 1U,
                memory_order_relaxed);
        }

        const int64_t jitter_signed = release_time_us - expected_release_us;
        const uint32_t release_jitter_us = (uint32_t)(
            (jitter_signed >= 0) ? jitter_signed : -jitter_signed);

        expected_sequence += notifications;
        expected_release_us += (int64_t)CONTROL_PERIOD_US * notifications;

        gpio_set_level(PROFILE_GPIO, 1);
        const uint32_t start_cycles = esp_cpu_get_cycle_count();

        actuator_sink = execute_control_algorithm(expected_sequence);

        const uint32_t execution_cycles =
            esp_cpu_get_cycle_count() - start_cycles;
        const int64_t completion_time_us = esp_timer_get_time();
        const uint32_t response_time_us = (uint32_t)(
            completion_time_us - release_time_us);
        const bool deadline_missed = response_time_us > CONTROL_DEADLINE_US;

        gpio_set_level(DEADLINE_MISS_GPIO, deadline_missed ? 1 : 0);
        gpio_set_level(PROFILE_GPIO, 0);

        const timing_sample_t sample = {
            .timestamp_us = release_time_us,
            .execution_cycles = execution_cycles,
            .release_jitter_us = release_jitter_us,
            .sequence = expected_sequence,
            .deadline_missed = deadline_missed,
        };

        (void)lockfree_ring_push(&s_timing_ring, &sample);
        update_timing_statistics(
            execution_cycles,
            release_jitter_us,
            deadline_missed);

        if (deadline_missed) {
            // The pulse remains high briefly so it is easy to capture externally.
            esp_rom_delay_us(20U);
            gpio_set_level(DEADLINE_MISS_GPIO, 0);
        }
    }
}

/**
 * @brief Consumes timing samples and reports aggregated statistics.
 *
 * @param argument Optional task argument. Not used.
 */
static void telemetry_task(void *argument)
{
    (void)argument;

    uint64_t consumed_samples = 0U;
    uint64_t consumed_deadline_misses = 0U;
    int64_t last_report_us = esp_timer_get_time();

    while (true) {
        timing_sample_t sample;

        while (lockfree_ring_pop(&s_timing_ring, &sample)) {
            consumed_samples++;
            if (sample.deadline_missed) {
                consumed_deadline_misses++;
            }
        }

        const int64_t now_us = esp_timer_get_time();
        if ((now_us - last_report_us) >=
            ((int64_t)TELEMETRY_REPORT_MS * 1000LL)) {
            const timing_stats_t stats = get_timing_statistics_snapshot();
            const uint32_t overruns = (uint32_t)atomic_load_explicit(
                &s_notification_overruns,
                memory_order_relaxed);

            ESP_LOGI(
                TAG,
                "samples=%" PRIu64
                " consumed=%" PRIu64
                " misses=%" PRIu64
                " consumed_misses=%" PRIu64
                " min_cycles=%" PRIu32
                " max_cycles=%" PRIu32
                " max_jitter_us=%" PRIu32
                " overruns=%" PRIu32
                " dropped=%" PRIu32,
                stats.samples,
                consumed_samples,
                stats.deadline_misses,
                consumed_deadline_misses,
                stats.min_execution_cycles,
                stats.max_execution_cycles,
                stats.max_release_jitter_us,
                overruns,
                lockfree_ring_get_dropped(&s_timing_ring));

            last_report_us = now_us;
        }

        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

/**
 * @brief Generates service-core load and protected shared-state activity.
 *
 * @param argument Optional task argument. Not used.
 */
static void service_stress_task(void *argument)
{
    (void)argument;

    uint32_t local = 0x12345678U;

    while (true) {
        for (uint32_t index = 0U; index < 10000U; ++index) {
            local ^= local << 13U;
            local ^= local >> 17U;
            local ^= local << 5U;
        }

        // Keep the cross-core critical section intentionally tiny.
        portENTER_CRITICAL(&s_stats_lock);
        s_shared_guarded_value = local;
        portEXIT_CRITICAL(&s_stats_lock);

        vTaskDelay(pdMS_TO_TICKS(STRESS_DELAY_MS));
    }
}

/**
 * @brief Creates the high-resolution periodic release timer.
 */
static void create_control_release_timer(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = control_release_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "control_release",
        .skip_unhandled_events = false,
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_release_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(
        s_release_timer,
        CONTROL_PERIOD_US));
}

/**
 * @brief Initializes and starts the multicore real-time scheduling demo.
 */
void realtime_scheduler_start(void)
{
    memset(&s_timing_stats, 0, sizeof(s_timing_stats));
    atomic_init(&s_release_sequence, 0U);
    atomic_init(&s_notification_overruns, 0U);
    lockfree_ring_init(&s_timing_ring);
    configure_profile_gpios();

    BaseType_t result = xTaskCreatePinnedToCore(
        control_task,
        "control_task",
        CONTROL_TASK_STACK_SIZE,
        NULL,
        CONTROL_TASK_PRIORITY,
        &s_control_task_handle,
        CONTROL_CORE);
    configASSERT(result == pdPASS);

    result = xTaskCreatePinnedToCore(
        telemetry_task,
        "telemetry_task",
        TELEMETRY_TASK_STACK_SIZE,
        NULL,
        TELEMETRY_TASK_PRIORITY,
        NULL,
        SERVICE_CORE);
    configASSERT(result == pdPASS);

    result = xTaskCreatePinnedToCore(
        service_stress_task,
        "service_stress",
        STRESS_TASK_STACK_SIZE,
        NULL,
        STRESS_TASK_PRIORITY,
        NULL,
        SERVICE_CORE);
    configASSERT(result == pdPASS);

    create_control_release_timer();

    ESP_LOGI(TAG, "Real-time scheduler demo started");
    ESP_LOGI(TAG, "Control task pinned to core %d", CONTROL_CORE);
    ESP_LOGI(TAG, "Service tasks pinned to core %d", SERVICE_CORE);
    ESP_LOGI(
        TAG,
        "GPIO %d: execution pulse, GPIO %d: deadline-miss pulse",
        PROFILE_GPIO,
        DEADLINE_MISS_GPIO);
}
