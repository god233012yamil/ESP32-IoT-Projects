#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESH_PROTOCOL_VERSION 1U
#define MESH_BROADCAST_NODE_ID 0xFFFFU
#define MESH_MAX_PACKET_SIZE 250U

typedef enum {
    MESH_PACKET_BEACON = 1,
    MESH_PACKET_SENSOR = 2,
    MESH_PACKET_ACK = 3,
} mesh_packet_type_t;

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t type;
    uint16_t source_id;
    uint16_t destination_id;
    uint16_t sequence;
    uint8_t ttl;
    uint8_t flags;
    uint32_t uptime_ms;
    int16_t temperature_centi_c;
    uint16_t humidity_centi_pct;
    uint16_t battery_mv;
    uint16_t payload_crc;
} mesh_packet_t;

/**
 * @brief Calculates a CRC-16/CCITT-FALSE checksum.
 *
 * Args:
 *     data: Pointer to the data buffer.
 *     length: Number of bytes to process.
 *
 * Returns:
 *     The calculated 16-bit CRC value.
 */
uint16_t mesh_crc16_ccitt(const uint8_t *data, size_t length);

/**
 * @brief Finalizes a packet by assigning its CRC field.
 *
 * Args:
 *     packet: Packet to finalize.
 */
void mesh_packet_finalize(mesh_packet_t *packet);

/**
 * @brief Validates packet size, version, type, and CRC.
 *
 * Args:
 *     data: Received byte buffer.
 *     length: Number of bytes in the buffer.
 *
 * Returns:
 *     ESP_OK when the packet is valid; otherwise an error code.
 */
esp_err_t mesh_packet_validate(const uint8_t *data, size_t length);

/**
 * @brief Converts a MAC address string into a six-byte array.
 *
 * Args:
 *     text: MAC address string in AA:BB:CC:DD:EE:FF format.
 *     mac: Destination six-byte array.
 *
 * Returns:
 *     ESP_OK on success; ESP_ERR_INVALID_ARG when parsing fails.
 */
esp_err_t mesh_parse_mac(const char *text, uint8_t mac[6]);

/**
 * @brief Checks whether a MAC address is the broadcast address.
 *
 * Args:
 *     mac: MAC address to inspect.
 *
 * Returns:
 *     True when all bytes are 0xFF; otherwise false.
 */
bool mesh_is_broadcast_mac(const uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif
