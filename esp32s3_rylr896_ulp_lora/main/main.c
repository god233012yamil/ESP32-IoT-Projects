/**
 * @file main.c
 * @brief ESP32-S3 Ultra-Low Power LoRa Sensor — application entry point.
 *
 * Architecture overview
 * ---------------------
 * On cold boot the main CPU:
 *   1. Loads and starts the ULP RISC-V binary (ulp_lora_scheduler).
 *   2. Configures the ULP timer to fire every ULP_WAKEUP_PERIOD_US.
 *   3. Enables the ULP wakeup source and enters deep sleep.
 *
 * While the main CPU sleeps the ULP:
 *   - Wakes on each RTC timer tick.
 *   - Increments ulp_wakeup_counter.
 *   - When the counter reaches TX_INTERVAL_COUNT it sets ulp_tx_due = 1
 *     and calls ulp_riscv_wakeup_main_processor().
 *
 * On each ULP-triggered wakeup the main CPU:
 *   1. Checks that the wakeup cause is ULP.
 *   2. Reads ulp_tx_due; if set, performs a LoRa transmission.
 *   3. Clears the shared flag variables.
 *   4. Re-enters deep sleep.
 *
 * Power budget (typical, hardware-dependent)
 * ------------------------------------------
 *   Deep sleep (main CPU off, ULP running) : ~25 uA
 *   Active CPU window (UART TX + overhead) : <200 ms
 *   Average current at 60-second interval  : well under 100 uA
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ULP RISC-V host-side API (main CPU) */
#include "ulp_riscv.h"

/*
 * Generated header produced by ulp_embed_binary() at build time.
 * Provides extern declarations for all global variables defined in the ULP
 * source (ulp_wakeup_counter, ulp_tx_due) so the main CPU can read/write
 * them in RTC slow memory.
 *
 * NOTE: This file does not exist until the first `idf.py build` run.
 * IntelliSense will report "cannot open source file" until that first build
 * generates it at:
 *   build/esp-idf/main/ulp_lora_scheduler/ulp_lora_scheduler.h
 * This is expected behaviour — the project compiles correctly with idf.py.
 */
#include "ulp_lora_scheduler.h"

/* LoRa module driver */
#include "rylr896.h"

/* ---- Configuration ----------------------------------------------------- */

static const char *TAG = "MAIN";

/**
 * @brief ULP internal timer period in microseconds.
 *
 * The ULP wakes this often to increment its counter.
 * 1 000 000 us = 1 second per ULP tick.
 * With TX_INTERVAL_COUNT = 60 in the ULP source this gives one LoRa
 * transmission per minute.  Adjust both values to tune the duty cycle.
 */
#define ULP_WAKEUP_PERIOD_US   1000000ULL   /* 1 second */

/**
 * @brief LoRa payload format string.
 *
 * The actual payload is formatted at runtime with the current wakeup count
 * so each packet carries unique content.
 */
#define LORA_PAYLOAD_FMT       "SENSOR:PKT#%" PRIu32

/* ---- Private helpers -------------------------------------------------- */

/**
 * @brief Load and start the ULP RISC-V binary.
 *
 * Called once on cold boot.  Configures the ULP wake period and starts
 * the binary that was embedded by ulp_embed_binary() in CMakeLists.txt.
 *
 * @return ESP_OK on success, or an esp_err_t code on failure.
 */
static esp_err_t start_ulp_program(void)
{
    esp_err_t err;

    // Configure the ULP timer wakeup period; the ULP will wake every time this period elapses.
    err = ulp_set_wakeup_period(0, ULP_WAKEUP_PERIOD_US);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ulp_set_wakeup_period: %s", esp_err_to_name(err));
        return err;
    }

    /*
     * Load the ULP binary into RTC memory.
     * These linker symbols are injected by ulp_embed_binary() and point to
     * the raw .bin blob that was compiled from ulp_lora_scheduler.c.
     */
    extern const uint8_t ulp_lora_scheduler_bin_start[]
        asm("_binary_ulp_lora_scheduler_bin_start");
    extern const uint8_t ulp_lora_scheduler_bin_end[]
        asm("_binary_ulp_lora_scheduler_bin_end");

    size_t bin_size = (size_t)(ulp_lora_scheduler_bin_end
                               - ulp_lora_scheduler_bin_start);

    
    // Load the binary into the ULP instruction memory and verify it fits.
    err = ulp_riscv_load_binary(ulp_lora_scheduler_bin_start, bin_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ulp_riscv_load_binary: %s", esp_err_to_name(err));
        return err;
    }

    // Start the ULP program running; does not return if successful.
    err = ulp_riscv_run();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ulp_riscv_run: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ULP RISC-V started (period=%" PRIu64 " us)",
             (uint64_t)ULP_WAKEUP_PERIOD_US);
    return ESP_OK;
}

