
/**
 * @file main.c
 *
 * @brief Practical demonstration of FreeRTOS Event Groups on ESP32-S3 using ESP-IDF.
 *
 * This file contains a complete, ready-to-compile ESP-IDF application showcasing
 * how to use FreeRTOS Event Groups for multi-task synchronization in real-world
 * embedded scenarios.
 *
 * Demonstrated use cases include:
 * - System startup barrier (waiting for multiple subsystems to initialize)
 * - OR vs AND event wait logic
 * - ADC sampling readiness signaling
 * - GPIO stability monitoring
 * - Simulated I2C temperature sensing
 * - Aggregation of multi-source readiness into a single payload
 * - Timeout-based fault detection
 *
 * Important design note:
 * Event Groups are used strictly for signaling state and readiness.
 * They are NOT used for data transfer. Shared data is kept minimal and
 * intentionally simple for demonstration purposes.
 *
 * Target:
 * - ESP32-S3
 *
 * Framework:
 * - ESP-IDF (FreeRTOS)
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#define APP_TAG "EVT_GRP_DEMO"

#define DEMO_GPIO_INPUT GPIO_NUM_0
#define DEMO_ADC_UNIT   ADC_UNIT_1
#define DEMO_ADC_CHANNEL ADC_CHANNEL_0

#define STACK_SMALL   3072
#define STACK_MEDIUM  4096

#define PRIO_INIT        8
#define PRIO_ADC         6
#define PRIO_TEMP        6
#define PRIO_GPIO        6
#define PRIO_AGGREGATOR  7
#define PRIO_DIAG        5

/* -------------------------------------------------------------------------- */
/* Event Bits                                                                  */
/* -------------------------------------------------------------------------- */

#define EVT_ADC_INIT      (1U << 0)
#define EVT_GPIO_INIT     (1U << 1)
#define EVT_I2C_INIT      (1U << 2)
#define EVT_NET_INIT      (1U << 3)

#define EVT_ADC_READY     (1U << 8)
#define EVT_TEMP_READY    (1U << 9)
#define EVT_GPIO_READY    (1U << 10)

#define EVT_ALL_INIT_MASK (EVT_ADC_INIT | EVT_GPIO_INIT | EVT_I2C_INIT | EVT_NET_INIT)
#define EVT_ALL_DATA_MASK (EVT_ADC_READY | EVT_TEMP_READY | EVT_GPIO_READY)
#define EVT_ANY_DATA_MASK EVT_ALL_DATA_MASK

/* -------------------------------------------------------------------------- */
/* Shared State                                                                */
/* -------------------------------------------------------------------------- */

static EventGroupHandle_t g_evt = NULL;
static adc_oneshot_unit_handle_t g_adc_handle = NULL;

static volatile int   g_last_adc_raw   = 0;
static volatile float g_last_temp_c    = 0.0f;
static volatile int   g_last_gpio_lvl  = 0;

/* -------------------------------------------------------------------------- */
/* Utility Functions                                                           */
/* -------------------------------------------------------------------------- */

/**
 * millis
 *
 * @brief Returns system uptime in milliseconds.
 *
 * @return uint32_t System uptime in milliseconds.
 */
static uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/**
 * init_gpio_input
 *
 * @brief Initializes a GPIO pin as input with pull-up enabled.
 *
 * @param pin GPIO number to configure.
 *
 * @return
 * - ESP_OK on success
 * - ESP_FAIL or error code on failure
 */
static esp_err_t init_gpio_input(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    return gpio_config(&cfg);
}

/**
 * init_adc_oneshot
 *
 * @brief Initializes the ADC oneshot driver and configures a single channel.
 *
 * @return
 * - ESP_OK on success
 * - ESP_FAIL or error code on failure
 */
static esp_err_t init_adc_oneshot(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = DEMO_ADC_UNIT,
    };

    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &g_adc_handle);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };

    return adc_oneshot_config_channel(g_adc_handle, DEMO_ADC_CHANNEL, &chan_cfg);
}

