/**
 * @file ulp_lora_scheduler.c
 * @brief ULP RISC-V coprocessor scheduler for LoRa transmission timing.
 *
 * Symbol naming rule (ESP-IDF v5.4.1)
 * ------------------------------------
 * The ULP build system automatically prepends "ulp_" to every global symbol
 * when generating the exported header (ulp_lora_scheduler.h).  Therefore:
 *
 *   Variable name in THIS file  | Declared in generated header as
 *   ----------------------------|---------------------------------
 *   wakeup_counter              | extern uint32_t ulp_wakeup_counter;
 *   tx_due                      | extern uint32_t ulp_tx_due;
 *
 * If the variable were named "ulp_wakeup_counter" here, the header would
 * declare it as "ulp_ulp_wakeup_counter" — causing the compile error:
 *   error: 'ulp_wakeup_counter' undeclared; did you mean 'ulp_ulp_wakeup_counter'?
 *
 * main.c accesses the symbols using the header names (with the ulp_ prefix),
 * so main.c uses ulp_wakeup_counter and ulp_tx_due — which is correct.
 */

#include <stdint.h>
#include "ulp_riscv_utils.h"

/* ---- Shared variables (exported to main CPU with "ulp_" prefix added) -- */

/**
 * @brief Wake cycle counter — incremented on every ULP timer tick.
 *
 * Exported as ulp_wakeup_counter in the generated header.
 * Reset to 0 by the main CPU after each LoRa transmission.
 */
volatile uint32_t wakeup_counter = 0;

/**
 * @brief Transmission-due flag — set when wakeup_counter reaches threshold.
 *
 * Exported as ulp_tx_due in the generated header.
 * Cleared to 0 by the main CPU after a successful LoRa transmission.
 */
volatile uint32_t tx_due = 0;

/* ---- Configuration ----------------------------------------------------- */

/**
 * @brief Number of ULP wake cycles between LoRa transmissions.
 *
 * With ULP_WAKEUP_PERIOD_US = 1,000,000 us (1 s) set in main.c,
 * TX_INTERVAL_COUNT = 60 gives one LoRa packet every ~60 seconds.
 */
#define TX_INTERVAL_COUNT  60U

/* ---- Entry point ------------------------------------------------------- */

/**
 * @brief ULP RISC-V entry point — invoked on every RTC timer tick.
 */
int main(void)
{
    wakeup_counter++;

    if (wakeup_counter >= TX_INTERVAL_COUNT) {
        tx_due = 1U;
        // Wake the main CPU to handle the transmission; does not return if successful.
        ulp_riscv_wakeup_main_processor();
    }

    /*
     * Halt the ULP until the next timer tick to save power.
     * The main CPU will wake us when tx_due is set, but we have no
     * other work to do until then.
     */
    ulp_riscv_halt();
    return 0;
}