#ifndef SECURE_STORAGE_H
#define SECURE_STORAGE_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SECURE_STORAGE_MAX_PAYLOAD_SIZE 512U

/**
 * @brief Describes an application record stored in LittleFS.
 */
typedef struct {
    uint32_t sequence;
    size_t payload_length;
    uint8_t payload[SECURE_STORAGE_MAX_PAYLOAD_SIZE];
} secure_storage_record_t;

/**
 * @brief Mounts the LittleFS partition and verifies its security metadata.
 *
 * The function verifies that the configured partition exists and carries the
 * encrypted partition flag. The flag causes transparent hardware encryption
 * only when ESP32-S3 Flash Encryption is active.
 *
 * Returns:
 *     ESP_OK when the filesystem is mounted.
 *     ESP_ERR_NOT_FOUND when the storage partition does not exist.
 *     ESP_ERR_INVALID_STATE when the partition is not marked encrypted.
 *     Another ESP-IDF error code when mounting fails.
 */
esp_err_t secure_storage_init(void);

/**
 * @brief Unmounts LittleFS and releases the VFS registration.
 *
 * Returns:
 *     ESP_OK when the filesystem is unmounted.
 *     Another ESP-IDF error code when unregistration fails.
 */
esp_err_t secure_storage_deinit(void);

/**
 * @brief Reports the active ESP32-S3 hardware security state.
 *
 * The function logs Flash Encryption, Secure Boot, and partition encryption
 * status. It does not modify eFuses or security configuration.
 */
void secure_storage_report_security_state(void);

/**
 * @brief Loads the most recent valid application record.
 *
 * The primary record is checked first. When it is missing or invalid, the
 * backup record is checked. A valid backup is copied back to the primary path.
 *
 * Args:
 *     record: Destination for the validated record.
 *
 * Returns:
 *     ESP_OK when a valid record is loaded.
 *     ESP_ERR_NOT_FOUND when no valid record exists.
 *     ESP_ERR_INVALID_ARG when record is NULL.
 *     ESP_ERR_INVALID_CRC when existing records fail validation.
 *     Another ESP-IDF error code when file access fails.
 */
esp_err_t secure_storage_load_record(secure_storage_record_t *record);

/**
 * @brief Atomically stores an application record.
 *
 * The function writes and validates a temporary record, moves the current
 * primary record to a backup path, and then renames the temporary record to the
 * primary path.
 *
 * Args:
 *     record: Record to persist.
 *
 * Returns:
 *     ESP_OK when the record is committed and verified.
 *     ESP_ERR_INVALID_ARG when the record is invalid.
 *     ESP_ERR_INVALID_SIZE when the payload is too large.
 *     Another ESP-IDF error code when a filesystem operation fails.
 */
esp_err_t secure_storage_store_record(const secure_storage_record_t *record);

/**
 * @brief Prints LittleFS total and used capacity.
 *
 * Returns:
 *     ESP_OK when filesystem information is available.
 *     Another ESP-IDF error code when the information query fails.
 */
esp_err_t secure_storage_print_usage(void);

#ifdef __cplusplus
}
#endif

#endif