/* -------------------------------------------------------------------------- */
/* FreeRTOS Tasks                                                              */
/* -------------------------------------------------------------------------- */

/**
 * init_task
 *
 * @brief Performs system initialization and sets startup barrier event bits.
 *
 * This task initializes GPIO and ADC peripherals and simulates initialization
 * of I2C and network subsystems. Each successful initialization sets a
 * corresponding event bit.
 *
 * Once all initialization steps are complete, the task deletes itself.
 *
 * @param arg Unused.
 */
static void init_task(void *arg)
{
    (void)arg;

    // Initialize GPIO input and set event bit if successful
    if (init_gpio_input(DEMO_GPIO_INPUT) == ESP_OK) {
        xEventGroupSetBits(g_evt, EVT_GPIO_INIT);
    }

    // Initialize ADC oneshot and set event bit if successful
    if (init_adc_oneshot() == ESP_OK) {
        xEventGroupSetBits(g_evt, EVT_ADC_INIT);
    }
    
    // Simulate I2C initialization
    vTaskDelay(pdMS_TO_TICKS(150));
    xEventGroupSetBits(g_evt, EVT_I2C_INIT);    
    
    // Simulate Network initialization
    vTaskDelay(pdMS_TO_TICKS(250));
    xEventGroupSetBits(g_evt, EVT_NET_INIT);

    vTaskDelete(NULL);
}

/**
 * adc_task
 *
 * @brief Periodically samples ADC and signals data readiness.
 *
 * This task waits for ADC initialization to complete, then periodically
 * reads raw ADC values and signals readiness using an Event Group bit.
 *
 * @param arg Unused.
 */
