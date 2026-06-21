# ESP32 UART Beginner Example

This ESP-IDF project accompanies the article "ESP32 UART: A Beginner's Guide to Reliable Serial Communication with ESP-IDF."

The application configures a dedicated hardware UART, receives newline-terminated commands, echoes terminal input, and sends text responses. UART0 remains available for flashing and ESP-IDF logging.

## Features

- Dedicated UART1 interface by default
- Configurable UART controller, GPIO pins, baud rate, and buffers
- FreeRTOS receive task
- CR, LF, and CRLF line-ending support
- Backspace handling
- Input overflow protection
- Interactive commands: `HELP`, `PING`, `STATUS`, `ECHO`, and `CLEAR`
- Google-style function documentation and beginner-focused comments

## Default Wiring

| ESP32 signal | Default GPIO | External device |
|---|---:|---|
| TX | GPIO17 | RX |
| RX | GPIO16 | TX |
| GND | GND | GND |

Both devices must use 3.3 V-compatible UART logic. Do not connect RS-232 voltage levels or a 5 V TX signal directly to an ESP32 GPIO.

## Terminal Settings

- Baud rate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

Because this example uses a second UART, connect it through a 3.3 V USB-to-UART adapter or another MCU. Keep the regular ESP-IDF USB/UART connection attached for flashing and logs.

## Build and Flash

Open an ESP-IDF terminal in the project directory and run:

```text
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the programming port used by the ESP32 board.

Open a second serial terminal for the external USB-to-UART adapter using the configured application baud rate.

## Configuration

Run:

```text
idf.py menuconfig
```

Then open:

```text
UART Beginner Example Configuration
```

The following options can be changed:

- UART controller number
- TX GPIO
- RX GPIO
- Baud rate
- RX ring buffer size
- TX ring buffer size

Pin availability differs between ESP32-family devices and development boards. Select GPIOs that are valid for your target and are not already used by flash, PSRAM, USB, or board peripherals.

## Commands

```text
HELP
PING
STATUS
ECHO Hello ESP32
CLEAR
```

Expected examples:

```text
> PING
PONG

> ECHO Hello ESP32
Hello ESP32
```

## Project Structure

```text
esp32_uart_beginner/
|-- CMakeLists.txt
|-- README.md
|-- sdkconfig.defaults
`-- main/
    |-- CMakeLists.txt
    |-- Kconfig.projbuild
    `-- uart_app.c
```