/**
 * @brief Perform a single LoRa transmission cycle.
 *
 * Initialises the UART driver, sends one packet, then releases the driver
 * to minimise power consumption.  Active window target: <200 ms.
 *
 * @param counter  Current value of ulp_wakeup_counter used in the payload.
 */
static void perform_lora_transmission(uint32_t counter)
{
    // Initialise the RYLR896 LoRa module; returns false on failure.
    if (!rylr896_init()) {
        ESP_LOGE(TAG, "Failed to initialise RYLR896 - skipping TX");
        return;
    }

    char payload[64];
    snprintf(payload, sizeof(payload), LORA_PAYLOAD_FMT, counter);

    // Send the LoRa data; returns false on failure.
    bool ok = rylr896_send_data(payload);
    if (!ok) {
        ESP_LOGE(TAG, "LoRa transmission failed (payload: %s)", payload);
    }

    // Deinitialise the LoRa module to save power until the next transmission.
    rylr896_deinit();
}

/**
 * @brief Configure and enter deep sleep with the ULP wakeup source enabled.
 *
 * This function does not return — the SoC resets on the next wakeup event.
 */
static void enter_deep_sleep(void)
{
    esp_err_t err = esp_sleep_enable_ulp_wakeup();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_sleep_enable_ulp_wakeup: %s", esp_err_to_name(err));
        /*
         * Without a wakeup source the device would sleep forever.
         * Restart to recover rather than hanging.
         */
        esp_restart();
    }

    ESP_LOGI(TAG, "Entering deep sleep - wake source: ULP");

    /*
     * Brief delay to let the UART FIFO drain so the log message above
     * reaches the console before power-down.
     */
    vTaskDelay(pdMS_TO_TICKS(10));

    esp_deep_sleep_start();
    /* Unreachable. */
}

/* ---- Application entry point ------------------------------------------ */

/**
 * @brief FreeRTOS application entry point.
 *
 * Handles both cold-boot initialisation and post-sleep wake processing.
 * Returns to deep sleep as quickly as possible to minimise average current.
 */
void app_main(void)
{
    // Check the wakeup cause to determine if this is a cold boot or a ULP wakeup.
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    if (cause == ESP_SLEEP_WAKEUP_ULP) {
        /*
         * ---- Wakeup from ULP ----
         * The ULP incremented its counter and signalled us.  Check the
         * shared tx_due flag; if set, transmit a LoRa packet.
         */
        ESP_LOGI(TAG, "Woke from ULP (counter=%" PRIu32 ", tx_due=%" PRIu32 ")",
                 (uint32_t)ulp_wakeup_counter,
                 (uint32_t)ulp_tx_due);

        if ((uint32_t)ulp_tx_due != 0U) {
            // Perform the LoRa transmission with the current wakeup counter value in the payload.
            perform_lora_transmission((uint32_t)ulp_wakeup_counter);
            // Clear shared flags — ULP reads these on the next cycle.
            ulp_wakeup_counter = 0;
            ulp_tx_due         = 0;
        } else {
            // Spurious wakeup — should not occur with current ULP logic.
            ESP_LOGW(TAG, "Woke from ULP but tx_due not set");
        }

    } else {
        /*
         * ---- Cold boot (or unexpected reset) ----
         * Initialise the ULP program and start it running.
         */
        if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
            ESP_LOGI(TAG, "Cold boot - loading ULP scheduler");
        } else {
            ESP_LOGI(TAG, "Unexpected wakeup cause %d - reloading ULP",
                     (int)cause);
        }

        // Reset shared variables in RTC memory before ULP starts.
        ulp_wakeup_counter = 0;
        ulp_tx_due         = 0;

        // Start the ULP program; does not return if successful.
        esp_err_t err = start_ulp_program();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start ULP (%s) - restarting",
                     esp_err_to_name(err));            
            // Restart to attempt recovery rather than hanging without a wakeup source.
            esp_restart();
        }
    }

    // Both paths converge here: return to deep sleep. 
    enter_deep_sleep();
}