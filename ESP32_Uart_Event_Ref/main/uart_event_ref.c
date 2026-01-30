/**
 * @file uart_event_ref.c
 * @brief ESP-IDF UART reference: event-driven RX + FreeRTOS tasks + error handling.
 *
 * This file demonstrates a production-friendly UART pattern for ESP32 projects:
 * - Configure UART (baud, framing, pins) using the ESP-IDF driver APIs.
 * - Install the UART driver with an event queue, so the ISR signals a FreeRTOS task.
 * - Run a dedicated RX task that blocks on the event queue, then reads available data.
 * - Handle common UART error events (FIFO overflow, buffer full, frame/parity errors).
 * - Implement a simple CR/LF line protocol to show a real notice-and-respond workflow.
 *
 * Wiring (default pins):
 * - USB-UART TX -> ESP32 RX (GPIO16)
 * - USB-UART RX -> ESP32 TX (GPIO17)
 * - GND -> GND
 */

#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_check.h"


/* ---------- User-tunable settings (keep simple; can be moved to menuconfig later) ---------- */

#define UART_PORT               UART_NUM_1
#define UART_BAUDRATE           115200
#define UART_TX_PIN             GPIO_NUM_17
#define UART_RX_PIN             GPIO_NUM_16

#define UART_RX_BUF_SIZE        2048
#define UART_TX_BUF_SIZE        2048
#define UART_EVT_QUEUE_LEN      20

#define RX_TASK_STACK           4096
#define RX_TASK_PRIO            10

#define TX_TASK_STACK           3072
#define TX_TASK_PRIO            9

#define LINE_BUF_SIZE           256

static const char *TAG = "uart_ref";
static QueueHandle_t s_uart_evt_queue = NULL;

// Error counters (simple visibility for beginners)
static uint32_t s_frame_err_count = 0;
static uint32_t s_parity_err_count = 0;
static uint32_t s_fifo_ovf_count = 0;
static uint32_t s_buf_full_count = 0;

// Optional: signal UART ready to other tasks 
static EventGroupHandle_t s_sys_eg = NULL;
#define SYS_EG_UART_READY_BIT   (1U << 0)

/**
 * @brief Write a string to the configured UART port.
 *
 * This is a convenience helper for small messages.
 * In real products, consider a dedicated TX queue or a mutex if multiple tasks transmit.
 *
 * @param text Null-terminated string to transmit.
 */
static void uart_write_str(const char *text)
{
    if (text == NULL) {
        return;
    }
    uart_write_bytes(UART_PORT, text, (int)strlen(text));
}

/**
 * @brief Configure UART and install the driver in event-queue mode.
 *
 * This sets typical UART framing (8N1), disables HW flow control, assigns pins,
 * and installs the driver with RX/TX ring buffers and an ISR-driven event queue.
 *
 * @return esp_err_t ESP_OK on success, otherwise an ESP-IDF error code.
 */
static esp_err_t uart_init_event_mode(void)
{
    // Configure UART parameters
    uart_config_t cfg = {
        .baud_rate = UART_BAUDRATE,             // Baud rate
        .data_bits = UART_DATA_8_BITS,          // Data bits
        .parity    = UART_PARITY_DISABLE,       // No parity
        .stop_bits = UART_STOP_BITS_1,          // 1 stop bit
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,  // No flow control
        .source_clk = UART_SCLK_DEFAULT,        // Use default source clock
    };
    ESP_RETURN_ON_ERROR(uart_param_config(UART_PORT, &cfg), TAG, "uart_param_config failed");

    // Set UART pins
    ESP_RETURN_ON_ERROR(uart_set_pin(
        UART_PORT,                              // UART port number
        UART_TX_PIN,                            // TX pin
        UART_RX_PIN,                            // RX pin
        UART_PIN_NO_CHANGE,                     // RTS pin
        UART_PIN_NO_CHANGE                      // CTS pin
        ), TAG, "uart_set_pin failed");

    // Install UART driver with event queue
    ESP_RETURN_ON_ERROR(uart_driver_install(
        UART_PORT,                              // UART port number
        UART_RX_BUF_SIZE,                       // RX buffer size
        UART_TX_BUF_SIZE,                       // TX buffer size
        UART_EVT_QUEUE_LEN,                     // Event queue length
        &s_uart_evt_queue,                      // Event queue handle
        0                                       // Interrupt allocation flags
        ), TAG, "uart_driver_install failed"
    );

    return ESP_OK;
}

