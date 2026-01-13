/**
 * @file main.c
 * @brief ESP32-S3 custom eFuse fields demo with build-time CSV table generation.
 *
 * This demo shows how to:
 *   - Define custom user eFuse fields in main/esp_efuse_custom_table.csv.
 *   - Generate esp_efuse_custom_table.c and esp_efuse_custom_table.h during the build.
 *   - Read and optionally program the custom fields using the ESP-IDF eFuse API.
 *
 * Safety notes:
 *   - eFuses are one-time programmable (0 -> 1). Burning is irreversible.
 *   - This project defaults to virtual eFuses via sdkconfig.defaults.
 *   - To really burn silicon, disable CONFIG_EFUSE_VIRTUAL in menuconfig.
 */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_efuse.h"

// Generated at build time from main/esp_efuse_custom_table.csv
#include "esp_efuse_custom_table.h"

static const char *TAG = "custom_efuse_demo";

/**
 * @brief Compute CRC-16/CCITT-FALSE over a byte buffer.
 *
 * Polynomial: 0x1021
 * Init: 0xFFFF
 * RefIn/RefOut: false
 * XorOut: 0x0000
 *
 * @param data Input data pointer.
 * @param len  Number of bytes.
 * @return uint16_t CRC value.
 */
static uint16_t crc16_ccitt_false(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

/**
 * @brief Read and print the custom fields.
 *
 * This function reads the custom fields defined in the custom CSV table:
 *   - USER_DATA.SERIAL_NUMBER (128 bits)
 *   - USER_DATA.HW_REV (16 bits)
 *   - USER_DATA.FEATURE_FLAGS (32 bits)
 *   - USER_DATA.PROVISIONING_CRC16 (16 bits)
 *
 * @return esp_err_t ESP_OK on success.
 */
static esp_err_t efuse_read_custom_fields(void)
{
    // SERIAL_NUMBER is 16 bytes ASCII, padded with 0x00.
    uint8_t serial_raw[16] = {0};
    uint16_t hw_rev = 0;
    uint32_t flags = 0;
    uint16_t crc16_stored = 0;

    esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, serial_raw, sizeof(serial_raw) * 8);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HW_REV, &hw_rev, sizeof(hw_rev) * 8);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_FEATURE_FLAGS, &flags, sizeof(flags) * 8);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_PROVISIONING_CRC16, &crc16_stored, sizeof(crc16_stored) * 8);
    if (err != ESP_OK) {
        return err;
    }

    // Convert serial to a printable C string.
    char serial_str[17] = {0};
    memcpy(serial_str, serial_raw, 16);

    ESP_LOGI(TAG, "SERIAL_NUMBER: '%s'", serial_str);
    ESP_LOGI(TAG, "HW_REV: 0x%04X (%u)", hw_rev, (unsigned)hw_rev);
    ESP_LOGI(TAG, "FEATURE_FLAGS: 0x%08" PRIX32, flags);
    ESP_LOGI(TAG, "PROVISIONING_CRC16: 0x%04X", crc16_stored);

    // Build the same 22-byte payload used for CRC computation.
    uint8_t payload[16 + 2 + 4] = {0};
    memcpy(&payload[0], serial_raw, 16);
    payload[16] = (uint8_t)(hw_rev & 0xFF);
    payload[17] = (uint8_t)((hw_rev >> 8) & 0xFF);
    payload[18] = (uint8_t)(flags & 0xFF);
    payload[19] = (uint8_t)((flags >> 8) & 0xFF);
    payload[20] = (uint8_t)((flags >> 16) & 0xFF);
    payload[21] = (uint8_t)((flags >> 24) & 0xFF);

    uint16_t crc16_calc = crc16_ccitt_false(payload, sizeof(payload));
    ESP_LOGI(TAG, "CRC16 recalculated: 0x%04X", crc16_calc);

    if (crc16_stored == 0) {
        ESP_LOGW(TAG, "CRC16 stored is 0x0000 (likely not provisioned yet)");
    } else if (crc16_stored != crc16_calc) {
        ESP_LOGW(TAG, "CRC16 mismatch (stored != calculated)");
    } else {
        ESP_LOGI(TAG, "CRC16 check: OK");
    }

    return ESP_OK;
}

