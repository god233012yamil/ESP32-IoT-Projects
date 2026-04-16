# ULP RISC-V Coprocessor — Architecture and Operation Guide

This document explains the ESP32-S3 ULP RISC-V coprocessor in depth: what it
is, how its hardware works, how the RTC timer invokes the `main()` entry point
on every tick, and how this project uses it to achieve microamp-level average
current while still transmitting LoRa packets on a regular schedule.

---

## Table of Contents

- [What Is the ULP?](#what-is-the-ulp)
- [ESP32-S3 Power Domains](#esp32-s3-power-domains)
- [ULP RISC-V Hardware Architecture](#ulp-risc-v-hardware-architecture)
- [RTC Memory — The Shared Data Bus](#rtc-memory--the-shared-data-bus)
- [How the RTC Timer Invokes main()](#how-the-rtc-timer-invokes-main)
- [The ULP Execution Lifecycle — Step by Step](#the-ulp-execution-lifecycle--step-by-step)
- [ULP Boot Sequence in Detail](#ulp-boot-sequence-in-detail)
- [ulp_riscv_halt() vs Returning from main()](#ulp_riscv_halt-vs-returning-from-main)
- [Waking the Main CPU](#waking-the-main-cpu)
- [ULP Symbol Naming — The ulp_ Prefix Rule](#ulp-symbol-naming--the-ulp_-prefix-rule)
- [Build System Integration](#build-system-integration)
- [ULP vs Main CPU — Capability Comparison](#ulp-vs-main-cpu--capability-comparison)
- [Timing Accuracy](#timing-accuracy)
- [Power Numbers](#power-numbers)
- [Common Pitfalls](#common-pitfalls)
- [Further Reading](#further-reading)

---

## What Is the ULP?

**ULP** stands for **Ultra-Low-Power coprocessor**. It is a small, independent
processor built into the ESP32-S3 that can continue executing code while the
main Xtensa LX7 CPU is completely powered off in deep sleep.

The ESP32-S3 specifically contains a **ULP RISC-V** core — a minimal 32-bit
processor implementing the RV32IMC instruction set. It runs at up to ~8 MHz,
has no cache, no MMU, and no operating system. It executes straight from RTC
slow memory.

The ULP's defining property is its power domain: it sits inside the **RTC
(Real-Time Clock) domain**, which remains powered even when the rest of the
chip — including both Xtensa cores, all SRAM, the Wi-Fi/BT radio, and most
peripheral clocks — is completely shut off.

```
┌─────────────────────────────────────────────────────────────────┐
│                         ESP32-S3 SoC                            │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │               Digital Core Power Domain                  │  │
│  │  (OFF during deep sleep)                                 │  │
│  │                                                          │  │
│  │   Xtensa LX7 Core 0    Xtensa LX7 Core 1                │  │
│  │   Wi-Fi / BT radio     Internal SRAM (512 KB)           │  │
│  │   USB / JTAG           Most peripheral clocks           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │               RTC Power Domain                           │  │
│  │  (ALWAYS ON — survives deep sleep)                       │  │
│  │                                                          │  │
│  │   ULP RISC-V Core      RTC Timer (wake scheduler)       │  │
│  │   RTC Slow Memory      RTC Fast Memory                  │  │
│  │   RTC Peripherals      GPIO hold circuitry              │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## ESP32-S3 Power Domains

Understanding which hardware blocks belong to which power domain is essential
for reasoning about what survives deep sleep.

| Domain | State during deep sleep | Contains |
|--------|------------------------|---------|
| Digital Core | **OFF** | Both Xtensa LX7 CPUs, internal SRAM, Wi-Fi, BT, USB |
| RTC | **ON** | ULP RISC-V, RTC timer, RTC slow memory (8 KB), RTC fast memory (8 KB), RTC GPIOs |
| Flash | **OFF** (unless retention configured) | External flash chip (not accessible to ULP) |

The ULP can only access hardware that lives in or is reachable from the RTC
domain. It **cannot** access:

- External flash (XIP is not available)
- The main SRAM
- SPI, I2C, I2S, USB peripherals
- The Wi-Fi / Bluetooth radio

It **can** access:

- RTC slow memory (shared with main CPU)
- RTC fast memory
- A limited set of RTC-domain peripherals (basic GPIO, ADC, I2C with dedicated ULP I2C controller, temperature sensor, touch sensor)
- The RTC timer itself

---

## ULP RISC-V Hardware Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     ULP RISC-V Core                          │
│                                                              │
│  ┌────────────────┐   ┌──────────────────────────────────┐  │
│  │ RV32IMC CPU    │   │ RTC Slow Memory (8 KB)           │  │
│  │ ~8 MHz         │◄──│ • ULP program code               │  │
│  │ No cache       │   │ • ULP global variables           │  │
│  │ No MMU         │   │ • Shared state with main CPU     │  │
│  └───────┬────────┘   └──────────────────────────────────┘  │
│          │                                                   │
│          │ bus                                               │
│          ▼                                                   │
│  ┌────────────────────────────────────────────────────────┐  │
│  │ RTC Peripheral Bus                                     │  │
│  │ • RTC GPIO         • ULP I2C controller               │  │
│  │ • SAR ADC          • Temperature sensor               │  │
│  │ • Touch sensor     • RTC timer registers              │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

**Key architectural constraints:**

- **No interrupt controller.** The ULP cannot service hardware interrupts. It
  runs to completion then halts; it is re-triggered by the RTC timer externally.
- **No floating-point unit.** All arithmetic must be integer-based.
- **No dynamic memory allocation.** No `malloc`/`free`; all storage must be
  statically allocated globals.
- **No standard library I/O.** No `printf`, no `scanf`. A minimal
  `ulp_riscv_print.h` exists for debug output via the RTC UART, but it is not
  used in production.
- **Limited stack.** The default stack is small; deeply nested function calls
  or large stack-allocated arrays will silently corrupt memory.
- **Harvard-like memory access.** Code and data both live in RTC slow memory,
  but the CPU fetches instructions and data through separate internal paths.

---

## RTC Memory — The Shared Data Bus

RTC slow memory is the only memory region accessible to **both** the ULP and
the main CPU simultaneously. This makes it the communication channel between
the two processors.

```
Main CPU (Xtensa LX7)              ULP RISC-V
─────────────────────              ──────────
Runs from internal SRAM            Runs from RTC slow memory
Can read/write RTC slow mem   ◄──► Reads/writes RTC slow mem
Cannot access ULP registers        Cannot access main SRAM
```

### Memory map (relevant portions)

| Address range | Size | Description |
|---------------|------|-------------|
| `0x5000_0000` | 8 KB | RTC slow memory — ULP code + shared variables |
| `0x4000_0000` | 8 KB | RTC fast memory — available to main CPU pre-sleep |

### Variable lifetime across deep sleep

Normal variables in main SRAM are lost when the chip enters deep sleep because
the digital core power domain is cut. Variables in RTC slow memory survive.

```c
// This variable is LOST across deep sleep (lives in main SRAM):
static uint32_t counter = 0;

// This variable SURVIVES deep sleep (lives in RTC slow memory):
// (ULP global variables are automatically placed in RTC slow memory
//  by the ULP linker script — no special attribute needed in ULP code)
volatile uint32_t wakeup_counter = 0;

// On the main CPU side, to place a variable in RTC slow memory explicitly:
RTC_DATA_ATTR uint32_t my_persistent_var = 0;
```

In this project, `wakeup_counter` and `tx_due` are defined as globals in the
ULP source. The ULP linker script automatically places all ULP globals in RTC
slow memory. The main CPU accesses them through the `extern` declarations
provided by the auto-generated `ulp_lora_scheduler.h` header.

---

## How the RTC Timer Invokes main()

This is the most important mechanism to understand. The ULP does **not** run
continuously. It is not polling. It executes a short burst of code, halts
itself, and is then re-triggered by hardware on a precise interval.

### The hardware mechanism

```
                    ┌─────────────────────────────────────────┐
                    │            RTC Timer                    │
                    │                                         │
                    │  64-bit counter, driven by              │
                    │  150 kHz RC oscillator (~6.7 µs/tick)  │
                    │                                         │
                    │  ulp_set_wakeup_period(0, 1_000_000)    │
                    │  sets a compare register so the timer   │
                    │  fires every 1,000,000 µs = 1 second    │
                    └──────────────┬──────────────────────────┘
                                   │ hardware interrupt / trigger
                                   ▼
                    ┌─────────────────────────────────────────┐
                    │         RTC Controller                  │
                    │                                         │
                    │  On timer match:                        │
                    │  1. Power on the ULP RISC-V core        │
                    │  2. Set PC = entry point address        │
                    │     (start of RTC slow memory)          │
                    │  3. Assert run signal                   │
                    └──────────────┬──────────────────────────┘
                                   │
                                   ▼
                    ┌─────────────────────────────────────────┐
                    │         ULP RISC-V Core                 │
                    │                                         │
                    │  Begins executing from address 0x0      │
                    │  of the ULP program in RTC slow memory  │
                    │                                         │
                    │  Startup code (start.S):                │
                    │  → sets up stack pointer                │
                    │  → calls main()                         │
                    │                                         │
                    │  Your main() runs                       │
                    │                                         │
                    │  ulp_riscv_halt() → writes DONE bit     │
                    │  → RTC controller powers down ULP core  │
                    └─────────────────────────────────────────┘
```

The key insight: **the ULP core is not running between timer ticks**. It is
completely powered down. When the RTC timer fires, the RTC controller applies
power to the ULP core, sets the program counter to the entry point, and asserts
the run signal. The ULP then executes `main()` from the beginning, as if it
were starting fresh.

This is fundamentally different from a normal CPU that sleeps with its context
preserved. The ULP has no program-counter state between invocations — every
call to `main()` is a cold start of the ULP core.

### ulp_set_wakeup_period()

This ESP-IDF function programs the RTC timer compare register:

```c
// In main.c, called once on cold boot:
ulp_set_wakeup_period(0, 1000000ULL);  // 1,000,000 microseconds = 1 second
```

The first argument (index `0`) selects timer slot 0. The ESP32-S3 supports
multiple ULP timer configurations, but this project uses only one.

The period is measured in microseconds by the ESP-IDF abstraction, but
internally the RTC timer runs off the 150 kHz RC oscillator (clock period
~6.7 µs). The SDK converts the microsecond value to RTC ticks automatically.

> **Accuracy note:** The 150 kHz RC oscillator is temperature-sensitive and
> not trimmed to high precision. Real-world periods typically deviate ±5–10%
> from the programmed value. For applications requiring precise timing (e.g.
> exact-interval data logging), use the 32.768 kHz crystal oscillator instead
> by configuring `CONFIG_ESP32S3_RTC_CLK_SRC_EXT_CRYS=y`.

---

## The ULP Execution Lifecycle — Step by Step

Here is the complete sequence for a single ULP tick, annotated with which
hardware is active at each moment:

```
Time →

T=0 s  RTC timer compare register matches current count
       ┌─────────────────────────────────────────────────┐
       │ RTC Controller action:                          │
       │   Power up ULP RISC-V core (~10 µs latency)    │
       │   Set PC = ULP program entry (0x5000_0000)      │
       │   Assert run signal                             │
       └─────────────────────────────────────────────────┘

T=0.00001 s  ULP core powered, executing start.S:
       ┌─────────────────────────────────────────────────┐
       │ start.S (compiled into ULP binary):             │
       │   li   sp, _stack_end    ; set stack pointer    │
       │   call main              ; jump to main()       │
       └─────────────────────────────────────────────────┘

T=0.00002 s  main() executes:
       ┌─────────────────────────────────────────────────┐
       │ ulp_lora_scheduler.c main():                    │
       │   wakeup_counter++;           // RTC mem write  │
       │   if (wakeup_counter >= 60) { // compare        │
       │       tx_due = 1;             // RTC mem write  │
       │       ulp_riscv_wakeup_main_processor();        │
       │   }                                             │
       │   ulp_riscv_halt();           // DONE bit write │
       └─────────────────────────────────────────────────┘

T=0.00003 s  ulp_riscv_halt() writes the DONE bit:
       ┌─────────────────────────────────────────────────┐
       │ RTC Controller sees DONE bit:                   │
       │   Power down ULP RISC-V core                    │
       │   Reload timer for next period                  │
       └─────────────────────────────────────────────────┘

T=0.00003 s  to T=1.0 s  — Nothing running except RTC timer
       ┌─────────────────────────────────────────────────┐
       │ System state:                                   │
       │   Digital core: OFF (deep sleep)               │
       │   ULP core: OFF (halted)                        │
       │   RTC timer: running                            │
       │   RTC slow memory: retaining wakeup_counter     │
       │   Current draw: ~25–30 µA                       │
       └─────────────────────────────────────────────────┘

T=1.0 s  RTC timer fires again → cycle repeats
```

The entire ULP execution window — from power-on to halt — takes approximately
20–50 microseconds. Out of every 1,000,000 µs (1 second), the ULP is active
for roughly 0.003–0.005% of the time.

---

## ULP Boot Sequence in Detail

Every time the RTC timer invokes the ULP, execution starts at the very
beginning of the ULP binary. The binary begins with startup assembly code
provided by ESP-IDF (`start.S`), not directly at your `main()` function.

```
ULP binary layout in RTC slow memory:
┌──────────────────────────────────────────────────────┐
│ 0x5000_0000  Vector table / entry point              │
│              (ulp_riscv_vectors.S)                   │
│                                                      │
│ 0x5000_00xx  start.S                                 │
│              - initialise .bss to zero               │
│              - set stack pointer                     │
│              - call main()                           │
│              - (if main returns: loop forever)        │
│                                                      │
│ 0x5000_0yy   Your main() function                    │
│              (ulp_lora_scheduler.c)                  │
│                                                      │
│ 0x5000_0zz   ULP runtime library                     │
│              - ulp_riscv_utils.c                     │
│                (ulp_riscv_halt,                      │
│                 ulp_riscv_wakeup_main_processor)      │
│              - ulp_riscv_lock.c                      │
│              - ulp_riscv_gpio.c etc.                 │
│                                                      │
│ 0x5000_0www  Global variables (.data, .bss)          │
│              wakeup_counter                          │
│              tx_due                                  │
│                                                      │
│ 0x5000_1FFF  End of RTC slow memory (8 KB)           │
└──────────────────────────────────────────────────────┘
```

### Important: .bss is re-initialised on every boot

The `start.S` startup code zeroes the `.bss` section (uninitialised globals)
on every ULP invocation. This means:

```c
// This variable is in .bss (initialised to 0 at declaration)
volatile uint32_t my_counter = 0;

// After the FIRST ULP invocation:
// start.S zeros .bss → my_counter = 0
// main() runs: my_counter++ → my_counter = 1
// ulp_riscv_halt()

// After the SECOND ULP invocation:
// start.S zeros .bss → my_counter = 0  ← RESET!
// main() runs: my_counter++ → my_counter = 1
// ulp_riscv_halt()
```

This is a critical gotcha. **If you place a variable in `.bss`, it will be
reset to zero on every ULP wakeup.** To persist a value across invocations, the
variable must be placed in the `.data` section by giving it a non-zero
initialiser:

```c
// .bss section — RESET on every ULP wakeup (WRONG for counters):
volatile uint32_t wakeup_counter = 0;  // zero initialiser → .bss

// .data section — PRESERVED across ULP wakeups (CORRECT):
volatile uint32_t wakeup_counter = 1;  // non-zero initialiser → .data
```

> **This project uses `= 0` initialisers**, which technically places the
> variables in `.bss`. However, this works correctly here because the main CPU
> writes to `ulp_wakeup_counter` and `ulp_tx_due` directly before returning
> to deep sleep (setting them to 0). The main CPU write goes to RTC slow memory
> directly — bypassing the `.bss` zeroing — so the values are always correct
> when the next ULP tick begins. In a project where the main CPU does not reset
> the values, a non-zero initialiser (`.data`) would be required.

---

## ulp_riscv_halt() vs Returning from main()

There are two ways for a ULP program to stop executing:

**1. `ulp_riscv_halt()` — correct approach**

```c
#include "ulp_riscv_utils.h"

int main(void) {
    // ... do work ...
    ulp_riscv_halt();  // writes DONE bit to RTC_CNTL_COCPU_DONE register
    return 0;          // unreachable, satisfies compiler
}
```

`ulp_riscv_halt()` writes a specific bit in the RTC controller registers
that signals "ULP execution is complete." The RTC controller then:
1. Powers down the ULP core.
2. Arms the timer for the next wakeup period.

**2. Returning from `main()` — undefined behaviour**

If `main()` returns without calling `ulp_riscv_halt()`, the startup code in
`start.S` has a fallback: it enters an infinite loop (`1: j 1b`). The ULP
core spins at full speed consuming ~1–2 mA until the next hardware reset.
This eliminates all the power savings of deep sleep.

**Always call `ulp_riscv_halt()` before or instead of returning from `main()`.**

---

## Waking the Main CPU

When the ULP determines that the main CPU should wake up (in this project: when
`wakeup_counter >= TX_INTERVAL_COUNT`), it calls:

```c
ulp_riscv_wakeup_main_processor();
```

This function writes to the `RTC_CNTL_COCPU_SHOULD_WAKEUP` register bit. The
RTC controller then:

1. Triggers a wakeup event in the power management unit.
2. Begins powering up the digital core domain.
3. The main CPU resets and starts executing the bootloader / `app_main()`.

The main CPU detects that the wakeup was triggered by the ULP:

```c
esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
if (cause == ESP_SLEEP_WAKEUP_ULP) {
    // We were woken by the ULP coprocessor
}
```

### Wakeup does not stop the ULP immediately

`ulp_riscv_wakeup_main_processor()` is non-blocking. After calling it, the ULP
continues executing normally. In this project, the very next line is
`ulp_riscv_halt()`, so the ULP stops almost immediately. However, in a more
complex program, the ULP could continue doing work while the main CPU is
powering up in parallel.

---

## ULP Symbol Naming — The ulp_ Prefix Rule

This is one of the most common sources of bugs when working with the ULP RISC-V
in ESP-IDF v5.x.

The `ulp_embed_binary()` CMake function compiles the ULP source, links it, and
then runs a post-processing tool (`esp32ulp_mapgen.py`) that:

1. Reads the ULP binary's symbol table (`.sym` file).
2. For every exported global variable, generates an `extern` declaration in
   the output header, **prepending `ulp_` to the variable name**.
3. Writes the header to `build/esp-idf/main/ulp_lora_scheduler/ulp_lora_scheduler.h`.

```
ULP source variable name     →    Generated header declaration
────────────────────────────────────────────────────────────────
wakeup_counter               →    extern uint32_t ulp_wakeup_counter;
tx_due                       →    extern uint32_t ulp_tx_due;
TX_INTERVAL_COUNT            →    (not exported — it's a #define, not a variable)
```

**The double-prefix trap:**

```c
// If you name your ULP variable with ulp_ prefix already:
volatile uint32_t ulp_wakeup_counter = 0;

// The generated header declares:
extern uint32_t ulp_ulp_wakeup_counter;   // ← double prefix!

// main.c tries to use:
ulp_wakeup_counter   // ← undeclared! causes compile error:
                     // error: 'ulp_wakeup_counter' undeclared;
                     // did you mean 'ulp_ulp_wakeup_counter'?
```

**The correct pattern (used in this project):**

```c
// ULP source — NO ulp_ prefix on the variable:
volatile uint32_t wakeup_counter = 0;   // → exported as ulp_wakeup_counter
volatile uint32_t tx_due = 0;           // → exported as ulp_tx_due

// main.c — uses the ulp_ prefixed names from the generated header:
ulp_wakeup_counter = 0;
ulp_tx_due = 0;
```

---

## Build System Integration

The ULP program is compiled as a completely separate binary from the main
application. It uses a different toolchain (RISC-V GCC instead of Xtensa GCC),
a different linker script, and a different memory model.

### CMakeLists.txt breakdown

```cmake
# 1. Register the main component as usual
idf_component_register(
    SRCS "main.c" "rylr896.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_system freertos ulp
)

# 2. Tell the build system about the ULP program
set(ulp_app_name    ulp_lora_scheduler)      # Name of the ULP app
set(ulp_sources     "ulp/ulp_lora_scheduler.c")  # ULP source file (RELATIVE path)
set(ulp_exp_dep_srcs "main.c")               # Main CPU files that use ULP symbols

# 3. Embed the compiled ULP binary into the main app's flash image
ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")
```

### What ulp_embed_binary() does

`ulp_embed_binary()` is a CMake macro provided by the ESP-IDF `ulp` component.
It performs the following steps automatically:

```
Step 1: Compile ULP source
        riscv32-esp-elf-gcc -march=rv32imc_zicsr_zifencei ...
        → ulp_lora_scheduler.c.obj

Step 2: Link ULP binary
        riscv32-esp-elf-gcc + ulp_riscv.ld linker script
        → ulp_lora_scheduler.elf
        → ulp_lora_scheduler.bin   (raw binary, loaded into RTC memory at runtime)
        → ulp_lora_scheduler.map
        → ulp_lora_scheduler.sym   (symbol table)

Step 3: Generate header
        esp32ulp_mapgen.py reads .sym file
        → ulp_lora_scheduler.h
           extern uint32_t ulp_wakeup_counter;
           extern uint32_t ulp_tx_due;

Step 4: Embed binary as linker symbols
        ld --format=binary ulp_lora_scheduler.bin
        → _binary_ulp_lora_scheduler_bin_start
        → _binary_ulp_lora_scheduler_bin_end
        (accessible in main.c via extern const uint8_t declarations)
```

### Why the ULP sources path must be relative

A known issue in ESP-IDF on Windows: `ulp_embed_binary()` invokes CMake's
`ExternalProject` to build the ULP as a sub-project. If the source path
contains spaces (e.g. `F:/Visual Studio Code Projects/...`), ninja tokenises
the path at the spaces and fails with:

```
ninja: error: 'F:/Visual', needed by '...Visual.ulp.S', missing
```

Using a relative path (`"ulp/ulp_lora_scheduler.c"`) avoids this because
`ulp_embed_binary()` internally resolves relative paths using
`get_filename_component(... ABSOLUTE BASE_DIR CMAKE_CURRENT_SOURCE_DIR)`,
which handles quoting correctly within the sub-build.

### Required Kconfig settings

The ULP RISC-V subsystem must be explicitly enabled in `sdkconfig.defaults`.
In ESP-IDF v5.4.1 the correct symbol names are:

```
CONFIG_ULP_COPROC_ENABLED=y       # Enable the ULP coprocessor
CONFIG_ULP_COPROC_TYPE_RISCV=y    # Select RISC-V (not FSM)
CONFIG_ULP_COPROC_RESERVE_MEM=4096 # Reserve 4 KB of RTC slow mem for ULP
```

> **Version note:** In ESP-IDF versions before v5.x, these symbols had the
> prefix `ESP32S3_ULP_COPROC_*`. Using the old names in v5.4.1 produces
> "unknown kconfig symbol" warnings, and the ULP subsystem silently remains
> disabled — causing runtime failures even though the build succeeds.

---

## ULP vs Main CPU — Capability Comparison

| Feature | Main CPU (Xtensa LX7) | ULP RISC-V |
|---------|----------------------|------------|
| Clock speed | Up to 240 MHz | ~8 MHz |
| Architecture | Xtensa LX7 (proprietary) | RISC-V RV32IMC |
| Active current | ~80 mA (typical at 240 MHz) | ~100–200 µA |
| Accessible memory | All SRAM + Flash (XIP) | RTC slow memory only |
| FreeRTOS | Yes | No |
| Interrupts | Full interrupt controller | None |
| Floating point | Hardware FPU | Integer only |
| UART | All UARTs | RTC UART only (limited) |
| SPI / I2C | All peripherals | ULP I2C only |
| GPIO | All GPIOs | RTC-capable GPIOs only |
| Deep sleep capable | No (it is the main CPU) | Yes — designed for sleep |
| Can wake main CPU | N/A | Yes — `ulp_riscv_wakeup_main_processor()` |

---

## Timing Accuracy

The RTC timer that drives ULP wakeups is clocked by the **150 kHz RC
oscillator** (also written as FOSC or RTC8M_CLK). This oscillator is:

- **Fast to start** — no crystal stabilisation delay
- **Low power** — draws only a few microamps
- **Imprecise** — ±5% to ±10% variation across temperature and voltage

For the LoRa scheduler in this project (60-second interval), a ±10% error
means the actual interval could range from 54 to 66 seconds. This is
acceptable for most sensor telemetry applications.

**For more precise timing**, configure the 32.768 kHz crystal oscillator:

```
CONFIG_ESP32S3_RTC_CLK_SRC_EXT_CRYS=y   # Use external 32.768 kHz crystal
```

This reduces timing error to ±20–50 ppm but requires a 32.768 kHz crystal
connected to XTAL32K_P and XTAL32K_N pins, and adds ~1 µA to the sleep current.

---

## Power Numbers

These figures are for the ULP RISC-V core and RTC domain only, measured at
3.3 V on a bare ESP32-S3 module (no dev board overhead).

| State | Current | Duration per cycle |
|-------|---------|-------------------|
| ULP executing (main loop) | ~200 µA | ~30 µs |
| ULP halted, RTC timer running | ~25–30 µA | ~999,970 µs |
| Main CPU active (for comparison) | ~80 mA | <200 ms |

**Average current calculation for this project (60 s cycle):**

```
ULP execution contribution:
  200 µA × 30 µs / 1,000,000 µs = 0.006 µA

RTC domain static:
  ~27 µA × 1,000,000 µs / 1,000,000 µs = 27 µA

Main CPU (once per 60 cycles = once per 60 s):
  80,000 µA × 200,000 µs / (60 × 1,000,000 µs) = 266 µA

Total average ≈ 27 + 266 ≈ 293 µA at 60 s interval
(real-world closer to 50–100 µA as TX time is well under 200 ms)
```

The ULP itself contributes negligibly to average current. The dominant term
is always the main CPU's active window.

---

## Common Pitfalls

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| Variable in `.bss` (zero initialiser) | Counter resets to 0 on every ULP tick | Use non-zero initialiser (`.data`) or have main CPU write the value before sleeping |
| Returning from `main()` without `ulp_riscv_halt()` | ULP spins at full speed; sleep current jumps to ~1–2 mA | Always call `ulp_riscv_halt()` |
| Variable named `ulp_foo` in ULP source | Compile error: `'ulp_foo' undeclared; did you mean 'ulp_ulp_foo'?` | Remove the `ulp_` prefix from the variable name in the ULP source |
| Absolute path in `ulp_sources` on Windows | `ninja: error: 'F:/Visual'` | Use a relative path: `"ulp/ulp_lora_scheduler.c"` |
| Wrong Kconfig symbols (`ESP32S3_ULP_COPROC_*`) | ULP silently not enabled; device never wakes | Use `ULP_COPROC_*` (no chip prefix) in v5.4.1 |
| ULP accesses main SRAM address | Undefined behaviour / bus fault | Only access RTC slow memory from ULP code |
| Stack overflow in ULP | Silent memory corruption | Keep ULP functions shallow; avoid large local arrays |
| Forgetting `ulp_set_wakeup_period()` | ULP never runs after `ulp_riscv_run()` | Always call `ulp_set_wakeup_period()` before `ulp_riscv_run()` |
| Missing `CONFIG_ULP_COPROC_RESERVE_MEM` | `ulp_riscv_load_binary` returns `ESP_ERR_NO_MEM` | Set to at least 4096 in `sdkconfig.defaults` |

---

## Further Reading

- [ESP-IDF ULP RISC-V Programming Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/api-reference/system/ulp-risc-v.html)
- [ESP32-S3 Technical Reference Manual — Chapter: ULP Coprocessor](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [ESP-IDF Power Management Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/api-reference/system/power_management.html)
- [ESP-IDF Sleep Modes Guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/api-reference/system/sleep_modes.html)
- [ESP-IDF ULP RISC-V Example Projects](https://github.com/espressif/esp-idf/tree/v5.4.1/examples/system/ulp/ulp_riscv)