/**
 * @brief Reset UART input state after an overflow/buffer-full condition.
 *
 * This recovery path is intentionally simple:
 * - Flush driver input (drops buffered bytes)
 * - Reset event queue (drops pending events)
 *
 * For binary protocols, you might instead resync on a known header or delimiter.
 */
static void uart_recover_from_overflow(void)
{
    uart_flush_input(UART_PORT);
    if (s_uart_evt_queue != NULL) {
        xQueueReset(s_uart_evt_queue);
    }
}

/**
 * @brief Handle a complete received line.
 *
 * This function demonstrates a minimal "console-like" interface.
 * It echoes the line and implements a few example commands:
 * - help
 * - status
 *
 * @param line Null-terminated ASCII line without CR/LF.
 */
static void handle_line(const char *line)
{
    if (line == NULL) {
        return;
    }

    // Echo the received line
    uart_write_str("echo: ");
    uart_write_str(line);
    uart_write_str("\r\n");

    // Handle "help" command
    if (strcmp(line, "help") == 0) {
        uart_write_str("commands: help, status\r\n");
        return;
    }

    // Handle "status" command
    if (strcmp(line, "status") == 0) {
        char msg[192];
        snprintf(msg, sizeof(msg),
                 "status: frame_err=%" PRIu32 ", parity_err=%" PRIu32
                 ", fifo_ovf=%" PRIu32 ", buf_full=%" PRIu32 "\r\n",
                 s_frame_err_count, s_parity_err_count, s_fifo_ovf_count, s_buf_full_count);
        uart_write_str(msg);
        return;
    }

    uart_write_str("unknown cmd (type 'help')\r\n");
}

/**
 * @brief Append received bytes into a line buffer and dispatch complete lines.
 *
 * This is a practical pattern for UART command consoles:
 * - Accumulate bytes until CR/LF
 * - On delimiter, NUL-terminate and handle the line
 * - Protect against overlong input by resetting the buffer
 *
 * @param buf Pointer to received bytes.
 * @param len Number of received bytes.
 * @param line_buf Destination line buffer.
 * @param line_len Current length in line_buf; updated by this function.
 */
static void line_accumulator_feed(const uint8_t *buf, int len, char *line_buf, size_t *line_len)
{
    if (buf == NULL || line_buf == NULL || line_len == NULL || len <= 0) {
        return;
    }

    for (int i = 0; i < len; i++) {
        const char c = (char)buf[i];

        /* Treat CR or LF as line terminator */
        if (c == '\r' || c == '\n') {
            if (*line_len > 0) {
                line_buf[*line_len] = '\0';
                handle_line(line_buf);
                *line_len = 0;
            }
            continue;
        }

        if (*line_len < (LINE_BUF_SIZE - 1)) {
            line_buf[*line_len] = c;
            (*line_len)++;
        } else {
            /* Overlong line: reset (simple and safe) */
            uart_write_str("warn: line too long, resetting\r\n");
            *line_len = 0;
        }
    }
}

/**
 * @brief FreeRTOS task: wait for UART driver events and handle RX + errors.
 *
 * The UART driver posts events from ISR context into an event queue.
 * This task blocks on that queue and responds deterministically:
 * - UART_DATA: read bytes and feed a line accumulator
 * - UART_FIFO_OVF / UART_BUFFER_FULL: flush and recover
 * - UART_FRAME_ERR / UART_PARITY_ERR: count and flush (simple resync)
 *
 * @param arg Unused.
 */