/**
 * @brief Check whether custom eFuse fields look provisioned (CRC present and self-consistent).
 *
 * This helper reads the current custom fields, recomputes CRC16 over the stored payload,
 * and returns true if the stored CRC is non-zero and matches the computed CRC.
 *
 * Note:
 * - This does NOT guarantee the content matches any particular "desired" values.
 * - It is intended as a safety gate to prevent repeated programming attempts.
 *
 * @param out_is_ok_crc Optional output flag indicating CRC match status.
 * @return true if the device appears provisioned; false otherwise.
 */
static bool efuse_is_provisioned(bool *out_is_ok_crc)
{
    uint8_t serial_raw[16] = {0};
    uint16_t hw_rev = 0;
    uint32_t flags = 0;
    uint16_t crc16_stored = 0;

    if (out_is_ok_crc) {
        *out_is_ok_crc = false;
    }

    esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, serial_raw, sizeof(serial_raw) * 8);
    if (err != ESP_OK) {
        return false;
    }
    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HW_REV, &hw_rev, sizeof(hw_rev) * 8);
    if (err != ESP_OK) {
        return false;
    }
    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_FEATURE_FLAGS, &flags, sizeof(flags) * 8);
    if (err != ESP_OK) {
        return false;
    }
    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_PROVISIONING_CRC16, &crc16_stored, sizeof(crc16_stored) * 8);
    if (err != ESP_OK) {
        return false;
    }

    uint8_t payload[16 + 2 + 4] = {0};
    memcpy(&payload[0], serial_raw, 16);
    payload[16] = (uint8_t)(hw_rev & 0xFF);
    payload[17] = (uint8_t)((hw_rev >> 8) & 0xFF);
    payload[18] = (uint8_t)(flags & 0xFF);
    payload[19] = (uint8_t)((flags >> 8) & 0xFF);
    payload[20] = (uint8_t)((flags >> 16) & 0xFF);
    payload[21] = (uint8_t)((flags >> 24) & 0xFF);

    uint16_t crc16_calc = crc16_ccitt_false(payload, sizeof(payload));
    bool ok = (crc16_stored != 0) && (crc16_stored == crc16_calc);

    if (out_is_ok_crc) {
        *out_is_ok_crc = ok;
    }

    return ok;
}

/**
 * @brief Program the custom fields and store a CRC16.
 *
 * This function burns:
 *   - SERIAL_NUMBER (16 bytes, padded)
 *   - HW_REV (uint16)
 *   - FEATURE_FLAGS (uint32)
 *   - PROVISIONING_CRC16 (CRC-16 over the fixed payload)
 *
 * IMPORTANT:
 * - In virtual eFuse mode, this modifies the virtual store only.
 * - When CONFIG_EFUSE_VIRTUAL is disabled, this will burn real eFuses.
 * - Batch mode is used because user blocks on ESP32-S3 use Reed-Solomon encoding.
 *
 * @param serial_ascii Null-terminated serial string (up to 16 chars are stored).
 * @param hw_rev       Hardware revision.
 * @param flags        Feature flags.
 * @return esp_err_t ESP_OK on success.
 */
/**
 * @brief Program the custom fields and store a CRC16.
 *
 * This function burns:
 *   - SERIAL_NUMBER (16 bytes, padded)
 *   - HW_REV (uint16)
 *   - FEATURE_FLAGS (uint32)
 *   - PROVISIONING_CRC16 (CRC-16 over the fixed payload)
 *
 * IMPORTANT:
 * - In virtual eFuse mode, this modifies the virtual store only.
 * - When CONFIG_EFUSE_VIRTUAL is disabled, this will burn real eFuses.
 * - Batch mode is used because user blocks on ESP32-S3 use Reed-Solomon encoding.
 *
 * Idempotency and safety:
 * - ESP-IDF forbids "re-programming" bits that are already set to 1.
 * - To keep this demo safe across reboots (especially with virtual eFuses persisted in flash),
 *   this function first checks whether the device appears provisioned (CRC is present and matches).
 * - If provisioned, the function returns ESP_OK without attempting any write.
 * - If not provisioned, the function will only stage NEW bits that transition 0 -> 1.
 * - If existing bits would require clearing (1 -> 0), the function fails with ESP_ERR_INVALID_STATE.
 *
 * @param serial_ascii Null-terminated serial string (up to 16 chars are stored).
 * @param hw_rev       Hardware revision.
 * @param flags        Feature flags.
 * @return esp_err_t ESP_OK on success.
 */
