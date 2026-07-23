#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "secure_storage.h"

#define APP_TAG "secure_littlefs"
#define STORAGE_TASK_STACK_SIZE 6144U
#define STORAGE_TASK_PRIORITY 5U
#define STORAGE_TEST_INTERVAL_MS 15000U

/**
 * @brief Builds the demonstration payload stored in LittleFS.
 *
 * The sample contains non-secret placeholder values. Production firmware must
 * never log or embed real credentials in source code.
 *
 * Args:
 *     record: Record whose payload will be populated.
 *
 * Returns:
 *     ESP_OK when the payload fits in the record buffer.
 *     ESP_ERR_INVALID_ARG when record is NULL.
 *     ESP_ERR_INVALID_SIZE when the generated text is too large.
 */
static esp_err_t build_demonstration_payload(
    secure_storage_record_t *record)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const int written = snprintf(
        (char *)record->payload,
        sizeof(record->payload),
        "{"
        "\"device\":\"esp32s3-secure-node\","
        "\"sequence\":%" PRIu32 ","
        "\"provisioned\":true,"
        "\"credential\":\"DEMO-NOT-A-REAL-SECRET\""
        "}",
        record->sequence);

    if ((written < 0) || ((size_t)written >= sizeof(record->payload))) {
        return ESP_ERR_INVALID_SIZE;
    }

    record->payload_length = (size_t)written;
    return ESP_OK;
}

/**
 * @brief Prints a validated record without exposing production secrets.
 *
 * This demonstration prints the complete placeholder payload. Replace this
 * behavior in a real product so that decrypted credentials are never logged.
 *
 * Args:
 *     record: Validated record to display.
 */
static void print_demonstration_record(
    const secure_storage_record_t *record)
{
    ESP_LOGI(
        APP_TAG,
        "Validated record sequence: %" PRIu32,
        record->sequence);
    ESP_LOGI(
        APP_TAG,
        "Validated payload length: %u bytes",
        (unsigned int)record->payload_length);
    ESP_LOGI(
        APP_TAG,
        "Demo payload: %.*s",
        (int)record->payload_length,
        (const char *)record->payload);
}

/**
 * @brief Executes the recurring secure storage verification workflow.
 *
 * The task loads the most recent valid record, increments its sequence number,
 * atomically stores a new record, and reads it back for verification.
 *
 * Args:
 *     context: Unused FreeRTOS task context.
 */
static void secure_storage_task(void *context)
{
    (void)context;

    uint32_t next_sequence = 1U;
    secure_storage_record_t existing_record = {0};

    const esp_err_t load_result =
        secure_storage_load_record(&existing_record);

    if (load_result == ESP_OK) {
        print_demonstration_record(&existing_record);
        next_sequence = existing_record.sequence + 1U;
    } else if (load_result == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(APP_TAG, "No previous record exists; creating the first record");
    } else {
        ESP_LOGE(
            APP_TAG,
            "Existing storage record is invalid: %s",
            esp_err_to_name(load_result));
    }

    while (true) {
        secure_storage_record_t new_record = {
            .sequence = next_sequence,
            .payload_length = 0U,
            .payload = {0},
        };

        esp_err_t result =
            build_demonstration_payload(&new_record);

        if (result == ESP_OK) {
            result = secure_storage_store_record(&new_record);
        }

        if (result == ESP_OK) {
            secure_storage_record_t verified_record = {0};
            result = secure_storage_load_record(&verified_record);

            if (result == ESP_OK) {
                print_demonstration_record(&verified_record);
                next_sequence = verified_record.sequence + 1U;
            }
        }

        if (result != ESP_OK) {
            ESP_LOGE(
                APP_TAG,
                "Storage transaction failed: %s",
                esp_err_to_name(result));
        }

        (void)secure_storage_print_usage();
        vTaskDelay(pdMS_TO_TICKS(STORAGE_TEST_INTERVAL_MS));
    }
}

/**
 * @brief Application entry point.
 *
 * The function reports the hardware security state, mounts encrypted LittleFS,
 * and starts the storage verification task.
 */
void app_main(void)
{
    ESP_LOGI(APP_TAG, "ESP32-S3 Secure LittleFS example starting");

    // Report the hardware security state before any filesystem operations.
    secure_storage_report_security_state();

    // Initialize the secure storage subsystem and mount LittleFS.
    const esp_err_t result = secure_storage_init();
    if (result != ESP_OK) {
        ESP_LOGE(
            APP_TAG,
            "Secure storage initialization failed: %s",
            esp_err_to_name(result));
        ESP_LOGE(
            APP_TAG,
            "The application will restart after a diagnostic delay");
        vTaskDelay(pdMS_TO_TICKS(10000U));
        esp_restart();
        return;
    }

    // Create the secure storage task.
    const BaseType_t task_result = xTaskCreate(
        secure_storage_task,
        "secure_storage_task",
        STORAGE_TASK_STACK_SIZE,
        NULL,
        STORAGE_TASK_PRIORITY,
        NULL);

    // If the task creation fails, deinitialize secure storage and restart.
    if (task_result != pdPASS) {
        ESP_LOGE(APP_TAG, "Failed to create secure storage task");
        (void)secure_storage_deinit();
        vTaskDelay(pdMS_TO_TICKS(10000U));
        esp_restart();
    }
}
