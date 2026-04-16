# REYAX RYLR896 — Module Reference Guide

This document covers the RYLR896 LoRa module hardware, AT command interface,
radio configuration, and how it is used in this project.

---

## Table of Contents

- [Overview](#overview)
- [Key Specifications](#key-specifications)
- [Pin Description](#pin-description)
- [Communication Interface](#communication-interface)
- [AT Command Reference](#at-command-reference)
- [Radio Configuration](#radio-configuration)
- [Packet Format](#packet-format)
- [Receiving Data](#receiving-data)
- [Power Considerations](#power-considerations)
- [How This Project Uses the RYLR896](#how-this-project-uses-the-rylr896)
- [Factory Reset](#factory-reset)
- [Regulatory Notes](#regulatory-notes)

---

## Overview

The REYAX RYLR896 is a compact, low-power LoRa transceiver module based on the
**Semtech SX1276** chipset. It communicates with a host microcontroller over a
standard UART serial interface using simple ASCII AT commands — no SPI, no
register-level programming, no LoRaWAN stack required.

The module handles all RF layer concerns (modulation, CRC, preamble, whitening)
internally. From the host microcontroller's perspective, sending a LoRa packet
is as simple as:

```
AT+SEND=0,12,Hello World!\r\n
```

This makes the RYLR896 well suited for point-to-point and star-topology sensor
networks where simplicity and low power are priorities.

---

## Key Specifications

| Parameter | Value |
|-----------|-------|
| RF chipset | Semtech SX1276 |
| Frequency bands | 433 MHz / 868 MHz / 915 MHz (variant-dependent) |
| Modulation | LoRa (CSS) |
| Max TX power | +22 dBm |
| Receive sensitivity | –148 dBm (SF12, BW125) |
| UART interface | 115200 baud, 8N1 |
| Supply voltage | 2.5 V – 3.6 V (3.3 V nominal) |
| TX current | ~120 mA at +22 dBm |
| RX current | ~14 mA |
| Sleep current | ~1 µA (module only, AT+MODE=1) |
| Max payload | 240 bytes per packet |
| Dimensions | 16 mm × 16 mm × 1 mm |

> **Variant selection:** The RYLR896 is sold in 433 MHz, 868 MHz, and 915 MHz
> variants. The hardware physically supports the specified band only. Check the
> label on your module before setting `AT+BAND`.

---

## Pin Description

| Pin | Name | Direction | Description |
|:---:|------|:---------:|-------------|
| 1 | GND | — | Ground |
| 2 | VCC | Input | 2.5 V – 3.6 V supply |
| 3 | TXD | Output | UART TX (data from module to host) |
| 4 | RXD | Input | UART RX (data from host to module) |
| 5 | NRST | Input | Active-low hardware reset; pull high via 10 kΩ if unused |
| 6 | ANT | — | RF antenna connection (50 Ω) |

> **NRST:** Leave this pin floating and the module may reset unexpectedly on
> power-up due to noise. Always tie it high through a 10 kΩ pull-up resistor
> unless you intend to use it for a hardware reset.

---

## Communication Interface

The RYLR896 communicates over a 3.3 V UART at **115200 baud, 8 data bits, no
parity, 1 stop bit (8N1)**. Hardware flow control is not used.

All commands are terminated with `\r\n` (CR LF). All responses from the module
end with `\r\n`.

### Response types

| Response | Meaning |
|----------|---------|
| `+OK` | Command executed successfully |
| `+OK=<value>` | Command executed; value returned |
| `+ERR=<n>` | Command failed; error code `n` |
| `+RCV=<addr>,<len>,<data>,<RSSI>,<SNR>` | Unsolicited: packet received |

### Error codes

| Code | Meaning |
|------|---------|
| 1 | AT command syntax error |
| 2 | Invalid parameter value |
| 3 | Module busy transmitting |
| 4 | Message too long (>240 bytes) |
| 5 | Invalid address |

---

## AT Command Reference

### AT — Ping / connection test

Check that the module is alive and responding.

```
Command:  AT\r\n
Response: +OK\r\n
```

### AT+RESET — Software reset

Restarts the module. All configuration reverts to the values stored in
non-volatile memory (which persist across resets).

```
Command:  AT+RESET\r\n
Response: +RESET\r\n
          +READY\r\n
```

### AT+ADDRESS — Set or query device address

Each node on the network must have a unique address. Range: 0–65535.

```
# Set address to 5
Command:  AT+ADDRESS=5\r\n
Response: +OK\r\n

# Query current address
Command:  AT+ADDRESS?\r\n
Response: +ADDRESS=5\r\n
```

### AT+NETWORKID — Set or query network ID

Only nodes with the same network ID can communicate. Range: 0–16 (values 3–15
are reserved by LoRaWAN; use 0–2 or 16–18 for private networks).

```
# Set network ID to 18
Command:  AT+NETWORKID=18\r\n
Response: +OK\r\n

# Query
Command:  AT+NETWORKID?\r\n
Response: +NETWORKID=18\r\n
```

### AT+BAND — Set or query RF frequency

The frequency must match the hardware variant of your module.

```
# Set to 915 MHz (Americas)
Command:  AT+BAND=915000000\r\n
Response: +OK\r\n

# Set to 868 MHz (Europe)
Command:  AT+BAND=868000000\r\n
Response: +OK\r\n

# Set to 433 MHz
Command:  AT+BAND=433000000\r\n
Response: +OK\r\n

# Query
Command:  AT+BAND?\r\n
Response: +BAND=915000000\r\n
```

### AT+CRFOP — Set TX output power

Range: 0–22 (dBm). Default is 22 dBm (maximum).

```
# Set to 14 dBm
Command:  AT+CRFOP=14\r\n
Response: +OK\r\n

# Query
Command:  AT+CRFOP?\r\n
Response: +CRFOP=22\r\n
```

### AT+PARAMETER — Set spreading factor, bandwidth, coding rate, preamble

```
Command:  AT+PARAMETER=<SF>,<BW>,<CR>,<Preamble>\r\n
Response: +OK\r\n
```

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| SF | 7–12 | 9 | Spreading Factor. Higher = longer range, slower data rate |
| BW | 7–9 | 7 | Bandwidth: 7=125 kHz, 8=250 kHz, 9=500 kHz |
| CR | 1–4 | 1 | Coding Rate: 1=4/5, 2=4/6, 3=4/7, 4=4/8 |
| Preamble | 4–7 | 12 | Preamble length in symbols |

Example — maximum range configuration:
```
AT+PARAMETER=12,7,4,7\r\n
```

Example — faster data rate (shorter range):
```
AT+PARAMETER=7,9,1,4\r\n
```

### AT+SEND — Transmit a data packet

```
Command:  AT+SEND=<dest_addr>,<length>,<data>\r\n
Response: +OK\r\n
```

| Field | Description |
|-------|-------------|
| `dest_addr` | Destination address (0 = broadcast to all nodes on the network) |
| `length` | Number of bytes in `data` (must be accurate; max 240) |
| `data` | ASCII payload |

Example — send "SENSOR:PKT#42" to address 0 (broadcast):
```
AT+SEND=0,13,SENSOR:PKT#42\r\n
+OK\r\n
```

Example — send "TEMP=23.5" to node at address 7:
```
AT+SEND=7,9,TEMP=23.5\r\n
+OK\r\n
```

### AT+MODE — Sleep mode

```
# Enter sleep mode (~1 µA)
Command:  AT+MODE=1\r\n
Response: +OK\r\n

# Return to normal mode
Command:  AT+MODE=0\r\n
Response: +OK\r\n
```

> **Note on sleep mode in this project:** This project does not use `AT+MODE=1`.
> Instead, the entire ESP32-S3 enters deep sleep and the RYLR896 is simply left
> idle (not actively transmitting or receiving). The module's idle receive
> current (~14 mA) is acceptable because the active window is so short (<200 ms).
> For duty cycles longer than a few minutes, consider adding `AT+MODE=1` before
> the ESP32-S3 enters deep sleep and `AT+MODE=0` after wakeup.

### AT+IPR — Change UART baud rate

```
# Change to 9600 baud
Command:  AT+IPR=9600\r\n
Response: +OK\r\n
```

> Stored to non-volatile memory. You must update your host UART driver to the
> new baud rate after issuing this command.

### AT+UID — Query unique ID

```
Command:  AT+UID?\r\n
Response: +UID=XXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n
```

---

## Radio Configuration

### Spreading Factor and Range

| SF | Approx. range (open air) | Data rate | Use case |
|----|--------------------------|-----------|----------|
| SF7 | ~2 km | Fastest | Short range, frequent updates |
| SF9 | ~5 km | Medium | Default — balanced range/speed |
| SF12 | ~15 km | Slowest | Maximum range, infrequent data |

Higher spreading factors increase time-on-air and therefore battery consumption
per packet. SF12 at 125 kHz BW takes ~2 seconds to send a small payload; SF7
takes ~50 ms for the same payload.

### Duty Cycle Regulations

In regions with duty cycle restrictions (EU 868 MHz: 1% duty cycle on most
sub-bands), long time-on-air at high SF combined with frequent transmissions
can violate regulatory limits. The default 60-second interval with SF9 is safe
for all regions, but verify if you change these parameters.

### Network Architecture

The RYLR896 supports point-to-point and simple star topologies natively:

```
Star topology (this project's typical use case):

  Node 1 (addr=1) ──┐
  Node 2 (addr=2) ──┤──► Gateway (addr=0, receives all broadcasts)
  Node 3 (addr=3) ──┘

All nodes send AT+SEND=0,... (broadcast)
Gateway listens and receives +RCV=<addr>,<len>,<data>,<RSSI>,<SNR>
```

---

## Packet Format

### Transmitted packet structure (handled internally by RYLR896)

```
┌──────────┬────────┬────────┬──────────┬─────────┬─────────┐
│ Preamble │  Sync  │ Header │  Source  │  Data   │   CRC   │
│ (symbols)│  Word  │        │  Address │(payload)│         │
└──────────┴────────┴────────┴──────────┴─────────┴─────────┘
           ← All managed by SX1276 chipset / RYLR896 firmware →
```

The host only sees the payload bytes. All framing, CRC generation/checking,
and address filtering are handled by the module.

### Received packet (unsolicited output from RYLR896)

When a node receives a packet addressed to it (or a broadcast), the module
outputs:

```
+RCV=<source_addr>,<length>,<data>,<RSSI>,<SNR>\r\n
```

Example:
```
+RCV=2,13,SENSOR:PKT#120,-72,8\r\n
```

| Field | Value | Meaning |
|-------|-------|---------|
| source_addr | 2 | Packet came from node address 2 |
| length | 13 | Payload is 13 bytes |
| data | SENSOR:PKT#120 | The actual payload |
| RSSI | -72 | Received signal strength in dBm |
| SNR | 8 | Signal-to-noise ratio in dB |

**RSSI guide:**

| RSSI | Link quality |
|------|-------------|
| > −70 dBm | Excellent |
| −70 to −100 dBm | Good |
| −100 to −120 dBm | Fair |
| < −120 dBm | Poor / edge of range |

---

## Receiving Data

This project is transmit-only. To add a receiver (e.g. a gateway), create a
second ESP32-S3 + RYLR896 system that:

1. Initialises the RYLR896 with the same `NETWORKID` and `BAND`.
2. Sets its own unique `ADDRESS` (e.g. address 0 for the gateway, since
   `AT+SEND=0,...` broadcasts to address 0).
3. Reads the UART continuously for `+RCV=...` lines.

Example receiver pseudo-code:
```c
// In a FreeRTOS task or polling loop:
char buf[256];
int len = uart_read_bytes(UART_NUM_1, buf, sizeof(buf)-1, pdMS_TO_TICKS(100));
if (len > 0) {
    buf[len] = '\0';
    if (strstr(buf, "+RCV=")) {
        // Parse: +RCV=<addr>,<len>,<data>,<rssi>,<snr>
        int src, plen, rssi, snr;
        char data[241];
        sscanf(buf, "+RCV=%d,%d,%240[^,],%d,%d", &src, &plen, data, &rssi, &snr);
        printf("From node %d: %s  (RSSI=%d dBm, SNR=%d dB)\n", src, data, rssi, snr);
    }
}
```

---

## Power Considerations

| Operating mode | Module current | Notes |
|----------------|---------------|-------|
| Transmitting (+22 dBm) | ~120 mA | Burst only; duration depends on SF |
| Receiving | ~14 mA | Continuous listening |
| Idle (standby) | ~1.5 mA | UART on, radio standby |
| Sleep (`AT+MODE=1`) | ~1 µA | UART off; wake with NRST or AT+MODE=0 |

In this project the module is in **idle/standby** mode during the ESP32-S3's
deep sleep. If your application has very long sleep intervals (>5 minutes),
adding `AT+MODE=1` before sleep and `AT+MODE=0` after wakeup will reduce the
module's contribution to average current to near zero.

---

## How This Project Uses the RYLR896

The driver in `main/rylr896.c` interacts with the module in this sequence on
every wakeup:

```
ESP32-S3                          RYLR896
────────                          ───────
uart_driver_install()
vTaskDelay(100 ms)          ←── module settle time

AT\r\n                      ──►
                            ◄── +OK\r\n

AT+ADDRESS=1\r\n            ──►
                            ◄── +OK\r\n

AT+NETWORKID=18\r\n         ──►
                            ◄── +OK\r\n

AT+BAND=915000000\r\n       ──►
                            ◄── +OK\r\n

AT+SEND=0,14,SENSOR:PKT#60  ──►
                            ◄── +OK\r\n
                                (RF packet transmitted over the air)

uart_driver_delete()
esp_deep_sleep_start()
```

The AT ping and configuration commands are re-sent on every wakeup. This is
intentional: it ensures the module is always in the correct state even after
an unexpected power interruption.

### Configuration constants in `main/rylr896.h`

```c
#define RYLR896_UART_PORT    UART_NUM_1      // ESP-IDF UART port
#define RYLR896_TX_PIN       17              // ESP32-S3 GPIO → RYLR896 RX
#define RYLR896_RX_PIN       18              // ESP32-S3 GPIO ← RYLR896 TX
#define RYLR896_BAUD_RATE    115200          // Factory default baud rate
#define RYLR896_ADDRESS      1               // This node's address
#define RYLR896_NETWORK_ID   18              // Network group ID
#define RYLR896_BAND_HZ      915000000       // RF frequency (915 MHz)
#define RYLR896_DEST_ADDR    0               // Broadcast destination
#define RYLR896_CMD_TIMEOUT_MS  3000         // AT response timeout
```

---

## Factory Reset

If the module is in an unknown configuration state, restore factory defaults:

```
AT+FACTORY\r\n
+FACTORY\r\n
+READY\r\n
```

Factory defaults:

| Parameter | Factory value |
|-----------|--------------|
| ADDRESS | 0 |
| NETWORKID | 18 |
| BAND | 915000000 Hz |
| CRFOP (TX power) | 22 dBm |
| PARAMETER (SF,BW,CR,PRE) | 9,7,1,12 |
| IPR (baud rate) | 115200 |
| MODE | 0 (normal) |

---

## Regulatory Notes

The RYLR896 operates in unlicensed ISM bands. Ensure your deployment complies
with local regulations:

| Region | Band | Max EIRP | Duty cycle |
|--------|------|----------|------------|
| Americas (FCC Part 15) | 902–928 MHz | 30 dBm | No limit (frequency hopping rules apply) |
| Europe (ETSI EN 300 220) | 863–870 MHz | 14–27 dBm | 0.1%–1% depending on sub-band |
| Australia (ACMA) | 915–928 MHz | 30 dBm | No limit |
| Japan | 920–928 MHz | 20 dBm | 1% |

> Always verify current regulations with your national radio authority before
> deploying in a commercial or public product.
