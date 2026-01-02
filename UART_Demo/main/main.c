/**
 * @file uart_ref_main.c
 * @brief ESP32-S3 UART reference design using ESP-IDF UART driver + FreeRTOS.
 *
 * This single-file example demonstrates a practical, production-oriented UART
 * architecture for IoT applications:
 *  - Event-driven UART reception (UART driver event queue).
 *  - Fast RX task that forwards bytes to a StreamBuffer (burst absorption).
 *  - Parser task that converts a byte stream into newline-delimited commands.
 *  - TX task that is the only UART writer, fed by a FreeRTOS queue (no interleaving).
 *
 * Test (typical):
 *  1) Connect an external USB-UART adapter to the configured pins:
 *     - ESP TX -> Adapter RX
 *     - ESP RX -> Adapter TX
 *     - GND    -> GND
 *  2) Open a serial terminal at 115200 baud, 8-N-1.
 *  3) Send commands:
 *     - PING     -> PONG
 *     - VERSION  -> version string
 *     - UPTIME   -> uptime in milliseconds
 *
 * Notes:
 *  - UART0 is often used by the ESP-IDF console/logging. Prefer UART1/2 for external
 *    links to avoid conflicts (this example uses UART1).
 *  - If your external device is bursty or high baud rate, consider enabling RTS/CTS
 *    hardware flow control and wiring CTS/RTS accordingly.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_err.h"

// UART configuration
#define UART_PORT              UART_NUM_1
#define UART_TX_PIN            GPIO_NUM_17
#define UART_RX_PIN            GPIO_NUM_18
#define UART_BAUD_RATE         115200

#define UART_RX_BUF_SIZE       4096
#define UART_TX_BUF_SIZE       2048
#define UART_EVT_QUEUE_LEN     20

// Stream buffer for RX data
#define STREAM_BUF_SIZE        4096
#define STREAM_TRIG_LEVEL      1

static const char *TAG = "uart_ref";

/**
 * @brief UART TX message container for the TX queue.
 *
 * This struct uses a fixed-size payload for simplicity and determinism.
 * For large payloads, consider queuing pointers to allocated buffers with a clear
 * ownership/free policy.
 */
typedef struct {
    size_t len;
    uint8_t data[256];
} uart_tx_msg_t;

/**
 * @brief Line accumulator for newline-delimited command parsing.
 *
 * This accumulator collects bytes until a '\n' is found, then exposes a NULL
 * terminated string.
 */
typedef struct {
    char line[256];
    size_t len;
} line_acc_t;

static QueueHandle_t uart_evt_queue = NULL;
static StreamBufferHandle_t rx_stream = NULL;
static QueueHandle_t tx_queue = NULL;

/**
 * @brief Reset a line accumulator to an empty state.
 *
 * @param[in,out] a Pointer to the accumulator.
 */
static void line_acc_reset(line_acc_t *a)
{
    a->len = 0;
    a->line[0] = '\0';
}

/**
 * @brief Push bytes into a line accumulator and detect completed lines.
 *
 * This function appends bytes to the accumulator until a newline '\n' is found.
 * Carriage returns '\r' are ignored. If the line exceeds the buffer, the line is
 * dropped and the accumulator is reset (simple overflow policy).
 *
 * @param[in,out] a Pointer to the accumulator.
 * @param[in] data Byte buffer to consume.
 * @param[in] n Number of bytes in @p data.
 * @return int 1 if a full line is ready (terminated by '\n'), 0 otherwise.
 */
static int line_acc_push(line_acc_t *a, const uint8_t *data, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char c = (char)data[i];

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            a->line[a->len] = '\0';
            return 1;
        }

        if (a->len < (sizeof(a->line) - 1)) {
            a->line[a->len++] = c;
        } else {
            // Overflow: drop line and reset.
            line_acc_reset(a);
        }
    }
    return 0;
}

