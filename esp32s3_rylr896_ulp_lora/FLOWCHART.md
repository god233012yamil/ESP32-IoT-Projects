# Program Flow — ESP32-S3 ULP LoRa Sensor

This document describes the complete program flow using Mermaid diagrams.

---

## 1. Top-Level System Flow

The system alternates between two states: the main CPU handling a transmission
and the ULP coprocessor counting sleep ticks. The main CPU is active for less
than 200 ms out of every 60 seconds.

```mermaid
stateDiagram-v2
    direction LR

    [*] --> ColdBoot : Power on / Hard reset

    ColdBoot --> ULPRunning : Load ULP binary\nStart ULP timer\nEnter deep sleep

    ULPRunning --> ULPRunning : RTC tick\n(counter < 60)

    ULPRunning --> MainCPUActive : counter >= 60\ntx_due = 1\nWake main CPU

    MainCPUActive --> ULPRunning : Send LoRa packet\nClear counters\nEnter deep sleep

    note right of ULPRunning
        ~25–30 µA
        ULP RISC-V only
        Main CPU OFF
    end note

    note right of MainCPUActive
        ~40–80 mA
        Duration < 200 ms
        UART + LoRa TX
    end note
```

---

## 2. app_main() — Boot and Wakeup Dispatch

Every time the ESP32-S3 starts (cold boot or deep-sleep wakeup), execution
enters `app_main()`. The wakeup cause determines which path is taken.

```mermaid
flowchart TD
    START([app_main called]) --> GET_CAUSE[esp_sleep_get_wakeup_cause]

    GET_CAUSE --> IS_ULP{Wakeup cause\n== ESP_SLEEP_WAKEUP_ULP?}

    IS_ULP -- Yes --> LOG_WAKE[Log: counter and tx_due values]
    IS_ULP -- No --> IS_UNDEF{cause ==\nESP_SLEEP_WAKEUP_UNDEFINED?}

    IS_UNDEF -- Yes\ncold boot --> LOG_COLD[Log: Cold boot]
    IS_UNDEF -- No\nunexpected reset --> LOG_UNEXPECTED[Log: Unexpected wakeup cause]

    LOG_COLD --> RESET_VARS[Reset ulp_wakeup_counter = 0\nReset ulp_tx_due = 0]
    LOG_UNEXPECTED --> RESET_VARS

    RESET_VARS --> START_ULP[start_ulp_program]
    START_ULP --> ULP_OK{ESP_OK?}
    ULP_OK -- No --> RESTART([esp_restart])
    ULP_OK -- Yes --> SLEEP

    LOG_WAKE --> TX_DUE{ulp_tx_due != 0?}
    TX_DUE -- Yes --> TX[perform_lora_transmission\ncounter]
    TX_DUE -- No --> LOG_SPURIOUS[Log: Spurious wakeup warning]

    TX --> CLEAR[ulp_wakeup_counter = 0\nulp_tx_due = 0]
    CLEAR --> SLEEP
    LOG_SPURIOUS --> SLEEP

    SLEEP([enter_deep_sleep]) --> DONE([esp_deep_sleep_start\nnever returns])
```

---

## 3. start_ulp_program() — ULP Initialisation

Called once on cold boot. Loads the compiled ULP RISC-V binary from flash into
RTC memory and starts the RTC timer that will tick every 1 second.

```mermaid
flowchart TD
    START([start_ulp_program]) --> SET_PERIOD[ulp_set_wakeup_period\n0, 1_000_000 us]
    SET_PERIOD --> PER_OK{ESP_OK?}
    PER_OK -- No --> ERR1([return error])
    PER_OK -- Yes --> LOAD[ulp_riscv_load_binary\nulp_lora_scheduler_bin_start\nbin_size]
    LOAD --> LOAD_OK{ESP_OK?}
    LOAD_OK -- No --> ERR2([return error])
    LOAD_OK -- Yes --> RUN[ulp_riscv_run]
    RUN --> RUN_OK{ESP_OK?}
    RUN_OK -- No --> ERR3([return error])
    RUN_OK -- Yes --> LOG[Log: ULP RISC-V started\nperiod = 1000000 us]
    LOG --> RET([return ESP_OK])
```

---

## 4. ULP RISC-V Scheduler — ulp_lora_scheduler.c

This program executes on the ULP RISC-V core. It is invoked by the RTC timer
every 1 second. The main CPU is completely powered off during this time.

