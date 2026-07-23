#include "secure_storage.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_flash_encrypt.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_secure_boot.h"

#define STORAGE_TAG "secure_storage"
#define STORAGE_PARTITION_LABEL "storage"
#define STORAGE_BASE_PATH "/littlefs"
#define STORAGE_PRIMARY_PATH STORAGE_BASE_PATH "/device_record.bin"
#define STORAGE_BACKUP_PATH STORAGE_BASE_PATH "/device_record.bak"
#define STORAGE_TEMP_PATH STORAGE_BASE_PATH "/device_record.tmp"

#define RECORD_MAGIC 0x53524631UL
#define RECORD_FORMAT_VERSION 1U
#define FNV1A_OFFSET_BASIS 2166136261UL
#define FNV1A_PRIME 16777619UL

/**
 * @brief Defines the serialized header stored before every payload.
 */
typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t header_size;
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t payload_checksum;
    uint32_t header_checksum;
} storage_record_header_t;

static bool s_is_mounted;

/**
 * @brief Calculates a 32-bit FNV-1a checksum.
 *
 * This checksum detects accidental corruption and incomplete writes. It is not
 * a cryptographic message authentication code and must not be treated as proof
 * that an attacker did not modify a record.
 *
 * Args:
 *     data: Bytes to process.
 *     length: Number of bytes in data.
 *
 * Returns:
 *     The calculated 32-bit FNV-1a checksum.
 */
static uint32_t calculate_fnv1a(const uint8_t *data, size_t length)
{
    uint32_t hash = FNV1A_OFFSET_BASIS;

    for (size_t index = 0; index < length; ++index) {
        hash ^= data[index];
        hash *= FNV1A_PRIME;
    }

    return hash;
}

/**
 * @brief Calculates the checksum for a serialized record header.
 *
 * The header checksum field is cleared before calculation so that the checksum
 * covers every other header field.
 *
 * Args:
 *     header: Header to checksum.
 *
 * Returns:
 *     The calculated header checksum.
 */
static uint32_t calculate_header_checksum(const storage_record_header_t *header)
{
    storage_record_header_t working_header = *header;
    working_header.header_checksum = 0U;

    return calculate_fnv1a(
        (const uint8_t *)&working_header,
        sizeof(working_header));
}

/**
 * @brief Converts an errno value into a practical ESP-IDF result.
 *
 * Args:
 *     error_number: errno value captured immediately after a failed operation.
 *
 * Returns:
 *     ESP_ERR_NOT_FOUND for a missing file.
 *     ESP_ERR_NO_MEM for an out-of-memory condition.
 *     ESP_FAIL for all other filesystem errors.
 */
static esp_err_t errno_to_esp_err(int error_number)
{
    if (error_number == ENOENT) {
        return ESP_ERR_NOT_FOUND;
    }

    if (error_number == ENOMEM) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_FAIL;
}

/**
 * @brief Flushes a stream and requests persistence from the VFS layer.
 *
 * Args:
 *     stream: Open stream that was written.
 *
 * Returns:
 *     ESP_OK when both flush operations succeed.
 *     ESP_FAIL when fflush or fsync fails.
 */
