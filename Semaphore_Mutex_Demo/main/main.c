/**
 * @file main.c
 * @brief Practical FreeRTOS synchronization demo on ESP32 using ESP-IDF.
 *
 * This single-file demo integrates three real-world synchronization patterns:
 * 1) Mutex: Protect a shared I2C bus across multiple tasks (resource protection).
 * 2) Binary semaphore: Signal a task from a GPIO ISR (event notification).
 * 3) Counting semaphore: Limit concurrent access to a pool of identical resources.
 *
 * Build:
 *   idf.py set-target esp32
 *   idf.py build flash monitor
 *
 * Notes:
 * - GPIO interrupt example uses GPIO0 (often BOOT button) with pull-up and falling-edge interrupt.
 * - I2C pins default to SDA=GPIO8 and SCL=GPIO9. Adjust if needed.
 */

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/i2c.h"

/* --------------------------- Configuration --------------------------- */

#define TAG "sync_demo"

/* GPIO interrupt example (BOOT on many devkits). */
#define DEMO_GPIO_INPUT          GPIO_NUM_0

/* I2C example defaults (adjust for your board if needed). */
#define DEMO_I2C_PORT            I2C_NUM_0
#define DEMO_I2C_SDA_GPIO        GPIO_NUM_8
#define DEMO_I2C_SCL_GPIO        GPIO_NUM_9
#define DEMO_I2C_FREQ_HZ         100000

/* Counting semaphore resource pool example. */
#define BUFFER_POOL_SIZE         3
#define WORKER_TASK_COUNT        5

/* --------------------------- Globals --------------------------- */

static SemaphoreHandle_t g_i2c_mutex = NULL;
static SemaphoreHandle_t g_gpio_sem = NULL;
static SemaphoreHandle_t g_pool_sem = NULL;

/* --------------------------- Forward Declarations --------------------------- */

static esp_err_t demo_i2c_init(void);
static void demo_gpio_init(void);

static void i2c_task_sensor(void *arg);
static void i2c_task_eeprom(void *arg);

static void gpio_event_task(void *arg);
static void worker_task(void *arg);

static void IRAM_ATTR gpio_isr_handler(void *arg);

/* --------------------------- Helpers --------------------------- */

/**
 * @brief Initialize I2C in master mode for demo usage.
 *
 * This config is intentionally simple. The demo performs minimal bus usage
 * to illustrate mutex protection and scheduling effects.
 *
 * Returns:
 *   ESP_OK on success, or an ESP-IDF error code on failure.
 */
static esp_err_t demo_i2c_init(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DEMO_I2C_SDA_GPIO,
        .scl_io_num = DEMO_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = DEMO_I2C_FREQ_HZ,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(DEMO_I2C_PORT, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(DEMO_I2C_PORT, cfg.mode, 0, 0, 0);
}

/**
 * @brief Configure a GPIO input with falling-edge interrupt and install ISR service.
 *
 * The ISR gives a binary semaphore to wake a task, which then performs all
 * non-ISR work (logging, debouncing, processing).
 *
 * Args:
 *   None
 *
 * Returns:
 *   None
 */
static void demo_gpio_init(void)
{
    // Configure GPIO input with pull-up and falling-edge interrupt
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << DEMO_GPIO_INPUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    // Configure the GPIO
    ESP_ERROR_CHECK(gpio_config(&io));

    // Install ISR service once.
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(DEMO_GPIO_INPUT, gpio_isr_handler, (void *)DEMO_GPIO_INPUT));
}

/* --------------------------- ISR --------------------------- */

/**
 * @brief GPIO ISR: give the semaphore to unblock the event task.
 *
 * This ISR must be short. It only signals the task and optionally yields to a
 * higher-priority task immediately.
 *
 * Args:
 *   arg: GPIO number casted to void*.
 *
 * Returns:
 *   None
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    (void)arg;

    BaseType_t higher_woken = pdFALSE;

    // Give the GPIO semaphore to wake the event task
    if (g_gpio_sem != NULL) {
        xSemaphoreGiveFromISR(g_gpio_sem, &higher_woken);
    }

    // Yield to the higher-priority task if needed
    if (higher_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/* --------------------------- Tasks --------------------------- */

/**
 * @brief Task A: pretend to read a sensor over I2C, protected by a mutex.
 *
 * The task takes the I2C mutex, performs a minimal I2C transaction stub (or just
 * a short critical region), then releases the mutex. This represents the common
 * "shared bus" scenario in embedded firmware.
 *
 * Args:
 *   arg: Unused (NULL).
 *
 * Returns:
 *   None (FreeRTOS task).
 */
