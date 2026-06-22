#include "eeprom_24lc256.h"

#include <string.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#define EEPROM_TRANSACTION_TIMEOUT_MS 200
#define EEPROM_WRITE_CYCLE_MS         6

static const char *TAG = "24lc256";

/**
 * @brief Validates that a transfer remains inside EEPROM capacity.
 *
 * @param memory_address First EEPROM byte address.
 * @param data_length Number of bytes in the transfer.
 *
 * @return True when the complete transfer is within the device capacity.
 */
static bool transfer_is_in_range(uint16_t memory_address, size_t data_length)
{
    return ((size_t)memory_address + data_length) <=
           EEPROM_24LC256_CAPACITY_BYTES;
}

/**
 * @brief Writes one chunk that does not cross an EEPROM page boundary.
 *
 * @param eeprom Initialized EEPROM context.
 * @param memory_address First EEPROM byte address.
 * @param data Source buffer.
 * @param data_length Number of bytes to write in the current page.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
static esp_err_t write_page_chunk(const eeprom_24lc256_t *eeprom,
                                  uint16_t memory_address,
                                  const uint8_t *data,
                                  size_t data_length)
{
    uint8_t transmit_buffer[EEPROM_24LC256_PAGE_SIZE + 2];

    // 24LC256 devices use a two-byte big-endian memory address.
    transmit_buffer[0] = (uint8_t)(memory_address >> 8);
    transmit_buffer[1] = (uint8_t)(memory_address & 0xFFU);
    memcpy(&transmit_buffer[2], data, data_length);

    ESP_RETURN_ON_ERROR(i2c_master_transmit(eeprom->device_handle,
                                             transmit_buffer,
                                             data_length + 2,
                                             EEPROM_TRANSACTION_TIMEOUT_MS),
                        TAG,
                        "EEPROM page write failed");

    // The EEPROM needs time to program the internal nonvolatile array.
    vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_CYCLE_MS));
    return ESP_OK;
}

/**
 * @brief Initializes the EEPROM.
 *
 * @param eeprom Initialized EEPROM context.
 * @param address The 7-bit I2C address of the EEPROM.
 * @param scl_speed_hz The SCL speed in Hertz.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_init(eeprom_24lc256_t *eeprom,
                              uint8_t address,
                              uint32_t scl_speed_hz)
{
    ESP_RETURN_ON_FALSE(eeprom != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "EEPROM context is NULL");

    memset(eeprom, 0, sizeof(*eeprom));
    ESP_RETURN_ON_ERROR(app_i2c_bus_add_device(address,
                                                scl_speed_hz,
                                                &eeprom->device_handle),
                        TAG,
                        "Failed to add EEPROM device");

    eeprom->address = address;
    eeprom->initialized = true;
    return ESP_OK;
}

/**
 * @brief Reads data from the EEPROM.
 *
 * @param eeprom Initialized EEPROM context.
 * @param memory_address First EEPROM byte address.
 * @param data Destination buffer.
 * @param data_length Number of bytes to read.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_read(const eeprom_24lc256_t *eeprom,
                              uint16_t memory_address,
                              uint8_t *data,
                              size_t data_length)
{
    ESP_RETURN_ON_FALSE(eeprom != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "EEPROM context is NULL");
    ESP_RETURN_ON_FALSE(eeprom->initialized,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "EEPROM is not initialized");
    ESP_RETURN_ON_FALSE(data != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Read destination is NULL");
    ESP_RETURN_ON_FALSE(data_length > 0,
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Read length is zero");
    ESP_RETURN_ON_FALSE(transfer_is_in_range(memory_address, data_length),
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Read exceeds EEPROM capacity");

    const uint8_t address_bytes[2] = {
        (uint8_t)(memory_address >> 8),
        (uint8_t)(memory_address & 0xFFU),
    };

    return i2c_master_transmit_receive(eeprom->device_handle,
                                       address_bytes,
                                       sizeof(address_bytes),
                                       data,
                                       data_length,
                                       EEPROM_TRANSACTION_TIMEOUT_MS);
}

/**
 * @brief Writes data to the EEPROM.
 *
 * @param eeprom Initialized EEPROM context.
 * @param memory_address First EEPROM byte address.
 * @param data Source buffer.
 * @param data_length Number of bytes to write.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_write(const eeprom_24lc256_t *eeprom,
                               uint16_t memory_address,
                               const uint8_t *data,
                               size_t data_length)
{
    ESP_RETURN_ON_FALSE(eeprom != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "EEPROM context is NULL");
    ESP_RETURN_ON_FALSE(eeprom->initialized,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "EEPROM is not initialized");
    ESP_RETURN_ON_FALSE(data != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Write source is NULL");
    ESP_RETURN_ON_FALSE(data_length > 0,
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Write length is zero");
    ESP_RETURN_ON_FALSE(transfer_is_in_range(memory_address, data_length),
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Write exceeds EEPROM capacity");

    size_t bytes_remaining = data_length;
    size_t source_offset = 0;
    uint16_t current_address = memory_address;

    while (bytes_remaining > 0) {
        const size_t page_offset = current_address % EEPROM_24LC256_PAGE_SIZE;
        const size_t page_space = EEPROM_24LC256_PAGE_SIZE - page_offset;
        const size_t chunk_length = (bytes_remaining < page_space)
                                        ? bytes_remaining
                                        : page_space;

        ESP_RETURN_ON_ERROR(write_page_chunk(eeprom,
                                              current_address,
                                              &data[source_offset],
                                              chunk_length),
                            TAG,
                            "EEPROM write failed at address 0x%04X",
                            current_address);

        current_address = (uint16_t)(current_address + chunk_length);
        source_offset += chunk_length;
        bytes_remaining -= chunk_length;
    }

    return ESP_OK;
}

/**
 * @brief Deinitializes the EEPROM.
 *
 * @param eeprom Initialized EEPROM context.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_deinit(eeprom_24lc256_t *eeprom)
{
    ESP_RETURN_ON_FALSE(eeprom != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "EEPROM context is NULL");

    if (!eeprom->initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(app_i2c_bus_remove_device(eeprom->device_handle),
                        TAG,
                        "Failed to remove EEPROM device");

    memset(eeprom, 0, sizeof(*eeprom));
    return ESP_OK;
}
