/**
 * @file rylr896.c
 * @brief REYAX RYLR896 LoRa module UART driver implementation.
 *
 * Implements UART-based AT command communication with the RYLR896.
 * All public functions validate their inputs and check ESP-IDF return codes;
 * failures are logged and propagated to the caller — no silent failures.
 */

#include "rylr896.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ---- Module-private constants ----------------------------------------- */

static const char *TAG = "RYLR896";

/** Carriage-return + line-feed terminator used by all AT commands. */
#define AT_CRLF          "\r\n"

/** Expected positive acknowledgement substring from the module. */
#define AT_OK_RESPONSE   "+OK"

/** Expected error substring returned by the module on bad commands. */
#define AT_ERR_RESPONSE  "+ERR"

/** Scratch buffer size for building AT command strings. */
#define CMD_BUF_SIZE     320

/** Receive buffer size for reading responses. */
#define RESP_BUF_SIZE    128

/* ---- Module-private state --------------------------------------------- */

/** True after rylr896_init() succeeds; guards against double-init. */
static bool s_is_initialised = false;

/* ---- Private helpers -------------------------------------------------- */

/**
 * @brief Flush the UART RX buffer to discard stale data.
 */
static void flush_rx(void)
{
    uart_flush_input(RYLR896_UART_PORT);
}

/**
 * @brief Send a raw AT command string (without CRLF) and wait for "+OK".
 *
 * Appends CRLF, writes to UART, then reads back the response line.
 *
 * @param cmd  Null-terminated AT command string (without trailing CRLF).
 *
 * @return true if "+OK" was found in the response within the timeout.
 * @return false on write error or timeout / unexpected response.
 */
static bool send_at_command(const char *cmd)
{
    if (cmd == NULL) {
        ESP_LOGE(TAG, "send_at_command: NULL command");
        return false;
    }

    /* Build the full command with CRLF terminator. */
    char full_cmd[CMD_BUF_SIZE];
    int len = snprintf(full_cmd, sizeof(full_cmd), "%s%s", cmd, AT_CRLF);
    if (len < 0 || len >= (int)sizeof(full_cmd)) {
        ESP_LOGE(TAG, "send_at_command: command too long (%d chars)", len);
        return false;
    }

    /* Discard any previously buffered RX data. */
    flush_rx();

    /* Transmit the command. */
    int written = uart_write_bytes(RYLR896_UART_PORT, full_cmd, (size_t)len);
    if (written != len) {
        ESP_LOGE(TAG, "UART write failed (wrote %d / %d bytes)", written, len);
        return false;
    }

    /* Wait for the TX FIFO to drain so we read a clean response. */
    esp_err_t drain_err = uart_wait_tx_done(RYLR896_UART_PORT,
                              pdMS_TO_TICKS(RYLR896_CMD_TIMEOUT_MS));
    if (drain_err != ESP_OK) {
        ESP_LOGE(TAG, "uart_wait_tx_done: %s", esp_err_to_name(drain_err));
        return false;
    }

    /* Read the response. */
    uint8_t resp[RESP_BUF_SIZE];
    memset(resp, 0, sizeof(resp));
    int rxlen = uart_read_bytes(RYLR896_UART_PORT,
                                resp,
                                sizeof(resp) - 1U,
                                pdMS_TO_TICKS(RYLR896_CMD_TIMEOUT_MS));
    if (rxlen <= 0) {
        ESP_LOGE(TAG, "No response to command: %s (timeout %d ms)",
                 cmd, RYLR896_CMD_TIMEOUT_MS);
        return false;
    }

    resp[rxlen] = '\0';  /* Null-terminate for string search. */
    ESP_LOGD(TAG, "Response (%d bytes): %s", rxlen, (char *)resp);

    if (strstr((char *)resp, AT_OK_RESPONSE) != NULL) {
        return true;
    }

    ESP_LOGE(TAG, "Unexpected response to '%s': %s", cmd, (char *)resp);
    return false;
}

/* ---- Public API implementation ---------------------------------------- */

/**
 * @brief Initialise the UART peripheral and configure the RYLR896 module.
 */