```mermaid
flowchart TD
    START([RTC timer fires\nULP wakes]) --> INC[wakeup_counter++]
    INC --> CHECK{wakeup_counter\n>= TX_INTERVAL_COUNT\n60?}
    CHECK -- No --> HALT
    CHECK -- Yes --> SET_FLAG[tx_due = 1]
    SET_FLAG --> WAKE[ulp_riscv_wakeup_main_processor]
    WAKE --> HALT([ulp_riscv_halt\nULP sleeps until\nnext RTC tick])
```

---

## 5. perform_lora_transmission() — LoRa TX Cycle

Called by `app_main()` when the ULP has signalled a transmission is due.
The entire UART driver lifecycle is contained within this function.

```mermaid
flowchart TD
    START([perform_lora_transmission\ncounter]) --> INIT[rylr896_init]
    INIT --> INIT_OK{success?}
    INIT_OK -- No --> LOG_FAIL[Log: Failed to initialise\nSkipping TX]
    LOG_FAIL --> DONE([return])

    INIT_OK -- Yes --> FMT[snprintf payload\nSENSOR:PKT#counter]
    FMT --> SEND[rylr896_send_data\npayload]
    SEND --> SEND_OK{success?}
    SEND_OK -- No --> LOG_ERR[Log: TX failed]
    SEND_OK -- Yes --> LOG_OK[Log: Sent N bytes]
    LOG_ERR --> DEINIT
    LOG_OK --> DEINIT[rylr896_deinit\nuart_driver_delete]
    DEINIT --> DONE2([return])
```

---

## 6. rylr896_init() — UART and Module Setup

Installs the ESP-IDF UART driver and configures the RYLR896 module with the
address, network ID, and radio frequency required for the network.

```mermaid
flowchart TD
    START([rylr896_init]) --> ALREADY{already\ninitialised?}
    ALREADY -- Yes --> RET_TRUE([return true])
    ALREADY -- No --> CFG[uart_param_config\n115200 8N1]
    CFG --> CFG_OK{ESP_OK?}
    CFG_OK -- No --> ERR([return false])
    CFG_OK -- Yes --> PINS[uart_set_pin\nTX=17 RX=18]
    PINS --> PINS_OK{ESP_OK?}
    PINS_OK -- No --> ERR
    PINS_OK -- Yes --> INSTALL[uart_driver_install\nRX buf 256]
    INSTALL --> INST_OK{ESP_OK?}
    INST_OK -- No --> ERR
    INST_OK -- Yes --> DELAY[vTaskDelay 100 ms\nmodule settle time]
    DELAY --> AT[send_at_command: AT]
    AT --> AT_OK{+OK?}
    AT_OK -- No --> DEL[uart_driver_delete]
    DEL --> ERR
    AT_OK -- Yes --> ADDR[AT+ADDRESS=1]
    ADDR --> ADDR_OK{+OK?}
    ADDR_OK -- No --> DEL
    ADDR_OK -- Yes --> NET[AT+NETWORKID=18]
    NET --> NET_OK{+OK?}
    NET_OK -- No --> DEL
    NET_OK -- Yes --> BAND[AT+BAND=915000000]
    BAND --> BAND_OK{+OK?}
    BAND_OK -- No --> DEL
    BAND_OK -- Yes --> SET_FLAG[s_is_initialised = true]
    SET_FLAG --> LOG[Log: Initialised]
    LOG --> RET([return true])
```

---

## 7. rylr896_send_data() — AT+SEND Command

Validates the payload, formats the AT+SEND command, transmits it over UART,
and waits for the `+OK` acknowledgement from the module.

```mermaid
flowchart TD
    START([rylr896_send_data\ndata]) --> NULL{data == NULL?}
    NULL -- Yes --> ERR([return false])
    NULL -- No --> EMPTY{strlen == 0?}
    EMPTY -- Yes --> ERR
    EMPTY -- No --> LEN{len > 240?}
    LEN -- Yes --> ERR
    LEN -- No --> INIT_CHK{s_is_initialised?}
    INIT_CHK -- No --> ERR
    INIT_CHK -- Yes --> FMT[snprintf cmd\nAT+SEND=0,len,data]
    FMT --> FLUSH[uart_flush_input]
    FLUSH --> WRITE[uart_write_bytes\ncmd]
    WRITE --> WRITE_OK{written ==\nlen?}
    WRITE_OK -- No --> ERR
    WRITE_OK -- Yes --> WAIT_TX[uart_wait_tx_done\n3000 ms]
    WAIT_TX --> TX_OK{ESP_OK?}
    TX_OK -- No --> ERR
    TX_OK -- Yes --> READ[uart_read_bytes\nup to 127 bytes\n3000 ms timeout]
    READ --> GOT_BYTES{rxlen > 0?}
    GOT_BYTES -- No --> ERR
    GOT_BYTES -- Yes --> SEARCH{strstr response\n+OK?}
    SEARCH -- No --> LOG_ERR[Log: unexpected response]
    LOG_ERR --> ERR
    SEARCH -- Yes --> LOG_OK[Log: Sent N bytes]
    LOG_OK --> RET([return true])
```

