#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define APP_UART_PORT ((uart_port_t)CONFIG_APP_UART_PORT_NUM)
#define APP_UART_TX_PIN CONFIG_APP_UART_TX_PIN
#define APP_UART_RX_PIN CONFIG_APP_UART_RX_PIN
#define APP_UART_BAUD_RATE CONFIG_APP_UART_BAUD_RATE
#define APP_UART_RX_BUFFER_SIZE CONFIG_APP_UART_RX_BUFFER_SIZE
#define APP_UART_TX_BUFFER_SIZE CONFIG_APP_UART_TX_BUFFER_SIZE

#define APP_UART_READ_CHUNK_SIZE 128U
#define APP_UART_LINE_BUFFER_SIZE 256U
#define APP_UART_TASK_STACK_SIZE 4096U
#define APP_UART_TASK_PRIORITY 5U
#define APP_UART_READ_TIMEOUT_MS 100U

static const char *TAG = "uart_app";

/**
 * @brief Sends a raw byte sequence through the application UART.
 *
 * This helper validates its arguments, queues the requested bytes for
 * transmission, and reports a driver error when the complete sequence cannot
 * be queued.
 *
 * @param data Pointer to the bytes that will be transmitted.
 * @param length Number of bytes to transmit.
 *
 * @return
 *     - ESP_OK: All bytes were queued successfully.
 *     - ESP_ERR_INVALID_ARG: The data pointer is NULL while length is nonzero.
 *     - ESP_FAIL: The UART driver did not accept the complete sequence.
 */