static void adc_task(void *arg)
{
    (void)arg;

    // Block until ADC initialization is complete
    xEventGroupWaitBits(g_evt, EVT_ADC_INIT, pdFALSE, pdTRUE, portMAX_DELAY);

    while (1) {
        int raw = 0;
        
        // Set ADC ready event bit upon successful read
        if (adc_oneshot_read(g_adc_handle, DEMO_ADC_CHANNEL, &raw) == ESP_OK) {
            g_last_adc_raw = raw;
            xEventGroupSetBits(g_evt, EVT_ADC_READY);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * temp_task
 *
 * @brief Simulates periodic temperature acquisition over I2C.
 *
 * This task represents an I2C-based temperature sensor. It generates a
 * slowly varying temperature value and signals readiness after each update.
 *
 * @param arg Unused.
 */
static void temp_task(void *arg)
{
    (void)arg;

    // Block until I2C initialization is complete
    xEventGroupWaitBits(g_evt, EVT_I2C_INIT, pdFALSE, pdTRUE, portMAX_DELAY);

    float temp = 24.0f;
    int direction = 1;

    while (1) {
        temp += (direction > 0) ? 0.1f : -0.1f;
        if (temp > 28.0f) direction = -1;
        if (temp < 22.0f) direction = 1;

        g_last_temp_c = temp;
        
        // Set the temperature ready event bit
        xEventGroupSetBits(g_evt, EVT_TEMP_READY);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * gpio_task
 *
 * @brief Monitors a GPIO input and detects stable logic levels.
 *
 * The task samples the GPIO periodically and considers the signal stable
 * after several consecutive identical reads. Once stable, it signals
 * readiness using an Event Group bit.
 *
 * @param arg Unused.
 */
static void gpio_task(void *arg)
{
    (void)arg;

    // Block until GPIO initialization is complete
    xEventGroupWaitBits(g_evt, EVT_GPIO_INIT, pdFALSE, 
        pdTRUE, portMAX_DELAY);

    // Read initial GPIO level
    int last = gpio_get_level(DEMO_GPIO_INPUT);
    
    int stable_count = 0;

    while (1) {
        
        // Read current GPIO level
        int cur = gpio_get_level(DEMO_GPIO_INPUT);
        
        g_last_gpio_lvl = cur;

        // Check for stability
        if (cur == last) {
            stable_count++;
        } else {
            stable_count = 0;
            last = cur;
        }

        // Set readiness bit if stable for 3 consecutive reads
        if (stable_count >= 3) {
            xEventGroupSetBits(g_evt, EVT_GPIO_READY);
            stable_count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * aggregator_task
 *
 * @brief Aggregates data once all producers have reported readiness.
 *
 * This task demonstrates AND-based Event Group synchronization.
 * It waits until ADC, temperature, and GPIO readiness bits are all set.
 * A timeout is used to detect partial system failures.
 *
 * @param arg Unused.
 */
static void aggregator_task(void *arg)
{
    (void)arg;

    // Block until system initialization is complete
    xEventGroupWaitBits(g_evt, EVT_ALL_INIT_MASK, pdFALSE, pdTRUE, portMAX_DELAY);

    while (1) {
        // Block until all data-ready events are set, with timeout
        EventBits_t bits = xEventGroupWaitBits(
            g_evt,
            EVT_ALL_DATA_MASK,
            pdTRUE,
            pdTRUE,
            pdMS_TO_TICKS(3000)
        );

        // Check if all data-ready bits are set
        if ((bits & EVT_ALL_DATA_MASK) != EVT_ALL_DATA_MASK) {
            ESP_LOGW(APP_TAG, "Aggregator timeout waiting for data");
            continue;
        }

        char payload[160];
        snprintf(payload, sizeof(payload),
                 "{\"ts_ms\":%" PRIu32 ",\"adc\":%d,\"temp\":%.2f,\"gpio\":%d}",
                 millis(), g_last_adc_raw, g_last_temp_c, g_last_gpio_lvl);

        ESP_LOGI(APP_TAG, "Payload: %s", payload);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * diagnostics_task
 *
 * @brief Demonstrates OR-based Event Group synchronization.
 *
 * This task wakes up when any data-ready bit is set and logs which
 * subsystem reported activity.
 *
 * @param arg Unused.
 */
static void diagnostics_task(void *arg)
{
    (void)arg;

    // Block until system initialization is complete
    xEventGroupWaitBits(g_evt, EVT_ALL_INIT_MASK, pdFALSE, pdTRUE, portMAX_DELAY);

    while (1) {
        // Block until any data-ready event is set
        EventBits_t bits = xEventGroupWaitBits(
            g_evt,
            EVT_ANY_DATA_MASK,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(5000)
        );

        if (bits == 0) {
            ESP_LOGI(APP_TAG, "No data events observed");
            continue;
        }

        if (bits & EVT_ADC_READY)  ESP_LOGI(APP_TAG, "ADC ready");
        if (bits & EVT_TEMP_READY) ESP_LOGI(APP_TAG, "Temp ready");
        if (bits & EVT_GPIO_READY) ESP_LOGI(APP_TAG, "GPIO ready");

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * app_main
 *
 * @brief ESP-IDF application entry point.
 *
 * Creates the global Event Group and starts all demo tasks.
 */
void app_main(void) {

    // Create global event group
    g_evt = xEventGroupCreate();
    if (!g_evt) {
        ESP_LOGE(APP_TAG, "Failed to create event group");
        return;
    }


    // Create demo tasks
    xTaskCreatePinnedToCore(init_task, "init_task", STACK_SMALL, NULL, PRIO_INIT, NULL, 0);
    xTaskCreatePinnedToCore(adc_task, "adc_task", STACK_MEDIUM, NULL, PRIO_ADC, NULL, 0);
    xTaskCreatePinnedToCore(temp_task, "temp_task", STACK_SMALL, NULL, PRIO_TEMP, NULL, 0);
    xTaskCreatePinnedToCore(gpio_task, "gpio_task", STACK_SMALL, NULL, PRIO_GPIO, NULL, 0);
    xTaskCreatePinnedToCore(aggregator_task, "aggregator_task", STACK_MEDIUM, NULL, PRIO_AGGREGATOR, NULL, 0);
    xTaskCreatePinnedToCore(diagnostics_task, "diagnostics_task", STACK_SMALL, NULL, PRIO_DIAG, NULL, 0);
}