---

## 8. enter_deep_sleep() — Sleep Entry

Final function called on every execution path. Configures the ULP as the wakeup
source and powers down the main CPU. Never returns.

```mermaid
flowchart TD
    START([enter_deep_sleep]) --> ENABLE[esp_sleep_enable_ulp_wakeup]
    ENABLE --> OK{ESP_OK?}
    OK -- No --> LOG_ERR[Log: wakeup config failed]
    LOG_ERR --> RESTART([esp_restart\nrecover from bad state])
    OK -- Yes --> LOG[Log: Entering deep sleep]
    LOG --> DELAY[vTaskDelay 10 ms\ndrain UART FIFO]
    DELAY --> SLEEP([esp_deep_sleep_start\nnever returns])
```

---

## 9. Complete Sequence Diagram

End-to-end timing across two full transmission cycles, showing the interaction
between hardware layers.

```mermaid
sequenceDiagram
    participant HW as RTC Timer
    participant ULP as ULP RISC-V
    participant CPU as Main CPU (Xtensa)
    participant MOD as RYLR896 Module

    Note over CPU: Power on / Reset
    CPU->>ULP: Load binary, set period 1s
    CPU->>ULP: ulp_riscv_run()
    CPU->>CPU: esp_deep_sleep_start()
    Note over CPU: SLEEPING

    loop Every 1 second (×59)
        HW->>ULP: RTC timer fires
        ULP->>ULP: wakeup_counter++
        ULP->>ULP: ulp_riscv_halt()
    end

    HW->>ULP: RTC timer fires (tick 60)
    ULP->>ULP: wakeup_counter++ → 60
    ULP->>ULP: tx_due = 1
    ULP->>CPU: ulp_riscv_wakeup_main_processor()
    Note over CPU: WAKING UP

    CPU->>MOD: UART: AT\r\n
    MOD-->>CPU: +OK\r\n
    CPU->>MOD: AT+ADDRESS=1\r\n
    MOD-->>CPU: +OK\r\n
    CPU->>MOD: AT+NETWORKID=18\r\n
    MOD-->>CPU: +OK\r\n
    CPU->>MOD: AT+BAND=915000000\r\n
    MOD-->>CPU: +OK\r\n
    CPU->>MOD: AT+SEND=0,14,SENSOR:PKT#60\r\n
    MOD-->>CPU: +OK\r\n
    Note over MOD: RF packet transmitted

    CPU->>CPU: uart_driver_delete()
    CPU->>ULP: ulp_wakeup_counter = 0
    CPU->>ULP: ulp_tx_due = 0
    CPU->>CPU: esp_deep_sleep_start()
    Note over CPU: SLEEPING

    loop Next 60-second cycle...
        HW->>ULP: RTC timer fires
        ULP->>ULP: wakeup_counter++
        ULP->>ULP: ulp_riscv_halt()
    end
```

---

## 10. RTC Shared Memory Layout

Two 32-bit variables in RTC slow memory are the only communication channel
between the ULP and the main CPU.

```mermaid
graph TD
    subgraph RTC["RTC Slow Memory — survives deep sleep"]
        direction LR
        WC["<b>ulp_wakeup_counter</b><br/>Type: uint32_t<br/>Written by: ULP core<br/>Read + cleared by: Main CPU<br/>Range: 0 – 60"]
        TD["<b>ulp_tx_due</b><br/>Type: uint32_t<br/>Set to 1 by: ULP core<br/>Cleared to 0 by: Main CPU<br/>Values: 0 or 1"]
    end

    ULP([ULP RISC-V Core]) -->|increments each tick| WC
    ULP -->|sets to 1 at threshold| TD
    WC -->|read on wakeup| CPU([Main CPU])
    TD -->|checked on wakeup| CPU
    CPU -->|reset to 0 after TX| WC
    CPU -->|reset to 0 after TX| TD
```

> **Naming note:** In the ULP source these variables are named `wakeup_counter` and `tx_due`. The ESP-IDF v5.4.1 ULP build system automatically prepends `ulp_` when generating the exported header, so the main CPU accesses them as `ulp_wakeup_counter` and `ulp_tx_due`.