/**
 * @brief Enqueue a string for asynchronous UART transmission.
 *
 * The TX task is the only task that calls uart_write_bytes(), which prevents
 * output interleaving and makes it easy to rate-limit or prioritize if needed.
 *
 * @param[in] s NULL-terminated string to send.
 * @return bool true on success, false on failure (queue full or message too large).
 */
static bool tx_send_str(const char *s)
{
    uart_tx_msg_t msg;
    size_t n = strlen(s);

    if (n == 0) {
        return true;
    }
    if (n >= sizeof(msg.data)) {
        return false;
    }

    memcpy(msg.data, s, n);
    msg.len = n;

    return (xQueueSend(tx_queue, &msg, pdMS_TO_TICKS(20)) == pdTRUE);
}

/**
 * @brief Handle a completed newline-delimited command line.
 *
 * This function demonstrates a simple command-response protocol. In production,
 * replace/extend with your real protocol and business logic.
 *
 * Supported commands:
 *  - PING
 *  - VERSION
 *  - UPTIME
 *
 * @param[in] line NULL-terminated command line without the trailing newline.
 */
static void handle_line(const char *line)
{
    if (strcmp(line, "PING") == 0) {
        (void)tx_send_str("PONG\n");
        return;
    }

    if (strcmp(line, "VERSION") == 0) {
        (void)tx_send_str("ESP32S3_UART_REF v1\n");
        return;
    }

    if (strcmp(line, "UPTIME") == 0) {
        uint32_t ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        char resp[64];
        snprintf(resp, sizeof(resp), "UPTIME_MS %u\n", (unsigned)ms);
        (void)tx_send_str(resp);
        return;
    }

    (void)tx_send_str("ERR UNKNOWN_CMD\n");
}

/**
 * @brief Initialize UART driver, event queue, stream buffer, and TX queue.
 *
 * This installs the ESP-IDF UART driver with an event queue so RX can be handled
 * efficiently and safely under FreeRTOS.
 */
static void uart_ref_init(void)
{
    // UART configuration parameters
    uart_config_t cfg = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Install UART driver with event queue
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT,
                                        UART_RX_BUF_SIZE,
                                        UART_TX_BUF_SIZE,
                                        UART_EVT_QUEUE_LEN,
                                        &uart_evt_queue,
                                        0));

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    
    
    // Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT,
                                 UART_TX_PIN,
                                 UART_RX_PIN,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    // Create RX stream buffer
    rx_stream = xStreamBufferCreate(STREAM_BUF_SIZE, STREAM_TRIG_LEVEL);
    if (rx_stream == NULL) {
        ESP_LOGE(TAG, "Failed to create RX stream buffer");
        abort();
    }

    // Create TX queue
    tx_queue = xQueueCreate(10, sizeof(uart_tx_msg_t));
    if (tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create TX queue");
        abort();
    }

    ESP_LOGI(TAG, "UART initialized on port %d (TX=%d, RX=%d) @ %d baud",
             (int)UART_PORT, (int)UART_TX_PIN, (int)UART_RX_PIN, (int)UART_BAUD_RATE);
}

/**
 * @brief UART RX task: consumes UART driver events and forwards bytes to a StreamBuffer.
 *
 * This task should remain fast and avoid heavy parsing or excessive logging.
 * It reads UART bytes when UART_DATA events occur and pushes them into a stream
 * buffer to decouple UART I/O from parsing/application logic.
 *
 * Overflow policy:
 *  - On FIFO overflow or driver buffer full, flush UART input and reset the event queue.
 *
 * @param[in] arg Unused.
 */