bool rylr896_init(void)
{
    if (s_is_initialised) {
        ESP_LOGW(TAG, "rylr896_init called while already initialised");
        return true;
    }

    /* Configure UART parameters. */
    const uart_config_t uart_cfg = {
        .baud_rate  = RYLR896_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(RYLR896_UART_PORT, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin(RYLR896_UART_PORT,
                       RYLR896_TX_PIN,
                       RYLR896_RX_PIN,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_driver_install(RYLR896_UART_PORT,
                              RYLR896_RX_BUF_SIZE,
                              RYLR896_TX_BUF_SIZE,
                              0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install: %s", esp_err_to_name(err));
        return false;
    }

    /* Allow module to settle after power-on / deep-sleep wakeup. */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Send AT (ping) — module should respond "+OK". */
    if (!send_at_command("AT")) {
        ESP_LOGE(TAG, "Module did not respond to AT ping");
        uart_driver_delete(RYLR896_UART_PORT);
        return false;
    }

    /* Set device address. */
    char cmd[CMD_BUF_SIZE];
    snprintf(cmd, sizeof(cmd), "AT+ADDRESS=%d", RYLR896_ADDRESS);
    if (!send_at_command(cmd)) {
        ESP_LOGE(TAG, "Failed to set ADDRESS");
        uart_driver_delete(RYLR896_UART_PORT);
        return false;
    }

    /* Set network ID. */
    snprintf(cmd, sizeof(cmd), "AT+NETWORKID=%d", RYLR896_NETWORK_ID);
    if (!send_at_command(cmd)) {
        ESP_LOGE(TAG, "Failed to set NETWORKID");
        uart_driver_delete(RYLR896_UART_PORT);
        return false;
    }

    /* Set RF band frequency. */
    snprintf(cmd, sizeof(cmd), "AT+BAND=%u", (unsigned int)RYLR896_BAND_HZ);
    if (!send_at_command(cmd)) {
        ESP_LOGE(TAG, "Failed to set BAND");
        uart_driver_delete(RYLR896_UART_PORT);
        return false;
    }

    s_is_initialised = true;
    ESP_LOGI(TAG, "Initialised (addr=%d, netid=%d, band=%u Hz)",
             RYLR896_ADDRESS, RYLR896_NETWORK_ID,
             (unsigned int)RYLR896_BAND_HZ);
    return true;
}

/**
 * @brief Release UART driver resources before deep sleep entry.
 */
void rylr896_deinit(void)
{
    if (!s_is_initialised) {
        return;
    }

    /* Ensure all TX bytes have been clocked out. */
    uart_wait_tx_done(RYLR896_UART_PORT, pdMS_TO_TICKS(200));
    uart_driver_delete(RYLR896_UART_PORT);
    s_is_initialised = false;
    ESP_LOGD(TAG, "UART driver released");
}

/**
 * @brief Send a null-terminated string payload via the RYLR896 LoRa module.
 */
bool rylr896_send_data(const char *data)
{
    if (data == NULL) {
        ESP_LOGE(TAG, "rylr896_send_data: NULL data pointer");
        return false;
    }

    size_t data_len = strlen(data);
    if (data_len == 0U) {
        ESP_LOGE(TAG, "rylr896_send_data: empty string");
        return false;
    }

    if (data_len > 240U) {
        ESP_LOGE(TAG, "rylr896_send_data: payload too long (%u bytes, max 240)",
                 (unsigned int)data_len);
        return false;
    }

    if (!s_is_initialised) {
        ESP_LOGE(TAG, "rylr896_send_data: driver not initialised");
        return false;
    }

    /*
     * Build AT+SEND command:
     *   AT+SEND=<dest_addr>,<length>,<data>
     */
    char cmd[CMD_BUF_SIZE];
    int cmd_len = snprintf(cmd, sizeof(cmd),
                           "AT+SEND=%d,%u,%s",
                           RYLR896_DEST_ADDR,
                           (unsigned int)data_len,
                           data);
    if (cmd_len < 0 || cmd_len >= (int)sizeof(cmd)) {
        ESP_LOGE(TAG, "rylr896_send_data: command buffer overflow");
        return false;
    }

    bool ok = send_at_command(cmd);
    if (ok) {
        ESP_LOGI(TAG, "Sent (%u bytes): %s", (unsigned int)data_len, data);
    } else {
        ESP_LOGE(TAG, "Send failed for payload: %s", data);
    }

    return ok;
}