/**
 * @file app_main.c
 * @brief ESP32-C6 power gating demo application (ESP-IDF).
 *
 * This project demonstrates multiple power gating techniques using a common firmware
 * interface. The actual hardware gating method is selected in menuconfig:
 *
 * - Regulator EN pin gating
 * - Load switch gating
 * - PFET high-side gating using a driver stage
 *
 * The demo focuses on safe sequencing:
 * 1) Enable gated rail
 * 2) Wait for stabilization
 * 3) Perform "work" (sensor read stub)
 * 4) Put buses into a safe state (avoid back-powering)
 * 5) Disable gated rail
 * 6) Enter deep sleep
 *
 * Notes:
 * - This demo intentionally keeps I2C handling as a stub to stay hardware-agnostic.
 * - In a real design, use external pullups on the gated rail if sensors are gated.
 */

#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_random.h"

#include "power_gating.h"
#include "bus_safe.h"
#include "sleep_ctrl.h"

static const char *TAG = "main";

/** Event bit: measurement completed. */
#define EVT_MEAS_DONE    (1U << 0)
/** Event bit: comm completed. */
#define EVT_COMM_DONE    (1U << 1)

static EventGroupHandle_t s_evt;

/**
 * @brief Build the power gating configuration from Kconfig.
 *
 * @return pg_config_t configuration.
 */
static pg_config_t build_pg_config_from_kconfig(void)
{
    pg_config_t cfg = {0};

#if CONFIG_PG_TECH_REG_EN
    cfg.technique = PG_TECH_REG_EN;
#elif CONFIG_PG_TECH_LOAD_SWITCH
    cfg.technique = PG_TECH_LOAD_SWITCH;
#elif CONFIG_PG_TECH_PFET_DRIVER
    cfg.technique = PG_TECH_PFET_DRIVER;
#else
    cfg.technique = PG_TECH_REG_EN;
#endif

    cfg.enable_gpio = CONFIG_PG_ENABLE_GPIO;
    cfg.active_high = CONFIG_PG_ACTIVE_HIGH;
    cfg.stabilize_ms = CONFIG_PG_STABILIZE_MS;

    return cfg;
}

/**
 * @brief Build the bus-safe configuration from Kconfig.
 *
 * @return bus_safe_config_t configuration.
 */
static bus_safe_config_t build_bus_config_from_kconfig(void)
{
    bus_safe_config_t cfg = {
        .i2c_scl_gpio = CONFIG_PG_BUS_I2C_SCL_GPIO,
        .i2c_sda_gpio = CONFIG_PG_BUS_I2C_SDA_GPIO,
    };
    return cfg;
}

/**
 * @brief Fake sensor read to simulate a real peripheral transaction.
 *
 * In a real product this function would initialize I2C/SPI, talk to a sensor,
 * then deinitialize the bus before power-off. Here we return a random value
 * to keep the demo hardware-agnostic.
 *
 * @return uint32_t Sample value.
 */
static uint32_t fake_sensor_read(void)
{
    // Mix two random sources to get a changing value across boots.
    uint32_t a = esp_random();
    uint32_t b = (uint32_t)xTaskGetTickCount();
    return (a ^ (b << 1)) & 0x0FFFU;
}

/**
 * @brief Measurement task: powers on rail, reads sensor, signals completion.
 *
 * @param arg Unused.
 */
static void task_measurement(void *arg)
{
    (void)arg;

    const pg_config_t *pg = pg_get_config();

    ESP_LOGI(TAG, "Measurement: enabling rail (GPIO=%d)", pg->enable_gpio);
    pg_set_enabled(true);

    // Wait for rail rise time and sensor startup.
    vTaskDelay(pdMS_TO_TICKS(pg->stabilize_ms));

    uint32_t sample = fake_sensor_read();

#if CONFIG_PG_LOG_SAMPLE
    ESP_LOGI(TAG, "Measurement: sample=%" PRIu32, sample);
#endif

    // In real code: deinit I2C/SPI here before cutting power.
    xEventGroupSetBits(s_evt, EVT_MEAS_DONE);

    // Delete self when done.
    vTaskDelete(NULL);
}

/**
 * @brief Communication task: waits for measurement then simulates sending data.
 *
 * In real firmware this task would enable Wi-Fi / BLE, publish telemetry, then
 * shut down the radio before deep sleep.
 *
 * @param arg Unused.
 */
static void task_comm(void *arg)
{
    (void)arg;

    // Wait for measurement to complete.
    xEventGroupWaitBits(s_evt, EVT_MEAS_DONE, pdFALSE, pdTRUE, portMAX_DELAY);

    // Simulate a short communication window.
    ESP_LOGI(TAG, "Comm: simulated transmit");
    vTaskDelay(pdMS_TO_TICKS(50));

    // Create event to signal completion.
    xEventGroupSetBits(s_evt, EVT_COMM_DONE);

    // Delete self when done.
    vTaskDelete(NULL);
}

/**
 * @brief Power manager task: orchestrates shutdown, gates rail, and sleeps.
 *
 * This task owns the power transition. It waits for other tasks to complete,
 * applies bus-safe states to prevent phantom powering, gates off the rail,
 * and enters deep sleep.
 *
 * @param arg Unused.
 */
static void task_power_manager(void *arg)
{
    (void)arg;

    // Wait for measurement and communication to complete.  
    xEventGroupWaitBits(s_evt, EVT_MEAS_DONE | EVT_COMM_DONE, pdFALSE, pdTRUE, portMAX_DELAY);

    // Apply bus-safe states before powering off the rail.
    ESP_LOGI(TAG, "Power manager: applying bus-safe state");
    bus_safe_apply_before_power_off();

    // Disable the gated rail.
    ESP_LOGI(TAG, "Power manager: disabling rail");
    pg_set_enabled(false);

    // Small delay to allow rail to collapse, helpful when measuring.
    vTaskDelay(pdMS_TO_TICKS(5));

    // Enter deep sleep until next measurement cycle.
    sleep_ctrl_enter_deep_sleep(CONFIG_PG_WAKE_INTERVAL_S);
}

void app_main(void)
{
    // Create synchronization primitive first.
    s_evt = xEventGroupCreate();

    // Configure bus-safe handling early.
    bus_safe_config_t bus_cfg = build_bus_config_from_kconfig();
    bus_safe_init(&bus_cfg);

    // Initialize power gating driver.
    pg_config_t pg_cfg = build_pg_config_from_kconfig();
    pg_init(&pg_cfg);

    ESP_LOGI(TAG, "Boot: starting tasks");

    // Create application tasks.
    xTaskCreate(task_measurement, "meas", 4096, NULL, 5, NULL);
    xTaskCreate(task_comm, "comm", 4096, NULL, 4, NULL);
    xTaskCreate(task_power_manager, "pwrmgr", 4096, NULL, 6, NULL);
}