static esp_err_t app_uart_write(const uint8_t *data, size_t length)
{
    if ((data == NULL) && (length > 0U)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (length == 0U) {
        return ESP_OK;
    }

    const int bytes_written = uart_write_bytes(
        APP_UART_PORT,
        (const char *)data,
        length
    );

    if (bytes_written != (int)length) {
        ESP_LOGE(
            TAG,
            "UART write incomplete: requested=%u, queued=%d",
            (unsigned int)length,
            bytes_written
        );
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Sends a null-terminated text string through the application UART.
 *
 * @param text Pointer to a null-terminated string.
 *
 * @return ESP_OK on success, or an error returned by app_uart_write().
 */
static esp_err_t app_uart_write_text(const char *text)
{
    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return app_uart_write((const uint8_t *)text, strlen(text));
}

/**
 * @brief Converts a command line to uppercase in place.
 *
 * Converting commands to uppercase makes the command interface
 * case-insensitive while leaving the parser simple for beginners.
 *
 * @param text Mutable null-terminated command string.
 */
static void app_uart_to_uppercase(char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        *text = (char)toupper((unsigned char)*text);
        text++;
    }
}

/**
 * @brief Removes leading and trailing ASCII spaces from a string.
 *
 * @param text Mutable null-terminated string.
 *
 * @return Pointer to the first non-space character inside the original buffer.
 */
static char *app_uart_trim(char *text)
{
    if (text == NULL) {
        return NULL;
    }

    while ((*text == ' ') || (*text == '\t')) {
        text++;
    }

    char *end = text + strlen(text);
    while ((end > text) && ((end[-1] == ' ') || (end[-1] == '\t'))) {
        end--;
    }

    *end = '\0';
    return text;
}

/**
 * @brief Sends the list of commands supported by the example.
 */
static void app_uart_send_help(void)
{
    static const char help_text[] =
        "\r\nSupported commands:\r\n"
        "  HELP    - Show this command list\r\n"
        "  PING    - Verify that the UART link is working\r\n"
        "  STATUS  - Show the UART configuration and uptime\r\n"
        "  ECHO x  - Return the text following ECHO\r\n"
        "  CLEAR   - Print blank lines in the terminal\r\n"
        "\r\n";

    ESP_ERROR_CHECK(app_uart_write_text(help_text));
}

/**
 * @brief Sends a status report over UART.
 *
 * The report includes the selected UART controller, GPIO assignments, baud
 * rate, and application uptime.
 */
static void app_uart_send_status(void)
{
    char response[192];
    const int64_t uptime_ms = esp_timer_get_time() / 1000;

    const int length = snprintf(
        response,
        sizeof(response),
        "UART%d status:\r\n"
        "  TX GPIO: %d\r\n"
        "  RX GPIO: %d\r\n"
        "  Baud: %d\r\n"
        "  Uptime: %lld ms\r\n",
        CONFIG_APP_UART_PORT_NUM,
        APP_UART_TX_PIN,
        APP_UART_RX_PIN,
        APP_UART_BAUD_RATE,
        (long long)uptime_ms
    );

    if ((length < 0) || ((size_t)length >= sizeof(response))) {
        ESP_LOGE(TAG, "Failed to format UART status response");
        return;
    }

    ESP_ERROR_CHECK(app_uart_write((const uint8_t *)response, (size_t)length));
}

/**
 * @brief Executes one complete command received from the UART terminal.
 *
 * @param command_line Mutable null-terminated command line without CR or LF.
 */
static void app_uart_process_command(char *command_line)
{
    char *command = app_uart_trim(command_line);
    if ((command == NULL) || (*command == '\0')) {
        return;
    }

    // Keep a copy of the original text so the ECHO payload preserves its case.
    char original[APP_UART_LINE_BUFFER_SIZE];
    (void)snprintf(original, sizeof(original), "%s", command);

    app_uart_to_uppercase(command);

    if (strcmp(command, "HELP") == 0) {
        app_uart_send_help();
        return;
    }

    if (strcmp(command, "PING") == 0) {
        ESP_ERROR_CHECK(app_uart_write_text("PONG\r\n"));
        return;
    }

    if (strcmp(command, "STATUS") == 0) {
        app_uart_send_status();
        return;
    }

    if (strcmp(command, "CLEAR") == 0) {
        ESP_ERROR_CHECK(app_uart_write_text("\r\n\r\n\r\n\r\n\r\n"));
        return;
    }

    if (strncmp(command, "ECHO ", 5U) == 0) {
        ESP_ERROR_CHECK(app_uart_write_text(&original[5]));
        ESP_ERROR_CHECK(app_uart_write_text("\r\n"));
        return;
    }

    ESP_ERROR_CHECK(
        app_uart_write_text("Unknown command. Type HELP for a command list.\r\n")
    );
}

/**
 * @brief Consumes received bytes and extracts newline-terminated commands.
 *
 * The parser accepts CR, LF, or CRLF line endings. Backspace and DEL are also
 * supported so the example works naturally with common serial terminals.
 *
 * @param data Pointer to newly received UART bytes.
 * @param length Number of bytes available in data.
 * @param line_buffer Persistent line assembly buffer.
 * @param line_length Pointer to the number of valid bytes in line_buffer.
 */
static void app_uart_consume_bytes(
    const uint8_t *data,
    size_t length,
    char *line_buffer,
    size_t *line_length
)
{
    if ((data == NULL) || (line_buffer == NULL) || (line_length == NULL)) {
        return;
    }

    for (size_t index = 0U; index < length; index++) {
        const uint8_t byte = data[index];

        if ((byte == '\r') || (byte == '\n')) {
            if (*line_length > 0U) {
                line_buffer[*line_length] = '\0';
                app_uart_process_command(line_buffer);
                *line_length = 0U;
            }
            continue;
        }

        if ((byte == '\b') || (byte == 0x7FU)) {
            if (*line_length > 0U) {
                (*line_length)--;
            }
            continue;
        }

        // Ignore non-printable control characters in this text-based example.
        if (!isprint((unsigned char)byte)) {
            continue;
        }

        if (*line_length < (APP_UART_LINE_BUFFER_SIZE - 1U)) {
            line_buffer[*line_length] = (char)byte;
            (*line_length)++;
        } else {
            *line_length = 0U;
            ESP_ERROR_CHECK(
                app_uart_write_text(
                    "Input line too long. Buffer cleared.\r\n"
                )
            );
        }
    }
}

/**
 * @brief Receives UART bytes and feeds them to the command parser.
 *
 * @param argument Unused FreeRTOS task argument.
 */
static void app_uart_receive_task(void *argument)
{
    (void)argument;

    uint8_t read_buffer[APP_UART_READ_CHUNK_SIZE];
    char line_buffer[APP_UART_LINE_BUFFER_SIZE];
    size_t line_length = 0U;

    ESP_ERROR_CHECK(
        app_uart_write_text(
            "\r\nESP32 UART beginner example is ready.\r\n"
            "Type HELP and press Enter.\r\n> "
        )
    );

    while (true) {
        const int received_length = uart_read_bytes(
            APP_UART_PORT,
            read_buffer,
            sizeof(read_buffer),
            pdMS_TO_TICKS(APP_UART_READ_TIMEOUT_MS)
        );

        if (received_length > 0) {
            // Echo typed characters so a simple terminal behaves interactively.
            ESP_ERROR_CHECK(
                app_uart_write(read_buffer, (size_t)received_length)
            );

            app_uart_consume_bytes(
                read_buffer,
                (size_t)received_length,
                line_buffer,
                &line_length
            );

            // Display a new prompt after a complete command line.
            if ((read_buffer[received_length - 1] == '\r') ||
                (read_buffer[received_length - 1] == '\n')) {
                ESP_ERROR_CHECK(app_uart_write_text("> "));
            }
        }
    }
}

/**
 * @brief Initializes the ESP-IDF UART driver and GPIO routing.
 *
 * @return ESP_OK when initialization succeeds; otherwise, an ESP-IDF error.
 */
static esp_err_t app_uart_init(void)
{
    // Configure the UART parameters and install the driver. The driver will use
    // the default event queue and a simple polling receive method, so no event
    // queue or receive buffer is needed.
    const uart_config_t uart_config = {
        .baud_rate = APP_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // The driver will be installed with zero-length buffers since the example uses
    // a simple polling method to receive bytes directly into a stack buffer. 
    // This is not recommended for production applications due to the risk of data loss 
    // if the application does not call uart
    esp_err_t result = uart_driver_install(
        APP_UART_PORT,
        APP_UART_RX_BUFFER_SIZE,
        APP_UART_TX_BUFFER_SIZE,
        0,
        NULL,
        0
    );
    if (result != ESP_OK) {
        return result;
    }

    // Configure the UART parameters and GPIO routing. If any of these steps fail, 
    // the driver will be uninstalled before returning the error.
    result = uart_param_config(APP_UART_PORT, &uart_config);
    if (result != ESP_OK) {
        (void)uart_driver_delete(APP_UART_PORT);
        return result;
    }

    // Configure the UART GPIO pins. If this step fails, the driver will be uninstalled before returning the error.
    result = uart_set_pin(
        APP_UART_PORT,
        APP_UART_TX_PIN,
        APP_UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );
    if (result != ESP_OK) {
        (void)uart_driver_delete(APP_UART_PORT);
        return result;
    }

    return ESP_OK;
}

/**
 * @brief ESP-IDF application entry point.
 */
void app_main(void)
{
    // Initialize the UART driver and GPIO routing, then start the receive task.
    ESP_ERROR_CHECK(app_uart_init());

    ESP_LOGI(
        TAG,
        "UART%d initialized: TX=%d, RX=%d, baud=%d",
        CONFIG_APP_UART_PORT_NUM,
        APP_UART_TX_PIN,
        APP_UART_RX_PIN,
        APP_UART_BAUD_RATE
    );

    // The receive task will run indefinitely, so it is not necessary to check its return value after creation.
    const BaseType_t task_result = xTaskCreate(
        app_uart_receive_task,
        "uart_receive_task",
        APP_UART_TASK_STACK_SIZE,
        NULL,
        APP_UART_TASK_PRIORITY,
        NULL
    );

    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART receive task");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}