static esp_err_t efuse_program_custom_fields(const char *serial_ascii, uint16_t hw_rev, uint32_t flags)
{
    if (serial_ascii == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // If the device already looks provisioned, do not attempt to re-program.
    // This avoids ESP_ERR_EFUSE_REPEATED_PROG on subsequent boots/runs.
    bool crc_ok = false;
    if (efuse_is_provisioned(&crc_ok) && crc_ok) {
        ESP_LOGI(TAG, "Device already provisioned (CRC OK). Skipping eFuse programming.");
        return ESP_OK;
    }

    // Desired payload (SERIAL_NUMBER[16] + HW_REV[2] + FEATURE_FLAGS[4])
    // Zero init ensures short serial strings are padded with 0x00.
    uint8_t desired_payload[16 + 2 + 4] = {0};

    // Copy up to 16 bytes from the serial string (no overread).
    size_t n = strlen(serial_ascii);
    if (n > 16) {
        n = 16;
    }
    memcpy(&desired_payload[0], serial_ascii, n);

    // HW_REV (little-endian)
    desired_payload[16] = (uint8_t)(hw_rev & 0xFF);
    desired_payload[17] = (uint8_t)((hw_rev >> 8) & 0xFF);

    // FEATURE_FLAGS (little-endian)
    desired_payload[18] = (uint8_t)(flags & 0xFF);
    desired_payload[19] = (uint8_t)((flags >> 8) & 0xFF);
    desired_payload[20] = (uint8_t)((flags >> 16) & 0xFF);
    desired_payload[21] = (uint8_t)((flags >> 24) & 0xFF);

    // Compute CRC16 over the fixed payload.
    const uint16_t desired_crc16 = crc16_ccitt_false(desired_payload, sizeof(desired_payload));
    const uint8_t desired_crc_le[2] = {
        (uint8_t)(desired_crc16 & 0xFF),
        (uint8_t)((desired_crc16 >> 8) & 0xFF),
    };

    // Read current values so we can:
    //  1) refuse programming if it would require clearing bits (1 -> 0)
    //  2) stage only NEW bits (avoid repeated programming)
    uint8_t cur_serial[16] = {0};
    uint16_t cur_hw_rev = 0;
    uint32_t cur_flags = 0;
    uint16_t cur_crc16 = 0;

    esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, cur_serial, sizeof(cur_serial) * 8);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HW_REV, &cur_hw_rev, sizeof(cur_hw_rev) * 8);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_FEATURE_FLAGS, &cur_flags, sizeof(cur_flags) * 8);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_PROVISIONING_CRC16, &cur_crc16, sizeof(cur_crc16) * 8);
    if (err != ESP_OK) {
        return err;
    }

    // Convert current HW/flags/CRC to little-endian byte arrays for bitwise checks.
    const uint8_t cur_hw_le[2] = {
        (uint8_t)(cur_hw_rev & 0xFF),
        (uint8_t)((cur_hw_rev >> 8) & 0xFF),
    };
    const uint8_t cur_flags_le[4] = {
        (uint8_t)(cur_flags & 0xFF),
        (uint8_t)((cur_flags >> 8) & 0xFF),
        (uint8_t)((cur_flags >> 16) & 0xFF),
        (uint8_t)((cur_flags >> 24) & 0xFF),
    };
    const uint8_t cur_crc_le[2] = {
        (uint8_t)(cur_crc16 & 0xFF),
        (uint8_t)((cur_crc16 >> 8) & 0xFF),
    };

    // Helper lambda-like macro: detect if we'd need to clear any bits (illegal for eFuses).
