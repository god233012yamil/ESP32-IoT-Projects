# ESP32 SPI Flash Demo

A comprehensive ESP32 (ESP-IDF v5+) demonstration project showing how to interface with external SPI flash memory devices using the ESP-IDF SPI Master driver. This project demonstrates various SPI flash operations including JEDEC ID reading, slow/fast read operations, DMA-friendly bulk transfers, and write/erase functionality.

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring Diagram](#wiring-diagram)
- [Software Requirements](#software-requirements)
- [Project Structure](#project-structure)
- [Building and Flashing](#building-and-flashing)
- [SPI Flash Operations](#spi-flash-operations)
  - [JEDEC ID Reading](#jedec-id-reading)
  - [Slow Read (0x03)](#slow-read-0x03)
  - [Fast Read (0x0B)](#fast-read-0x0b)
  - [DMA Bulk Read](#dma-bulk-read)
  - [Write Operations](#write-operations)
  - [Erase Operations](#erase-operations)
- [Configuration](#configuration)
- [Code Overview](#code-overview)
- [Usage Examples](#usage-examples)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## Features

- ✅ **JEDEC ID Reading** - Device identification using 0x9F command
- ✅ **Slow Read (0x03)** - Basic read operation without dummy cycles
- ✅ **Fast Read (0x0B)** - High-speed read with configurable dummy cycles
- ✅ **DMA-Friendly Bulk Reads** - Large data transfers with DMA support
- ✅ **Page Programming (0x02)** - Write data with automatic page boundary handling
- ✅ **Sector Erase (0x20)** - 4KB sector erase functionality
- ✅ **Write Enable/Status Polling** - Proper write/erase flow control
- ✅ **Error Handling** - Comprehensive error checking and logging
- ✅ **Memory Safety** - DMA-capable buffer allocation and proper cleanup

## Hardware Requirements

### ESP32 Development Board
- Any ESP32 development board (ESP32-DevKitC, ESP32-WROOM, etc.)
- ESP-IDF v5.0 or later support

### SPI Flash Memory
- **Recommended**: W25Q32 or compatible SPI flash (W25Qxx series)
- **Interface**: Standard SPI (MOSI, MISO, SCLK, CS)
- **Voltage**: 3.3V compatible
- **Capacity**: Any size (demo uses first few KB)

### Additional Components
- Breadboard or PCB for connections
- Jumper wires
- 3.3V power supply (usually from ESP32)

## Wiring Diagram

Connect your SPI flash to the ESP32 as follows:

```
ESP32 Pin    │ SPI Flash Pin │ Function
─────────────┼───────────────┼──────────
GPIO23       │ DI (MOSI)     │ Data In
GPIO19       │ DO (MISO)     │ Data Out  
GPIO18       │ CLK           │ Clock
GPIO5        │ CS#           │ Chip Select
3.3V         │ VCC           │ Power
GND          │ GND           │ Ground
```

> **Note**: Pin assignments can be modified in the `PIN_NUM_*` defines at the top of `main.c`

## Software Requirements

- **ESP-IDF**: Version 5.0 or later
- **Python**: 3.6+ (for ESP-IDF tools)
- **Git**: For cloning the repository

### ESP-IDF Installation

If you haven't installed ESP-IDF yet:

1. Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. Install ESP-IDF v5.0 or later
3. Set up the environment variables

## Project Structure

```
esp32-spi-flash-demo/
├── main/
│   └── main.c              # Main application code
├── CMakeLists.txt          # Build configuration
├── sdkconfig               # ESP-IDF configuration
└── README.md               # This file
```

## Building and Flashing

1. **Clone the repository**:
   ```bash
   git clone https://github.com/god233012yamil/esp32-spi-flash-demo.git
   cd esp32-spi-flash-demo
   ```

2. **Set ESP32 as target**:
   ```bash
   idf.py set-target esp32
   ```

3. **Configure the project** (optional):
   ```bash
   idf.py menuconfig
   ```

4. **Build the project**:
   ```bash
   idf.py build
   ```

5. **Flash and monitor**:
   ```bash
   idf.py flash monitor
   ```

6. **Exit monitor**: Press `Ctrl+]`

## SPI Flash Operations

### JEDEC ID Reading

The JEDEC ID operation (0x9F) reads three identification bytes:
- **Manufacturer ID**: Identifies the flash manufacturer
- **Memory Type**: Specifies the memory type  
- **Capacity**: Indicates the flash capacity

```c
static void spi_flash_read_id(void)
```

### Slow Read (0x03)

Basic read operation without dummy cycles, suitable for lower clock speeds:
- Command: 0x03
- Address: 24-bit
- No dummy cycles required
- Sequential data output

```c
static esp_err_t spi_flash_read_slow(uint32_t address, uint8_t *data, size_t length)
```

### Fast Read (0x0B)

High-speed read operation with dummy cycles for better performance:
- Command: 0x0B  
- Address: 24-bit
- Dummy cycles: 8 bits (configurable)
- Uses `spi_transaction_ext_t` for precise timing control

```c
static esp_err_t spi_flash_read_fast(uint32_t address, uint8_t *data, size_t length, uint8_t dummy_bits)
```

### DMA Bulk Read

Optimized for large data transfers using DMA:
- Chunked reading for large datasets
- DMA-capable buffer allocation
- Configurable chunk sizes
- Efficient memory usage

```c
static esp_err_t spi_flash_read_bulk_dma(uint32_t address, uint8_t *out, size_t length, size_t chunk_max)
```

### Write Operations

**Page Programming (0x02)**:
- Maximum 256 bytes per operation
- Cannot cross page boundaries
- Requires Write Enable (0x06) before programming
- Automatic status polling for completion

```c
static esp_err_t spi_flash_page_program(uint32_t address, const uint8_t *data, size_t length)
static esp_err_t spi_flash_write_buffer(uint32_t address, const uint8_t *data, size_t length)
```

### Erase Operations

**Sector Erase (0x20)**:
- Erases 4KB sectors
- Requires Write Enable (0x06)
- Status polling for completion
- ⚠️ **Destructive operation** - use with caution

```c
static esp_err_t spi_flash_sector_erase(uint32_t address)
```

## Configuration

### Pin Configuration
Modify the pin assignments in `main.c`:
```c
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23  
#define PIN_NUM_CLK  18
#define PIN_NUM_CS    5
```

### SPI Configuration
Adjust SPI parameters:
```c
spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 8 * 1000 * 1000, // 8 MHz
    .mode = 0,                         // SPI Mode 0
    .spics_io_num = PIN_NUM_CS,
    .queue_size = 4,
    .flags = SPI_DEVICE_HALFDUPLEX,
};
```

### Flash Parameters
Configure flash-specific settings:
```c
#define FLASH_PAGE_SIZE        256U     // Page size in bytes
#define FLASH_SECTOR_SIZE      4096U    // Sector size in bytes  
#define FAST_READ_DUMMY_BITS   8        // Dummy bits for fast read
```

## Code Overview

### Key Functions

| Function | Purpose |
|----------|---------|
| `spi_flash_init()` | Initialize SPI bus and device |
| `spi_flash_read_id()` | Read JEDEC ID |
| `spi_flash_read_slow()` | Slow read operation |
| `spi_flash_read_fast()` | Fast read with dummy cycles |
| `spi_flash_read_bulk_dma()` | DMA bulk read |
| `spi_flash_write_enable()` | Enable write operations |
| `spi_flash_page_program()` | Program single page |
| `spi_flash_write_buffer()` | Write arbitrary length data |
| `spi_flash_sector_erase()` | Erase 4KB sector |
| `spi_flash_wait_ready()` | Poll for operation completion |

### Memory Management

- Uses `heap_caps_malloc(MALLOC_CAP_DMA)` for DMA-compatible buffers
- Proper error handling with cleanup
- Memory safety with bounds checking

## Usage Examples

### Basic Read Operation
```c
uint8_t data[256];
esp_err_t err = spi_flash_read_fast(0x000000, data, sizeof(data), 8);
if (err == ESP_OK) {
    // Process data
}
```

### Write and Verify
```c
// Erase sector first
esp_err_t err = spi_flash_sector_erase(0x001000);
if (err == ESP_OK) {
    // Write data
    uint8_t write_data[256] = {/* your data */};
    err = spi_flash_write_buffer(0x001000, write_data, sizeof(write_data));
    
    // Verify
    uint8_t read_data[256];
    err = spi_flash_read_fast(0x001000, read_data, sizeof(read_data), 8);
    // Compare write_data with read_data
}
```

## Troubleshooting

### Common Issues

**1. JEDEC ID reads as 0xFF 0xFF 0xFF**
- Check wiring connections
- Verify power supply (3.3V)
- Ensure flash chip is properly seated

**2. Read data is incorrect**
- Verify SPI mode (usually Mode 0)
- Check clock speed (try reducing to 1MHz)
- Confirm dummy bit configuration for fast read

**3. Write operations fail**  
- Ensure Write Enable (0x06) is sent before writes
- Check that sector is erased before programming
- Verify address alignment and page boundaries

**4. DMA errors**
- Use `MALLOC_CAP_DMA` for buffer allocation
- Ensure buffer addresses are word-aligned
- Check max_transfer_sz configuration

### Debug Tips

- Enable ESP-IDF SPI debug logs:
  ```c
  esp_log_level_set("spi_master", ESP_LOG_DEBUG);
  ```
- Use logic analyzer to verify SPI timing
- Start with lower clock speeds (1-2 MHz)
- Test with known good flash chips first

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Areas for Contribution

- Support for additional flash types (QSPI, different manufacturers)
- Performance optimizations
- Additional erase modes (block erase, chip erase)
- Flash file system integration
- Unit tests and CI/CD

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**⚠️ Important Notes:**
- This demo includes **destructive erase operations** that will modify flash content
- Always backup important data before testing
- Test on known-safe memory regions first
- Some flash devices may have different command sets - consult your datasheet

**📧 Questions?** Open an issue or discussion on GitHub!