static void uart_event_task(void *arg)
{
    (void)arg;

    uart_event_t evt;
    uint8_t rx[256];

    char line_buf[LINE_BUF_SIZE];
    size_t line_len = 0;

    ESP_LOGI(TAG, "UART event task started (port=%d, baud=%d)", (int)UART_PORT, UART_BAUDRATE);

    if (s_sys_eg != NULL) {
        xEventGroupSetBits(s_sys_eg, SYS_EG_UART_READY_BIT);
    }

    while (1) {

        // Wait for next UART event
        if (xQueueReceive(s_uart_evt_queue, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (evt.type) {
            case UART_DATA: {
                // Read the bytes associated with this event
                int to_read = (evt.size < (int)sizeof(rx)) ? evt.size : (int)sizeof(rx);
                int n = uart_read_bytes(UART_PORT, rx, to_read, 0);
                if (n > 0) {
                    line_accumulator_feed(rx, n, line_buf, &line_len);
                }
                break;
            }

            case UART_FIFO_OVF:
                s_fifo_ovf_count++;
                ESP_LOGW(TAG, "UART FIFO overflow, recovering");
                uart_recover_from_overflow();
                break;

            case UART_BUFFER_FULL:
                s_buf_full_count++;
                ESP_LOGW(TAG, "UART ring buffer full, recovering");
                uart_recover_from_overflow();
                break;

            case UART_FRAME_ERR:
                s_frame_err_count++;
                ESP_LOGW(TAG, "UART frame error (count=%" PRIu32 ")", s_frame_err_count);
                // Simple resync: flush. For structured protocols, resync on delimiter/header.
                uart_flush_input(UART_PORT);
                break;

            case UART_PARITY_ERR:
                s_parity_err_count++;
                ESP_LOGW(TAG, "UART parity error (count=%" PRIu32 ")", s_parity_err_count);
                uart_flush_input(UART_PORT);
                break;

            default:
                // Keep the beginner reference clean: ignore the rest. 
                break;
        }
    }
}

/**
 * @brief FreeRTOS task: periodically transmit a heartbeat over UART.
 *
 * This is a realistic "system is alive" pattern. It also proves TX works while RX is event-driven.
 *
 * @param arg Unused.
 */
static void uart_tx_heartbeat_task(void *arg)
{
    (void)arg;

    // Wait until UART is ready
    if (s_sys_eg != NULL) {
        xEventGroupWaitBits(s_sys_eg, SYS_EG_UART_READY_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }

    int counter = 0;
    while (1) {
        char msg[96];
        snprintf(msg, sizeof(msg), "heartbeat %d (type 'help' or 'status')\r\n", counter++);
        uart_write_str(msg);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/**
 * @brief Application entry point.
 *
 * Initializes UART in event mode, prints a banner, then creates:
 * - An event-driven UART RX task (primary reference pattern)
 * - An optional TX heartbeat task (real-life "alive" indicator)
 */
void app_main(void)
{
    // Create system event group
    s_sys_eg = xEventGroupCreate();

    // Initialize UART in event mode
    ESP_ERROR_CHECK(uart_init_event_mode());

    // Print banner
    uart_write_str("\r\n");
    uart_write_str("=== ESP32 UART Event Reference ===\r\n");
    uart_write_str("Type: help, status\r\n");
    uart_write_str("==================================\r\n");

    // Create the UART event task
    xTaskCreate(uart_event_task, "uart_event_task", RX_TASK_STACK, NULL, RX_TASK_PRIO, NULL);
    
    // Create a TX heartbeat task
    xTaskCreate(uart_tx_heartbeat_task, "uart_tx_hb_task", TX_TASK_STACK, NULL, TX_TASK_PRIO, NULL);
}