static void uart_rx_event_task(void *arg)
{
    (void)arg;

    uart_event_t evt;

    // RX buffer for uart_read_bytes()
    uint8_t *buf = (uint8_t *)malloc(1024);
    if (buf == NULL) {
        ESP_LOGE(TAG, "RX malloc failed");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        // Wait for UART events
        if (xQueueReceive(uart_evt_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // Handle data event
        if (evt.type == UART_DATA) {
            // Find out how many bytes to read
            int to_read = (int)evt.size;

            while (to_read > 0) {
                int chunk = (to_read > 1024) ? 1024 : to_read;

                // Read bytes from UART
                int n = uart_read_bytes(UART_PORT, buf, chunk, pdMS_TO_TICKS(20));

                // Push bytes to StreamBuffer
                if (n > 0) {
                    size_t pushed = xStreamBufferSend(rx_stream, buf, (size_t)n, 0);
                    if (pushed < (size_t)n) {
                        ESP_LOGW(TAG, "RX stream full, dropped %d bytes", (int)((size_t)n - pushed));
                    }
                    to_read -= n;
                } else {
                    // No data read within timeout; exit this inner loop.
                    break;
                }
            }
            continue;
        }

        // Handle overflow events
        if (evt.type == UART_FIFO_OVF || evt.type == UART_BUFFER_FULL) {
            ESP_LOGW(TAG, "UART overflow/buffer full, flushing input");
            uart_flush_input(UART_PORT);
            xQueueReset(uart_evt_queue);
            continue;
        }

        // Handle frame error event
        if (evt.type == UART_FRAME_ERR) {
            ESP_LOGW(TAG, "UART frame error");
            continue;
        }

        // Handle parity error event
        if (evt.type == UART_PARITY_ERR) {
            ESP_LOGW(TAG, "UART parity error");
            continue;
        }

        // Other events can be handled here if needed (break, pattern detection, etc.)
    }

    // Unreachable in normal operation.
    free(buf);
    vTaskDelete(NULL);
}

/**
 * @brief Parser task: pulls bytes from the StreamBuffer and extracts newline-delimited commands.
 *
 * This task converts the received byte stream into application-level messages.
 * For simplicity, this example uses newline-delimited ASCII commands. For binary
 * protocols, implement a framing parser (length-prefixed, SLIP, COBS, etc.).
 *
 * @param[in] arg Unused.
 */
static void uart_parser_task(void *arg)
{
    (void)arg;

    uint8_t tmp[128];
    line_acc_t acc;
    line_acc_reset(&acc);

    while (1) {
        // Read bytes from the RX stream buffer
        size_t n = xStreamBufferReceive(rx_stream, tmp, sizeof(tmp), pdMS_TO_TICKS(200));
        if (n == 0) {
            // Timeout. You may implement line timeout/resync policies here.
            continue;
        }

        if (line_acc_push(&acc, tmp, n)) {
            ESP_LOGI(TAG, "CMD: %s", acc.line);
            handle_line(acc.line);
            line_acc_reset(&acc);
        }
    }
}

/**
 * @brief UART TX task: the only task that writes to UART.
 *
 * This task ensures UART output is serialized. All other tasks should enqueue
 * messages to @c tx_queue and never write directly to the UART peripheral.
 *
 * @param[in] arg Unused.
 */
static void uart_tx_task(void *arg)
{
    (void)arg;

    uart_tx_msg_t msg;

    while (1) {
        // Wait for a message to send
        if (xQueueReceive(tx_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // Send the message via UART
        uart_write_bytes(UART_PORT, (const char *)msg.data, msg.len);

        // Wait for transmission to complete
        uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
    }
}

/**
 * @brief ESP-IDF application entry point.
 *
 * Initializes UART and starts the RX, parser, and TX tasks.
 */
void app_main(void)
{
    // Initialize UART and related resources
    uart_ref_init();

    // Priorities: RX slightly higher than parser; TX similar to parser.
    xTaskCreate(uart_rx_event_task, "uart_rx_evt", 4096, NULL, 12, NULL);
    xTaskCreate(uart_parser_task,   "uart_parser", 4096, NULL, 10, NULL);
    xTaskCreate(uart_tx_task,       "uart_tx",     3072, NULL, 10, NULL);

    (void)tx_send_str("READY\n");
}