#define EFUSE_CONFLICT_EXISTS(cur_b, desired_b) (((cur_b) & (uint8_t)(~(desired_b))) != 0)

    for (size_t i = 0; i < 16; i++) {
        if (EFUSE_CONFLICT_EXISTS(cur_serial[i], desired_payload[i])) {
            ESP_LOGE(TAG, "SERIAL_NUMBER conflict: would require clearing bits at byte %u", (unsigned)i);
            return ESP_ERR_INVALID_STATE;
        }
    }
    for (size_t i = 0; i < 2; i++) {
        if (EFUSE_CONFLICT_EXISTS(cur_hw_le[i], desired_payload[16 + i])) {
            ESP_LOGE(TAG, "HW_REV conflict: would require clearing bits at byte %u", (unsigned)i);
            return ESP_ERR_INVALID_STATE;
        }
    }
    for (size_t i = 0; i < 4; i++) {
        if (EFUSE_CONFLICT_EXISTS(cur_flags_le[i], desired_payload[18 + i])) {
            ESP_LOGE(TAG, "FEATURE_FLAGS conflict: would require clearing bits at byte %u", (unsigned)i);
            return ESP_ERR_INVALID_STATE;
        }
    }
    for (size_t i = 0; i < 2; i++) {
        if (EFUSE_CONFLICT_EXISTS(cur_crc_le[i], desired_crc_le[i])) {
            ESP_LOGE(TAG, "CRC16 conflict: would require clearing bits at byte %u", (unsigned)i);
            return ESP_ERR_INVALID_STATE;
        }
    }

    // Build delta buffers: stage only bits that transition 0 -> 1.
    uint8_t serial_delta[16] = {0};
    uint8_t hw_delta[2] = {0};
    uint8_t flags_delta[4] = {0};
    uint8_t crc_delta[2] = {0};

    bool need_serial = false;
    bool need_hw = false;
    bool need_flags = false;
    bool need_crc = false;

    for (size_t i = 0; i < 16; i++) {
        serial_delta[i] = (uint8_t)(desired_payload[i] & (uint8_t)(~cur_serial[i]));
        need_serial |= (serial_delta[i] != 0);
    }
    for (size_t i = 0; i < 2; i++) {
        hw_delta[i] = (uint8_t)(desired_payload[16 + i] & (uint8_t)(~cur_hw_le[i]));
        need_hw |= (hw_delta[i] != 0);
    }
    for (size_t i = 0; i < 4; i++) {
        flags_delta[i] = (uint8_t)(desired_payload[18 + i] & (uint8_t)(~cur_flags_le[i]));
        need_flags |= (flags_delta[i] != 0);
    }
    for (size_t i = 0; i < 2; i++) {
        crc_delta[i] = (uint8_t)(desired_crc_le[i] & (uint8_t)(~cur_crc_le[i]));
        need_crc |= (crc_delta[i] != 0);
    }

    if (!need_serial && !need_hw && !need_flags && !need_crc) {
        ESP_LOGI(TAG, "No new bits to program. Skipping commit.");
        return ESP_OK;
    }

    // Begin batch write mode (required for RS-coded user blocks on ESP32-S3).
    err = esp_efuse_batch_write_begin();
    if (err != ESP_OK) {
        return err;
    }

    // Stage only the deltas. This avoids repeated-programming errors.
    if (need_serial) {
        err = esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_SERIAL_NUMBER, serial_delta, 16 * 8);
        if (err != ESP_OK) {
            esp_efuse_batch_write_cancel();
            return err;
        }
    }

    if (need_hw) {
        err = esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_HW_REV, hw_delta, 2 * 8);
        if (err != ESP_OK) {
            esp_efuse_batch_write_cancel();
            return err;
        }
    }

    if (need_flags) {
        err = esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_FEATURE_FLAGS, flags_delta, 4 * 8);
        if (err != ESP_OK) {
            esp_efuse_batch_write_cancel();
            return err;
        }
    }

    if (need_crc) {
        err = esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_PROVISIONING_CRC16, crc_delta, 2 * 8);
        if (err != ESP_OK) {
            esp_efuse_batch_write_cancel();
            return err;
        }
    }

    // Burn staged bits.
    err = esp_efuse_batch_write_commit();
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Provisioning committed (CRC16=0x%04X)", desired_crc16);
    return ESP_OK;
}

/**
 * @brief ESP-IDF application entry point.
 *
 * Behavior:
 * - Always reads and prints the custom fields.
 * - If CONFIG_DEMO_PROGRAM_EFUSE is enabled, it attempts to provision example values.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Custom eFuse demo starting (target=%s)", CONFIG_IDF_TARGET);

    // Optional programming step.
#if CONFIG_DEMO_PROGRAM_EFUSE
    ESP_LOGW(TAG, "CONFIG_DEMO_PROGRAM_EFUSE is enabled. Provisioning will be attempted.");

    // Example values.
    // NOTE: Keep these constant for a demo. In production, these come from a provisioning system.
    const char *serial = "SN-ESP32S3-0001";
    uint16_t hw_rev = 0x0001;
    uint32_t flags = 0x0000000F;

    esp_err_t err = efuse_program_custom_fields(serial, hw_rev, flags);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Provisioning failed: %s", esp_err_to_name(err));
    }
#endif

    // Read and display fields (and validate CRC).
    err = efuse_read_custom_fields();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(err));
    }

    // Idle loop.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}