#include "mesh_protocol.h"

#include <stdio.h>
#include <string.h>

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
uint16_t mesh_crc16_ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;

    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8U;
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U)
                                  : (uint16_t)(crc << 1U);
        }
    }

    return crc;
}

/**
 * @brief Finalizes a packet by assigning its CRC field.
 *
 * Args:
 *     packet: Packet to finalize.
 */
void mesh_packet_finalize(mesh_packet_t *packet)
{
    if (packet == NULL) {
        return;
    }

    packet->payload_crc = 0U;
    packet->payload_crc = mesh_crc16_ccitt((const uint8_t *)packet,
                                          sizeof(*packet));
}

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
esp_err_t mesh_packet_validate(const uint8_t *data, size_t length)
{
    if ((data == NULL) || (length != sizeof(mesh_packet_t))) {
        return ESP_ERR_INVALID_SIZE;
    }

    mesh_packet_t packet;
    memcpy(&packet, data, sizeof(packet));

    if (packet.version != MESH_PROTOCOL_VERSION) {
        return ESP_ERR_INVALID_VERSION;
    }

    if ((packet.type < MESH_PACKET_BEACON) ||
        (packet.type > MESH_PACKET_ACK)) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint16_t received_crc = packet.payload_crc;
    packet.payload_crc = 0U;
    const uint16_t calculated_crc = mesh_crc16_ccitt(
        (const uint8_t *)&packet, sizeof(packet));

    return (received_crc == calculated_crc) ? ESP_OK : ESP_ERR_INVALID_CRC;
}

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
esp_err_t mesh_parse_mac(const char *text, uint8_t mac[6])
{
    if ((text == NULL) || (mac == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    unsigned int values[6];
    const int parsed = sscanf(text,
                              "%2x:%2x:%2x:%2x:%2x:%2x",
                              &values[0], &values[1], &values[2],
                              &values[3], &values[4], &values[5]);
    if (parsed != 6) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < 6U; ++i) {
        if (values[i] > 0xFFU) {
            return ESP_ERR_INVALID_ARG;
        }
        mac[i] = (uint8_t)values[i];
    }

    return ESP_OK;
}

/**
 * @brief Checks whether a MAC address is the broadcast address.
 *
 * Args:
 *     mac: MAC address to inspect.
 *
 * Returns:
 *     True when all bytes are 0xFF; otherwise false.
 */
bool mesh_is_broadcast_mac(const uint8_t mac[6])
{
    static const uint8_t broadcast[6] = {
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
    };

    return (mac != NULL) && (memcmp(mac, broadcast, sizeof(broadcast)) == 0);
}
