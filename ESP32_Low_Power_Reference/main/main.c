/**
 * @file main.c
 * @brief Power-efficient ESP32 reference project (ESP-IDF + FreeRTOS).
 *
 * This project is intentionally small and practical. It demonstrates the core
 * firmware techniques that directly impact battery life:
 *
 * 1) Event-driven FreeRTOS tasks (block, do not poll)
 * 2) ESP-IDF power management (DFS + optional automatic light sleep)
 * 3) Deep sleep duty-cycling (wake -> work -> sleep)
 * 4) Explicit Wi-Fi lifecycle (connect -> short transaction -> shutdown)
 * 5) Basic GPIO wake (EXT0) to avoid periodic wakeups when possible
 *
 * Build:
 *   idf.py set-target esp32
 *   idf.py menuconfig
 *   idf.py build flash monitor
 */

#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "wifi_manager.h"

static const char *TAG = "lp_ref";

// Optional "sensor power" GPIO (demo). If you do not have external hardware,
// you can leave this pin unconnected.
#define GPIO_SENSOR_PWR GPIO_NUM_21

static TaskHandle_t s_button_task;

/**
 * @brief Configure ESP-IDF power management (DFS + optional light sleep).
 *
 * In ESP-IDF, automatic frequency scaling and automatic light sleep only work
 * when the scheduler has no runnable tasks. This means your application must
 * block on events (queues, notifications, event groups) instead of polling.
 */
static void enable_power_management(void)
{
#if CONFIG_PM_ENABLE
    esp_pm_config_t cfg = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 40,
        .light_sleep_enable = true,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&cfg));
#endif
}

/**
 * @brief Initialize a GPIO used to power-gate an external sensor.
 *
 * Many IoT designs burn power in external sensors, not the ESP32. A common
 * technique is to power the sensor via a load switch controlled by a GPIO.
 */
static void sensor_power_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << GPIO_SENSOR_PWR,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(GPIO_SENSOR_PWR, 0);
}

/**
 * @brief Power on/off the sensor rail.
 */
static inline void sensor_power_set(bool on)
{
    gpio_set_level(GPIO_SENSOR_PWR, on ? 1 : 0);
}

/**
 * @brief Perform a short "work burst".
 *
 * Replace this with your real sampling and network transaction. The key is to
 * keep the active window short and deterministic.
 */
static void do_work_burst(void)
{
    // Simulate powering a sensor, waiting for settle time, then reading it.
    sensor_power_set(true);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Fake sensor reading.
    int fake_mv = 1830;

    sensor_power_set(false);

    ESP_LOGW(TAG, "sample: adc_mv=%d", fake_mv);

#ifdef CONFIG_LP_ENABLE_WIFI
    // Wi-Fi work should be connect -> short TX -> shutdown.
    esp_err_t err = wifi_manager_connect(CONFIG_LP_WIFI_CONNECT_TIMEOUT_MS);
    if (err == ESP_OK) {
        (void)wifi_manager_demo_tx(CONFIG_LP_WIFI_TX_HOST,
                                  CONFIG_LP_WIFI_TX_PORT,
                                  3000);
    }
    wifi_manager_shutdown();
#endif
}

/**
 * @brief Configure wake sources for deep sleep.
 */
static void configure_wake_sources(void)
{
    // Periodic wake
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup((uint64_t)CONFIG_LP_REPORT_PERIOD_SEC * 1000000ULL));

#if CONFIG_LP_ENABLE_GPIO_WAKE
    // EXT0 uses a single RTC IO pin and level trigger.
    gpio_num_t wake_gpio = (gpio_num_t)CONFIG_LP_WAKE_GPIO;
    // Ensure RTC domain can read the pin state.
    esp_sleep_enable_ext0_wakeup(wake_gpio, CONFIG_LP_WAKE_LEVEL);
#endif
}

/**
 * @brief Enter deep sleep immediately.
 */
static void enter_deep_sleep_now(void)
{
    ESP_LOGW(TAG, "entering deep sleep (%d s)", CONFIG_LP_REPORT_PERIOD_SEC);
    esp_deep_sleep_start();
}

/**
 * @brief ISR for the wake button (runtime event, not deep sleep wake).
 *
 * This demonstrates a low-power FreeRTOS pattern: the task blocks forever and
 * wakes only when the ISR notifies it.
 */
static void IRAM_ATTR button_isr(void *arg)
{
    BaseType_t hp = pdFALSE;
    vTaskNotifyGiveFromISR(s_button_task, &hp);
    if (hp) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Configure a GPIO interrupt to demonstrate event-driven tasks.
 *
 * This interrupt is not used to wake from deep sleep. It is a runtime example
 * of how to avoid polling while the device is awake.
 */
static void runtime_button_init(gpio_num_t gpio_num)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio_num, button_isr, NULL);
}

/**
 * @brief Task that blocks until a button event occurs.
 */
static void button_task(void *arg)
{
    (void)arg;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGW(TAG, "button event -> work burst");
        do_work_burst();

        // In a real product, you may choose to sleep immediately after the event.
        // This reference keeps running until the periodic deep sleep occurs.
    }
}

void app_main(void)
{
    // Enable ESP-IDF power management (DFS + optional light sleep).
    enable_power_management();
    
    // Initialize sensor power GPIO (demo).
    sensor_power_init();

    // Print wakeup cause for debugging your sleep strategy.
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGW(TAG, "wakeup cause=%d", (int)cause);

    // Configure deep sleep wake sources early.
    configure_wake_sources();

    // Start a single event-driven task to demonstrate proper blocking behavior.
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, &s_button_task);

#if CONFIG_LP_ENABLE_GPIO_WAKE
    // If the same GPIO is used for EXT0 wake (common dev board BOOT button),
    // it can also be used as a runtime interrupt for demonstration.
    runtime_button_init((gpio_num_t)CONFIG_LP_WAKE_GPIO);
#else
    runtime_button_init(GPIO_NUM_0);
#endif

    // Perform one work burst after boot/wake, then go back to deep sleep.
    // This is the typical duty-cycled architecture for battery nodes.
    do_work_burst();

    // Give logs a moment to flush. Keep this short.
    vTaskDelay(pdMS_TO_TICKS(50));

    // Enter deep sleep until the next wake event.
    enter_deep_sleep_now();
}
