#ifndef EEPROM_24LC256_H
#define EEPROM_24LC256_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define EEPROM_24LC256_CAPACITY_BYTES 32768U
#define EEPROM_24LC256_PAGE_SIZE      64U

/**
 * @brief 24LC256 EEPROM driver context.
 */
typedef struct {
    i2c_master_dev_handle_t device_handle;
    uint8_t address;
    bool initialized;
} eeprom_24lc256_t;

/**
 * @brief Initializes a 24LC256-compatible EEPROM driver instance.
 *
 * @param eeprom Driver context to initialize.
 * @param address Seven-bit EEPROM address, normally 0x50 through 0x57.
 * @param scl_speed_hz I2C clock speed in hertz.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_init(eeprom_24lc256_t *eeprom,
                              uint8_t address,
                              uint32_t scl_speed_hz);

/**
 * @brief Reads bytes from EEPROM memory.
 *
 * @param eeprom Initialized EEPROM context.
 * @param memory_address First 16-bit memory address to read.
 * @param data Destination buffer.
 * @param data_length Number of bytes to read.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_read(const eeprom_24lc256_t *eeprom,
                              uint16_t memory_address,
                              uint8_t *data,
                              size_t data_length);

/**
 * @brief Writes bytes while respecting 64-byte EEPROM page boundaries.
 *
 * @param eeprom Initialized EEPROM context.
 * @param memory_address First 16-bit memory address to write.
 * @param data Source buffer.
 * @param data_length Number of bytes to write.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_write(const eeprom_24lc256_t *eeprom,
                               uint16_t memory_address,
                               const uint8_t *data,
                               size_t data_length);

/**
 * @brief Releases the EEPROM device handle.
 *
 * @param eeprom Driver context to deinitialize.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t eeprom_24lc256_deinit(eeprom_24lc256_t *eeprom);

#endif
