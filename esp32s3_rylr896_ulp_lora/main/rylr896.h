/**
 * @file rylr896.h
 * @brief REYAX RYLR896 LoRa module UART driver interface.
 *
 * Provides initialisation, AT command helpers, and a single high-level
 * rylr896_send_data() function for sending arbitrary payloads over LoRa.
 *
 * Hardware connection (default, configurable via menuconfig or constants below):
 *   ESP32-S3 GPIO17  -> RYLR896 RX
 *   ESP32-S3 GPIO18  -> RYLR896 TX
 *   ESP32-S3 3V3     -> RYLR896 VCC
 *   ESP32-S3 GND     -> RYLR896 GND
 *
 * The RYLR896 operates at 115200 baud by default.
 */

#ifndef RYLR896_H
#define RYLR896_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Hardware configuration ------------------------------------------- */

/** UART port used to communicate with the RYLR896 module. */
#define RYLR896_UART_PORT    UART_NUM_1

/** GPIO number for ESP32-S3 TX -> RYLR896 RX. */
#define RYLR896_TX_PIN       17

/** GPIO number for ESP32-S3 RX -> RYLR896 TX. */
#define RYLR896_RX_PIN       18

/** UART baud rate (RYLR896 factory default). */
#define RYLR896_BAUD_RATE    115200

/** UART RX ring-buffer size in bytes. */
#define RYLR896_RX_BUF_SIZE  256

/** UART TX ring-buffer size in bytes (0 = no TX buffer, blocking writes). */
#define RYLR896_TX_BUF_SIZE  0

/* ---- LoRa module configuration ---------------------------------------- */

/** Local device address (0–65535). Change to match your network plan. */
#define RYLR896_ADDRESS      1

/** Network ID (shared by all nodes that should communicate). */
#define RYLR896_NETWORK_ID   18

/** RF frequency in Hz (915 MHz for US; use 868000000 for EU). */
#define RYLR896_BAND_HZ      915000000

/** Destination address for AT+SEND (0 = broadcast). */
#define RYLR896_DEST_ADDR    0

/* ---- Timeout configuration -------------------------------------------- */

/** Maximum time (ms) to wait for a single AT command response. */
#define RYLR896_CMD_TIMEOUT_MS   3000

/* ---- Public API -------------------------------------------------------- */

/**
 * @brief Initialise the UART peripheral and configure the RYLR896 module.
 *
 * Installs the UART driver, configures baud rate / framing, and sends the
 * required AT setup commands (ADDRESS, NETWORKID, BAND).
 *
 * Must be called once before any rylr896_send_data() call.  After the
 * transmission session is complete, call rylr896_deinit() to release
 * hardware resources before entering deep sleep.
 *
 * @return true  if UART init and all setup AT commands succeeded.
 * @return false if any step failed (logs an error message).
 */
bool rylr896_init(void);

/**
 * @brief Release UART driver resources.
 *
 * Flushes any pending data, then uninstalls the UART driver so the
 * peripheral is powered down before deep sleep entry.
 *
 * Safe to call even if rylr896_init() was not called (no-op in that case).
 */
void rylr896_deinit(void);

/**
 * @brief Send a null-terminated string payload via the RYLR896 LoRa module.
 *
 * Formats and transmits an AT+SEND command, then blocks until the module
 * responds with "+OK" or the command timeout elapses.
 *
 * @param data  Null-terminated ASCII string to transmit (max 240 bytes per
 *              RYLR896 datasheet limit).  Must not be NULL.
 *
 * @return true  if the module acknowledged the send with "+OK".
 * @return false if data is NULL, the string is empty, the UART write failed,
 *               or a "+OK" response was not received within the timeout.
 */
bool rylr896_send_data(const char *data);

#ifdef __cplusplus
}
#endif

#endif /* RYLR896_H */