static void i2c_task_sensor(void *arg)
{
    (void)arg;

    while (true) {
        if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            ESP_LOGI(TAG, "I2C SENSOR: bus locked");

            // Create and initialize an I2C commands list with a given buffer
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            
            // Start the I2C command sequence
            i2c_master_start(cmd);
            
            // Wtrite to device address 0x48 (typical temp sensor)
            i2c_master_write_byte(cmd, (0x48 << 1) | I2C_MASTER_WRITE, true);
            
            // End the I2C command sequence
            i2c_master_stop(cmd);
            
            // Execute the I2C command
            (void)i2c_master_cmd_begin(DEMO_I2C_PORT, cmd, pdMS_TO_TICKS(20));
            
            // Delete the I2C commands list to free resources
            i2c_cmd_link_delete(cmd);

            ESP_LOGI(TAG, "I2C SENSOR: bus released");
            xSemaphoreGive(g_i2c_mutex);
        } else {
            ESP_LOGW(TAG, "I2C SENSOR: failed to lock bus (timeout)");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Task B: pretend to write to EEPROM over I2C, protected by a mutex.
 *
 * This task competes with the sensor task for the same I2C bus mutex.
 * The point is not successful I2C I/O, but correct mutual exclusion.
 *
 * Args:
 *   arg: Unused (NULL).
 *
 * Returns:
 *   None (FreeRTOS task).
 */
static void i2c_task_eeprom(void *arg)
{
    (void)arg;

    while (true) {
        if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            ESP_LOGI(TAG, "I2C EEPROM: bus locked");

            // Create and initialize an I2C commands list with a given buffer
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();

            // Start the I2C command sequence
            i2c_master_start(cmd);
            
            // Write to device address 0x50 (typical EEPROM)
            i2c_master_write_byte(cmd, (0x50 << 1) | I2C_MASTER_WRITE, true);
            
            // Write two bytes of "data".
            i2c_master_write_byte(cmd, 0x00, true);
            i2c_master_write_byte(cmd, 0xAA, true);

            // End the I2C command sequence
            i2c_master_stop(cmd);
            
            // Execute the I2C command
            (void)i2c_master_cmd_begin(DEMO_I2C_PORT, cmd, pdMS_TO_TICKS(20));
            
            // Delete the I2C commands list to free resources
            i2c_cmd_link_delete(cmd);

            ESP_LOGI(TAG, "I2C EEPROM: bus released");
            xSemaphoreGive(g_i2c_mutex);
        } else {
            ESP_LOGW(TAG, "I2C EEPROM: failed to lock bus (timeout)");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * @brief Task: wait for GPIO ISR events via a binary semaphore.
 *
 * This task blocks on a binary semaphore that is "given" by the GPIO ISR.
 * All real processing should happen here, not in the ISR.
 *
 * Args:
 *   arg: Unused (NULL).
 *
 * Returns:
 *   None (FreeRTOS task).
 */
static void gpio_event_task(void *arg)
{
    (void)arg;

    while (true) {
        if (xSemaphoreTake(g_gpio_sem, portMAX_DELAY) == pdTRUE) {
            // Basic "debounce" delay for a mechanical button.
            vTaskDelay(pdMS_TO_TICKS(40));

            // Read GPIO level to confirm the event.
            int level = gpio_get_level(DEMO_GPIO_INPUT);
            ESP_LOGI(TAG, "GPIO EVENT: ISR signaled (gpio=%d level=%d)", (int)DEMO_GPIO_INPUT, level);
        }
    }
}

/**
 * @brief Task: worker that consumes a slot from a counting semaphore.
 *
 * The counting semaphore represents a pool of BUFFER_POOL_SIZE identical resources.
 * Only BUFFER_POOL_SIZE workers can "own" a resource simultaneously.
 *
 * Args:
 *   arg: Pointer-sized integer (task index) casted to void*.
 *
 * Returns:
 *   None (FreeRTOS task).
 */
static void worker_task(void *arg)
{
    int id = (int)(intptr_t)arg;

    while (true) {
        // Acquire one resource from the pool.
        if (xSemaphoreTake(g_pool_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "WORKER %d: acquired pool slot", id);

            // Simulate work holding a scarce resource (buffer, slot, handle).
            vTaskDelay(pdMS_TO_TICKS(600));

            ESP_LOGI(TAG, "WORKER %d: releasing pool slot", id);
            xSemaphoreGive(g_pool_sem);
        } else {
            ESP_LOGW(TAG, "WORKER %d: timed out waiting for pool slot", id);
        }

        // Stagger workers slightly.
        vTaskDelay(pdMS_TO_TICKS(200 + (id * 50)));
    }
}

/* --------------------------- App Entry --------------------------- */

/**
 * @brief ESP-IDF application entry point.
 *
 * This function initializes:
 * - I2C driver (for the mutex-protected shared bus demo)
 * - GPIO interrupt + binary semaphore (for ISR-to-task signaling demo)
 * - Counting semaphore (resource pool demo)
 * Then it starts tasks for each scenario.
 *
 * Returns:
 *   None
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Semaphore vs Mutex demo");

    // Create primitives first (best practice).
    g_i2c_mutex = xSemaphoreCreateMutex();
    g_gpio_sem = xSemaphoreCreateBinary();
    g_pool_sem = xSemaphoreCreateCounting(BUFFER_POOL_SIZE, BUFFER_POOL_SIZE);

    if (g_i2c_mutex == NULL || g_gpio_sem == NULL || g_pool_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create synchronization primitives");
        return;
    }

    // Initialize peripherals.
    esp_err_t err = demo_i2c_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C init failed (%s). Mutex demo will still run, but I2C calls may fail.",
                 esp_err_to_name(err));
    }

    // GPIO init for interrupt demo.
    demo_gpio_init();

    // Start tasks. 
    xTaskCreate(i2c_task_sensor, "i2c_sensor", 4096, NULL, 5, NULL);
    xTaskCreate(i2c_task_eeprom, "i2c_eeprom", 4096, NULL, 5, NULL);
    xTaskCreate(gpio_event_task, "gpio_evt", 3072, NULL, 10, NULL);

    for (int i = 0; i < WORKER_TASK_COUNT; i++) {
        xTaskCreate(worker_task, "worker", 3072, (void *)(intptr_t)i, 4, NULL);
    }

    ESP_LOGI(TAG, "Tasks started. Press BOOT (GPIO0) to trigger GPIO semaphore.");
}