static esp_err_t flush_and_sync(FILE *stream)
{
    if (fflush(stream) != 0) {
        ESP_LOGE(STORAGE_TAG, "fflush failed: errno=%d", errno);
        return ESP_FAIL;
    }

    const int file_descriptor = fileno(stream);
    if ((file_descriptor >= 0) && (fsync(file_descriptor) != 0)) {
        ESP_LOGE(STORAGE_TAG, "fsync failed: errno=%d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Removes a path while accepting an already-missing file.
 *
 * Args:
 *     path: File path to remove.
 *
 * Returns:
 *     ESP_OK when the path is removed or does not exist.
 *     ESP_FAIL for another removal error.
 */
static esp_err_t remove_if_present(const char *path)
{
    if (unlink(path) == 0) {
        return ESP_OK;
    }

    if (errno == ENOENT) {
        return ESP_OK;
    }

    ESP_LOGE(STORAGE_TAG, "Failed to remove %s: errno=%d", path, errno);
    return ESP_FAIL;
}

/**
 * @brief Validates an in-memory public record.
 *
 * Args:
 *     record: Record supplied by the caller.
 *
 * Returns:
 *     ESP_OK when the record is valid.
 *     ESP_ERR_INVALID_ARG when record is NULL.
 *     ESP_ERR_INVALID_SIZE when the payload length exceeds the limit.
 */
static esp_err_t validate_public_record(const secure_storage_record_t *record)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (record->payload_length > SECURE_STORAGE_MAX_PAYLOAD_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

/**
 * @brief Writes one complete serialized record to a file.
 *
 * Args:
 *     path: Destination path.
 *     record: Validated public record to serialize.
 *
 * Returns:
 *     ESP_OK when the complete record is written and synchronized.
 *     ESP_FAIL when writing, closing, or synchronization fails.
 */
static esp_err_t write_record_file(
    const char *path,
    const secure_storage_record_t *record)
{
    storage_record_header_t header = {
        .magic = RECORD_MAGIC,
        .format_version = RECORD_FORMAT_VERSION,
        .header_size = sizeof(storage_record_header_t),
        .sequence = record->sequence,
        .payload_length = (uint32_t)record->payload_length,
        .payload_checksum = calculate_fnv1a(
            record->payload,
            record->payload_length),
        .header_checksum = 0U,
    };
    header.header_checksum = calculate_header_checksum(&header);

    FILE *stream = fopen(path, "wb");
    if (stream == NULL) {
        ESP_LOGE(STORAGE_TAG, "Failed to open %s: errno=%d", path, errno);
        return errno_to_esp_err(errno);
    }

    esp_err_t result = ESP_OK;

    if (fwrite(&header, 1U, sizeof(header), stream) != sizeof(header)) {
        ESP_LOGE(STORAGE_TAG, "Failed to write header to %s", path);
        result = ESP_FAIL;
    }

    if ((result == ESP_OK) && (record->payload_length > 0U)) {
        if (fwrite(
                record->payload,
                1U,
                record->payload_length,
                stream) != record->payload_length) {
            ESP_LOGE(STORAGE_TAG, "Failed to write payload to %s", path);
            result = ESP_FAIL;
        }
    }

    if (result == ESP_OK) {
        result = flush_and_sync(stream);
    }

    if (fclose(stream) != 0) {
        ESP_LOGE(STORAGE_TAG, "Failed to close %s: errno=%d", path, errno);
        result = ESP_FAIL;
    }

    return result;
}

/**
 * @brief Reads and validates one serialized record file.
 *
 * Args:
 *     path: Source path.
 *     record: Destination record.
 *
 * Returns:
 *     ESP_OK when the complete record passes validation.
 *     ESP_ERR_NOT_FOUND when the file does not exist.
 *     ESP_ERR_INVALID_CRC when checksums do not match.
 *     ESP_ERR_INVALID_RESPONSE when the record format is invalid.
 *     ESP_FAIL for another filesystem failure.
 */
static esp_err_t read_record_file(
    const char *path,
    secure_storage_record_t *record)
{
    FILE *stream = fopen(path, "rb");
    if (stream == NULL) {
        return errno_to_esp_err(errno);
    }

    storage_record_header_t header = {0};
    esp_err_t result = ESP_OK;

    if (fread(&header, 1U, sizeof(header), stream) != sizeof(header)) {
        ESP_LOGW(STORAGE_TAG, "Record header is incomplete: %s", path);
        result = ESP_ERR_INVALID_RESPONSE;
    }

    if ((result == ESP_OK) &&
        ((header.magic != RECORD_MAGIC) ||
         (header.format_version != RECORD_FORMAT_VERSION) ||
         (header.header_size != sizeof(storage_record_header_t)) ||
         (header.payload_length > SECURE_STORAGE_MAX_PAYLOAD_SIZE))) {
        ESP_LOGW(STORAGE_TAG, "Record format is invalid: %s", path);
        result = ESP_ERR_INVALID_RESPONSE;
    }

    if ((result == ESP_OK) &&
        (header.header_checksum != calculate_header_checksum(&header))) {
        ESP_LOGW(STORAGE_TAG, "Record header checksum failed: %s", path);
        result = ESP_ERR_INVALID_CRC;
    }

    if (result == ESP_OK) {
        memset(record, 0, sizeof(*record));
        record->sequence = header.sequence;
        record->payload_length = header.payload_length;

        if ((record->payload_length > 0U) &&
            (fread(
                record->payload,
                1U,
                record->payload_length,
                stream) != record->payload_length)) {
            ESP_LOGW(STORAGE_TAG, "Record payload is incomplete: %s", path);
            result = ESP_ERR_INVALID_RESPONSE;
        }
    }

    if ((result == ESP_OK) &&
        (header.payload_checksum != calculate_fnv1a(
            record->payload,
            record->payload_length))) {
        ESP_LOGW(STORAGE_TAG, "Record payload checksum failed: %s", path);
        result = ESP_ERR_INVALID_CRC;
    }

    // A valid record must end immediately after the declared payload.
    if ((result == ESP_OK) && (fgetc(stream) != EOF)) {
        ESP_LOGW(STORAGE_TAG, "Record contains unexpected trailing data: %s", path);
        result = ESP_ERR_INVALID_RESPONSE;
    }

    if (fclose(stream) != 0) {
        ESP_LOGE(STORAGE_TAG, "Failed to close %s: errno=%d", path, errno);
        result = ESP_FAIL;
    }

    return result;
}

/**
 * @brief Restores a validated backup record to the primary path.
 *
 * Args:
 *     record: Validated backup record to write as the primary record.
 *
 * Returns:
 *     ESP_OK when the primary record is restored.
 *     Another ESP-IDF error code when restoration fails.
 */
static esp_err_t restore_primary_from_backup(
    const secure_storage_record_t *record)
{
    ESP_LOGW(STORAGE_TAG, "Restoring primary record from validated backup");

    const esp_err_t result = write_record_file(STORAGE_TEMP_PATH, record);
    if (result != ESP_OK) {
        return result;
    }

    if (rename(STORAGE_TEMP_PATH, STORAGE_PRIMARY_PATH) != 0) {
        ESP_LOGE(
            STORAGE_TAG,
            "Failed to restore primary record: errno=%d",
            errno);
        (void)remove_if_present(STORAGE_TEMP_PATH);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t secure_storage_init(void)
{
    if (s_is_mounted) {
        return ESP_OK;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        STORAGE_PARTITION_LABEL);

    if (partition == NULL) {
        ESP_LOGE(
            STORAGE_TAG,
            "Partition '%s' was not found",
            STORAGE_PARTITION_LABEL);
        return ESP_ERR_NOT_FOUND;
    }

    if (!partition->encrypted) {
        ESP_LOGE(
            STORAGE_TAG,
            "Partition '%s' is not marked encrypted",
            STORAGE_PARTITION_LABEL);
        return ESP_ERR_INVALID_STATE;
    }

    const esp_vfs_littlefs_conf_t configuration = {
        .base_path = STORAGE_BASE_PATH,
        .partition_label = STORAGE_PARTITION_LABEL,
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    const esp_err_t result = esp_vfs_littlefs_register(&configuration);
    if (result != ESP_OK) {
        ESP_LOGE(
            STORAGE_TAG,
            "LittleFS mount failed: %s",
            esp_err_to_name(result));
        return result;
    }

    s_is_mounted = true;
    ESP_LOGI(STORAGE_TAG, "LittleFS mounted at %s", STORAGE_BASE_PATH);
    return ESP_OK;
}

esp_err_t secure_storage_deinit(void)
{
    if (!s_is_mounted) {
        return ESP_OK;
    }

    const esp_err_t result =
        esp_vfs_littlefs_unregister(STORAGE_PARTITION_LABEL);

    if (result == ESP_OK) {
        s_is_mounted = false;
        ESP_LOGI(STORAGE_TAG, "LittleFS unmounted");
    }

    return result;
}

void secure_storage_report_security_state(void)
{
    const bool flash_encryption_enabled = esp_flash_encryption_enabled();
    const bool secure_boot_enabled = esp_secure_boot_enabled();

    ESP_LOGI(
        STORAGE_TAG,
        "Flash Encryption: %s",
        flash_encryption_enabled ? "ENABLED" : "DISABLED");
    ESP_LOGI(
        STORAGE_TAG,
        "Secure Boot: %s",
        secure_boot_enabled ? "ENABLED" : "DISABLED");

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        STORAGE_PARTITION_LABEL);

    if (partition == NULL) {
        ESP_LOGE(
            STORAGE_TAG,
            "Storage partition '%s' is missing",
            STORAGE_PARTITION_LABEL);
        return;
    }

    ESP_LOGI(
        STORAGE_TAG,
        "Storage partition encrypted flag: %s",
        partition->encrypted ? "SET" : "NOT SET");

    if (!flash_encryption_enabled) {
        ESP_LOGW(
            STORAGE_TAG,
            "Raw LittleFS data is not protected until Flash Encryption is enabled");
    }

    if (!secure_boot_enabled) {
        ESP_LOGW(
            STORAGE_TAG,
            "Unsigned firmware may bypass storage controls until Secure Boot is enabled");
    }
}

esp_err_t secure_storage_load_record(secure_storage_record_t *record)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t primary_result =
        read_record_file(STORAGE_PRIMARY_PATH, record);

    if (primary_result == ESP_OK) {
        return ESP_OK;
    }

    secure_storage_record_t backup_record = {0};
    const esp_err_t backup_result =
        read_record_file(STORAGE_BACKUP_PATH, &backup_record);

    if (backup_result == ESP_OK) {
        *record = backup_record;
        const esp_err_t restore_result =
            restore_primary_from_backup(&backup_record);

        if (restore_result != ESP_OK) {
            ESP_LOGW(
                STORAGE_TAG,
                "Backup is valid, but primary restoration failed: %s",
                esp_err_to_name(restore_result));
        }

        return ESP_OK;
    }

    if ((primary_result == ESP_ERR_NOT_FOUND) &&
        (backup_result == ESP_ERR_NOT_FOUND)) {
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGE(
        STORAGE_TAG,
        "No valid record; primary=%s, backup=%s",
        esp_err_to_name(primary_result),
        esp_err_to_name(backup_result));
    return ESP_ERR_INVALID_CRC;
}

esp_err_t secure_storage_store_record(
    const secure_storage_record_t *record)
{
    esp_err_t result = validate_public_record(record);
    if (result != ESP_OK) {
        return result;
    }

    // Remove any temporary file left by an interrupted older transaction.
    result = remove_if_present(STORAGE_TEMP_PATH);
    if (result != ESP_OK) {
        return result;
    }

    result = write_record_file(STORAGE_TEMP_PATH, record);
    if (result != ESP_OK) {
        (void)remove_if_present(STORAGE_TEMP_PATH);
        return result;
    }

    // Read the temporary file back before replacing the committed record.
    secure_storage_record_t verification_record = {0};
    result = read_record_file(STORAGE_TEMP_PATH, &verification_record);
    if (result != ESP_OK) {
        (void)remove_if_present(STORAGE_TEMP_PATH);
        return result;
    }

    if ((verification_record.sequence != record->sequence) ||
        (verification_record.payload_length != record->payload_length) ||
        (memcmp(
            verification_record.payload,
            record->payload,
            record->payload_length) != 0)) {
        ESP_LOGE(STORAGE_TAG, "Temporary record verification mismatch");
        (void)remove_if_present(STORAGE_TEMP_PATH);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Preserve the previous valid record as a recovery point.
    result = remove_if_present(STORAGE_BACKUP_PATH);
    if (result != ESP_OK) {
        (void)remove_if_present(STORAGE_TEMP_PATH);
        return result;
    }

    struct stat file_status = {0};
    if (stat(STORAGE_PRIMARY_PATH, &file_status) == 0) {
        if (rename(STORAGE_PRIMARY_PATH, STORAGE_BACKUP_PATH) != 0) {
            ESP_LOGE(
                STORAGE_TAG,
                "Failed to create backup record: errno=%d",
                errno);
            (void)remove_if_present(STORAGE_TEMP_PATH);
            return ESP_FAIL;
        }
    } else if (errno != ENOENT) {
        ESP_LOGE(
            STORAGE_TAG,
            "Failed to inspect primary record: errno=%d",
            errno);
        (void)remove_if_present(STORAGE_TEMP_PATH);
        return ESP_FAIL;
    }

    if (rename(STORAGE_TEMP_PATH, STORAGE_PRIMARY_PATH) != 0) {
        ESP_LOGE(
            STORAGE_TAG,
            "Failed to commit primary record: errno=%d",
            errno);

        // Attempt to restore the previous primary record after a failed rename.
        if (stat(STORAGE_BACKUP_PATH, &file_status) == 0) {
            (void)rename(STORAGE_BACKUP_PATH, STORAGE_PRIMARY_PATH);
        }

        (void)remove_if_present(STORAGE_TEMP_PATH);
        return ESP_FAIL;
    }

    secure_storage_record_t committed_record = {0};
    result = read_record_file(STORAGE_PRIMARY_PATH, &committed_record);
    if (result != ESP_OK) {
        ESP_LOGE(
            STORAGE_TAG,
            "Committed record verification failed: %s",
            esp_err_to_name(result));
        return result;
    }

    return ESP_OK;
}

esp_err_t secure_storage_print_usage(void)
{
    size_t total_bytes = 0U;
    size_t used_bytes = 0U;

    const esp_err_t result = esp_littlefs_info(
        STORAGE_PARTITION_LABEL,
        &total_bytes,
        &used_bytes);

    if (result == ESP_OK) {
        ESP_LOGI(
            STORAGE_TAG,
            "LittleFS usage: %u / %u bytes",
            (unsigned int)used_bytes,
            (unsigned int)total_bytes);
    }

    return result;
}
