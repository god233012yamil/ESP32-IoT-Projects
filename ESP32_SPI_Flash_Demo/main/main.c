/**
 * @file main.c
 * @brief ESP32 (ESP-IDF v5+) SPI demo: Read JEDEC ID, Slow Read (0x03), Fast Read (0x0B with dummy),
 *        DMA-friendly bulk read, and Write/Erase (WREN 0x06, Page Program 0x02, optional Sector Erase 0x20).
 *
 * This example shows:
 *   1) Initializing SPI3_HOST (legacy VSPI) and adding a W25Q32-like SPI flash device.
 *   2) Reading JEDEC ID using 0x9F.
 *   3) Reading data using 0x03 (slow read, no dummy).
 *   4) Reading data using 0x0B (fast read) with dummy cycles via spi_transaction_ext_t.
 *   5) DMA-friendly bulk reads (bigger max_transfer_sz + DMA-capable buffers).
 *   6) Write/erase flow: Write Enable (0x06), Page Program (0x02), optional Sector Erase (0x20),
 *      and status polling with 0x05 (WIP bit).
 *
 * Wiring (example):
 *   - ESP32 GPIO23  -> W25Q32 MOSI (DI)
 *   - ESP32 GPIO19  -> W25Q32 MISO (DO)
 *   - ESP32 GPIO18  -> W25Q32 SCLK (CLK)
 *   - ESP32 GPIO5   -> W25Q32 CS   (CS#)
 *   - 3.3V          -> VCC
 *   - GND           -> GND
 *
 * Build:
 *   idf.py set-target esp32
 *   idf.py build flash monitor
 *
 * Notes:
 *   - Page size is typically 256 bytes; page program must not cross page boundaries.
 *   - Sector erase shown is 4KB (0x20). Use with caution; it will erase data.
 *   - For simplicity, this demo uses blocking transactions.
 *   - If you prefer higher throughput, consider queued transactions and reusing TX/RX buffers.
 */

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>   // memcpy, memset
#include <stdlib.h>   // malloc, free
#include <inttypes.h> // PRIu32, PRIx32

/* ---------- User Pin Mapping (adjust as needed) ---------- */
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS    5

/* ---------- Flash Command Opcodes ---------- */
#define CMD_READ_ID        0x9F   /*!< JEDEC ID: Manufacturer, MemoryType, Capacity */
#define CMD_RDSR1          0x05   /*!< Read Status Register-1 (WIP bit is bit0) */
#define CMD_WREN           0x06   /*!< Write Enable */
#define CMD_READ_DATA      0x03   /*!< Slow Read (no dummy) */
#define CMD_FAST_READ      0x0B   /*!< Fast Read (requires dummy cycles) */
#define CMD_PAGE_PROGRAM   0x02   /*!< Page Program (up to 256 bytes, no crossing page boundary) */
#define CMD_SECTOR_ERASE   0x20   /*!< Sector Erase 4KB (optional; destructive) */

/* ---------- Device Characteristics (common for W25Qxx) ---------- */
#define FLASH_PAGE_SIZE        256U
#define FLASH_SECTOR_SIZE      4096U
#define FAST_READ_DUMMY_BITS   8      /* 8 dummy bits (1 dummy byte) for 0x0B on many chips */

/* ---------- Logging ---------- */
static const char *TAG = "SPI_Flash";

/* ---------- SPI Device Handle ---------- */
static spi_device_handle_t g_spi = NULL;

/**
 * @brief Initialize the SPI bus and add the external flash device.
 *
 * Sets up SPI3_HOST (VSPI equivalent in ESP-IDF v5.x) with a larger max_transfer_sz
 * to support DMA-friendly bulk transfers. Adds the flash device at a modest clock.
 * This configuration is suitable for common SPI flash parts (e.g., W25Q32 family).
 *
 * @note On ESP-IDF v4.x, replace SPI3_HOST with VSPI_HOST.
 * @note Ensure wiring matches the pin mapping above.
 *
 * @return void
 */
static void spi_flash_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32 * 1024,   // Larger for DMA-friendly bulk reads
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000, // 8 MHz (raise once stable)
        .mode = 0,                         // Mode 0 (CPOL=0, CPHA=0)
        .spics_io_num = PIN_NUM_CS,        // CS pin
        .queue_size = 4,
        // Fast read is half-duplex (cmd+addr then read). This flag can help, but is optional.
        .flags = SPI_DEVICE_HALFDUPLEX,
        .command_bits = 0,   // use per-transaction sizes
        .address_bits = 0,   // use per-transaction sizes
    };

    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &g_spi));
    ESP_LOGI(TAG, "SPI Flash device initialized on SPI3_HOST (VSPI).");
}

/**
 * @brief Read the JEDEC ID (0x9F) and log it.
 *
 * Sends 0x9F and reads 3 ID bytes (Manufacturer, MemoryType, Capacity).
 * Many devices place these bytes after the first returned position; thus we log rx[1..3].
 *
 * @return void
 */
static void spi_flash_read_id(void)
{
    uint8_t tx[4] = { CMD_READ_ID, 0x00, 0x00, 0x00 };
    uint8_t rx[4] = { 0 };

    spi_transaction_t t = {0};
    t.length    = 8 * sizeof(tx);
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    ESP_ERROR_CHECK(spi_device_transmit(g_spi, &t));
    ESP_LOGI(TAG, "JEDEC ID: %02X %02X %02X", rx[1], rx[2], rx[3]);
}

/**
 * @brief Read 'length' bytes using slow read (0x03), no dummy cycles.
 *
 * Builds a TX buffer containing CMD + 24-bit address, followed by zero-dummy bytes to
 * clock out the RX payload. Performs a single blocking transaction and copies out data.
 *
 * @param address  24-bit start address in flash.
 * @param data     Output buffer (must be non-NULL).
 * @param length   Number of bytes to read (must be > 0).
 *
 * @retval ESP_OK on success; data filled.
 * @retval ESP_ERR_INVALID_ARG on bad args.
 * @retval ESP_ERR_NO_MEM if allocation failed.
 * @retval esp_err_t underlying SPI error.
 */
static esp_err_t spi_flash_read_slow(uint32_t address, uint8_t *data, size_t length)
{
    if (!data || length == 0) return ESP_ERR_INVALID_ARG;

    const size_t kCmdLen = 4; // 0x03 + 24-bit address
    const size_t total = kCmdLen + length;

    uint8_t *tx = (uint8_t *)malloc(total);
    uint8_t *rx = (uint8_t *)malloc(total);
    if (!tx || !rx) {
        free(tx); free(rx);
        return ESP_ERR_NO_MEM;
    }

    tx[0] = CMD_READ_DATA;
    tx[1] = (address >> 16) & 0xFF;
    tx[2] = (address >> 8)  & 0xFF;
    tx[3] =  address        & 0xFF;
    memset(tx + kCmdLen, 0x00, length);
    memset(rx, 0x00, total);

    spi_transaction_t t = {0};
    t.length    = 8 * total;
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(g_spi, &t);
    if (err == ESP_OK) memcpy(data, rx + kCmdLen, length);

    free(tx); free(rx);
    return err;
}

/**
 * @brief Read 'length' bytes using fast read (0x0B) with dummy cycles.
 *
 * Uses spi_transaction_ext_t to specify variable command, address, and dummy bits.
 * This performs a half-duplex transaction: send CMD+ADDR+(dummy), then read data.
 *
 * @param address  24-bit start address in flash.
 * @param data     Output buffer (must be non-NULL).
 * @param length   Number of bytes to read (must be > 0).
 * @param dummy_bits Number of dummy bits (often 8 for 0x0B).
 *
 * @retval ESP_OK on success; data filled.
 * @retval ESP_ERR_INVALID_ARG on bad args.
 * @retval esp_err_t underlying SPI error.
 */
static esp_err_t spi_flash_read_fast(uint32_t address, uint8_t *data, size_t length, uint8_t dummy_bits)
{
    if (!data || length == 0) return ESP_ERR_INVALID_ARG;

    spi_transaction_ext_t t = {0};

    // Tell the driver we provide cmd/addr/dummy sizes per transaction
    t.base.flags    = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    t.base.length   = 8 * length;   // number of data bits
    t.base.rxlength = 8 * length;   // number of bits to read
    t.base.rx_buffer = data;        // read directly into 'data'

    // Per-transaction sizing
    t.command_bits = 8;             // 8-bit command
    t.address_bits = 24;            // 24-bit address
    t.dummy_bits   = dummy_bits;    // usually 8 for 0x0B fast read

    // In this ESP-IDF layout, cmd/addr are on the base struct:
    t.base.cmd  = CMD_FAST_READ;
    t.base.addr = address & 0x00FFFFFFu;

    return spi_device_transmit(g_spi, &t.base);
}

/**
 * @brief DMA-friendly bulk read loop using fast read (0x0B) and large transfers.
 *
 * Splits the read into chunks that fit into the device's configured max_transfer_sz.
 * Allocates DMA-capable buffers (if needed) and reads directly into 'out' using
 * fast read with dummy cycles. Uses blocking transactions in a loop for simplicity.
 *
 * @param address    Start address.
 * @param out        Output buffer (must be non-NULL).
 * @param length     Total number of bytes to read.
 * @param chunk_max  Max payload per transaction (<= device/bus limit).
 *
 * @retval ESP_OK on full success.
 * @retval esp_err_t on first failure encountered.
 */
static esp_err_t spi_flash_read_bulk_dma(uint32_t address, uint8_t *out, size_t length, size_t chunk_max)
{
    if (!out || length == 0) return ESP_ERR_INVALID_ARG;

    // Guard chunk size (leave room if you switch to TX/RX buffers)
    if (chunk_max == 0) chunk_max = 16 * 1024;

    size_t remaining = length;
    uint32_t curr = address;
    uint8_t *dst = out;

    while (remaining > 0) {
        size_t this_len = remaining > chunk_max ? chunk_max : remaining;

        esp_err_t err = spi_flash_read_fast(curr, dst, this_len, FAST_READ_DUMMY_BITS);
        if (err != ESP_OK) return err;

        curr += this_len;
        dst  += this_len;
        remaining -= this_len;
    }
    return ESP_OK;
}

/**
 * @brief Issue Write Enable (0x06) to set the WEL bit before program/erase.
 *
 * @retval ESP_OK on success.
 * @retval esp_err_t on SPI error.
 */
static esp_err_t spi_flash_write_enable(void)
{
    uint8_t cmd = CMD_WREN;
    spi_transaction_t t = {0};
    t.length    = 8;
    t.tx_buffer = &cmd;
    return spi_device_transmit(g_spi, &t);
}

/**
 * @brief Read Status Register-1 (0x05) and return it in 'status'.
 *
 * @param status  Out parameter for status register value.
 *
 * @retval ESP_OK on success.
 * @retval esp_err_t on SPI error.
 */
static esp_err_t spi_flash_read_status1(uint8_t *status)
{
    if (!status) return ESP_ERR_INVALID_ARG;

    uint8_t tx[2] = { CMD_RDSR1, 0x00 };
    uint8_t rx[2] = { 0 };

    spi_transaction_t t = {0};
    t.length    = 8 * sizeof(tx);
    t.tx_buffer = tx;
    t.rx_buffer = rx;

    esp_err_t err = spi_device_transmit(g_spi, &t);
    if (err == ESP_OK) *status = rx[1];
    return err;
}

/**
 * @brief Wait until flash is ready (WIP=0) with timeout.
 *
 * Polls Status Register-1 WIP bit (bit0) until it clears or timeout happens.
 *
 * @param timeout_ms  Maximum time to wait in milliseconds.
 *
 * @retval ESP_OK if ready within timeout.
 * @retval ESP_ERR_TIMEOUT if still busy after timeout.
 * @retval esp_err_t on SPI error.
 */
static esp_err_t spi_flash_wait_ready(uint32_t timeout_ms)
{
    const TickType_t start = xTaskGetTickCount();
    const TickType_t to_ticks = pdMS_TO_TICKS(timeout_ms);

    for (;;) {
        uint8_t sr1 = 0;
        esp_err_t err = spi_flash_read_status1(&sr1);
        if (err != ESP_OK) return err;

        if ((sr1 & 0x01) == 0) return ESP_OK; // WIP == 0 => ready

        if ((xTaskGetTickCount() - start) > to_ticks) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Page Program (0x02) up to 256 bytes at 'address' (must not cross page boundary).
 *
 * Caller must ensure:
 *  - Length <= 256.
 *  - Address and length do not cross a 256-byte page boundary.
 *
 * @param address  24-bit destination address.
 * @param data     Pointer to data to write.
 * @param length   Number of bytes to write (<= 256).
 *
 * @retval ESP_OK on success.
 * @retval ESP_ERR_INVALID_ARG for bad inputs.
 * @retval ESP_ERR_NO_MEM if allocation failed.
 * @retval esp_err_t underlying SPI error.
 */
static esp_err_t spi_flash_page_program(uint32_t address, const uint8_t *data, size_t length)
{
    if (!data || length == 0 || length > FLASH_PAGE_SIZE) return ESP_ERR_INVALID_ARG;

    // Check boundary
    uint32_t page_off = address & (FLASH_PAGE_SIZE - 1);
    if (page_off + length > FLASH_PAGE_SIZE) return ESP_ERR_INVALID_ARG;

    ESP_RETURN_ON_ERROR(spi_flash_write_enable(), TAG, "WREN failed");

    const size_t kHdr = 4; // 0x02 + 24-bit address
    size_t total = kHdr + length;

    uint8_t *tx = (uint8_t *)heap_caps_malloc(total, MALLOC_CAP_DMA);
    if (!tx) return ESP_ERR_NO_MEM;

    tx[0] = CMD_PAGE_PROGRAM;
    tx[1] = (address >> 16) & 0xFF;
    tx[2] = (address >> 8)  & 0xFF;
    tx[3] =  address        & 0xFF;
    memcpy(tx + kHdr, data, length);

    spi_transaction_t t = {0};
    t.length    = 8 * total;
    t.tx_buffer = tx;

    esp_err_t err = spi_device_transmit(g_spi, &t);
    free(tx);
    if (err != ESP_OK) return err;

    // Wait until program completes
    return spi_flash_wait_ready(300); // 300 ms is usually plenty for a single page
}

/**
 * @brief Program an arbitrary-length buffer by splitting into page-sized chunks.
 *
 * Splits the write into 256-byte pages, respecting page boundaries. For each page:
 *   WREN -> PAGE PROGRAM -> wait ready.
 *
 * @param address  Start address.
 * @param data     Input buffer.
 * @param length   Number of bytes to program.
 *
 * @retval ESP_OK on full success.
 * @retval esp_err_t on failure at any step.
 */
static esp_err_t spi_flash_write_buffer(uint32_t address, const uint8_t *data, size_t length)
{
    if (!data || length == 0) return ESP_ERR_INVALID_ARG;

    size_t remaining = length;
    const uint8_t *src = data;
    uint32_t addr = address;

    while (remaining > 0) {
        uint32_t page_off = addr & (FLASH_PAGE_SIZE - 1);
        size_t chunk = FLASH_PAGE_SIZE - page_off;
        if (chunk > remaining) chunk = remaining;

        esp_err_t err = spi_flash_page_program(addr, src, chunk);
        if (err != ESP_OK) return err;

        addr += chunk;
        src  += chunk;
        remaining -= chunk;
    }
    return ESP_OK;
}

/**
 * @brief (Optional) Sector Erase 4KB (0x20) at 'address' (sector-aligned recommended).
 *
 * @param address  Address within the sector to erase (commonly aligned to 4KB).
 *
 * @retval ESP_OK on success.
 * @retval esp_err_t on SPI or timeout errors.
 */
static esp_err_t spi_flash_sector_erase(uint32_t address)
{
    ESP_RETURN_ON_ERROR(spi_flash_write_enable(), TAG, "WREN failed");

    uint8_t tx[4] = {
        CMD_SECTOR_ERASE,
        (uint8_t)((address >> 16) & 0xFF),
        (uint8_t)((address >> 8)  & 0xFF),
        (uint8_t)( address        & 0xFF),
    };

    spi_transaction_t t = {0};
    t.length    = 8 * sizeof(tx);
    t.tx_buffer = tx;

    ESP_RETURN_ON_ERROR(spi_device_transmit(g_spi, &t), TAG, "Erase tx failed");
    return spi_flash_wait_ready(4000); // Sector erase can take milliseconds
}

/**
 * @brief Demo entry: init bus/device, read ID, slow read, fast read, DMA bulk read,
 *        (optional) erase+program+verify flow.
 *
 * @return void
 */
void app_main(void)
{
    // Initialize SPI bus and add flash device to the bus
    spi_flash_init();

    // --- JEDEC ID ---
    spi_flash_read_id();

    // --- Slow Read (0x03) 16 bytes @ 0x000000 ---
    uint8_t slow_buf[16] = {0};
    ESP_ERROR_CHECK(spi_flash_read_slow(0x000000, slow_buf, sizeof(slow_buf)));
    ESP_LOGI(TAG, "Slow Read 0x03 @0x000000:");
    for (size_t i = 0; i < sizeof(slow_buf); ++i) printf("%02X ", slow_buf[i]);
    printf("\n");

    // --- Fast Read (0x0B) 16 bytes @ 0x000000 ---
    uint8_t fast_buf[16] = {0};
    ESP_ERROR_CHECK(spi_flash_read_fast(0x000000, fast_buf, sizeof(fast_buf), FAST_READ_DUMMY_BITS));
    ESP_LOGI(TAG, "Fast Read 0x0B @0x000000:");
    for (size_t i = 0; i < sizeof(fast_buf); ++i) printf("%02X ", fast_buf[i]);
    printf("\n");

    // --- DMA-friendly bulk read (0x0B) 1 KiB @ 0x000000 ---
    enum { BULK_LEN = 1024 };
    uint8_t *bulk = (uint8_t *)heap_caps_malloc(BULK_LEN, MALLOC_CAP_DMA);
    configASSERT(bulk != NULL);
    memset(bulk, 0, BULK_LEN);
    ESP_ERROR_CHECK(spi_flash_read_bulk_dma(0x000000, bulk, BULK_LEN, 16 * 1024));
    ESP_LOGI(TAG, "Bulk fast read 1 KiB done (showing first 32 bytes):");
    for (size_t i = 0; i < 32; ++i) printf("%02X ", bulk[i]);
    printf("\n");

    // ===== OPTIONAL: ERASE + PROGRAM + VERIFY DEMO =====
    // WARNING: This erases a 4KB sector. Pick a known-safe offset on your chip!
    const uint32_t demo_addr = 0x001000; // Choose a sector you can safely modify
    ESP_LOGW(TAG, "Erasing 4KB sector at 0x%06" PRIx32 " (demo)", demo_addr);
    ESP_ERROR_CHECK(spi_flash_sector_erase(demo_addr));

    // Prepare test pattern
    uint8_t pattern[FLASH_PAGE_SIZE];
    for (size_t i = 0; i < sizeof(pattern); ++i) pattern[i] = (uint8_t)i;

    // Program one page
    ESP_LOGI(TAG, "Programming one page (%u bytes) at 0x%06" PRIx32, (unsigned)sizeof(pattern), demo_addr);
    ESP_ERROR_CHECK(spi_flash_write_buffer(demo_addr, pattern, sizeof(pattern)));

    // Verify
    uint8_t verify[FLASH_PAGE_SIZE] = {0};
    ESP_ERROR_CHECK(spi_flash_read_fast(demo_addr, verify, sizeof(verify), FAST_READ_DUMMY_BITS));

    bool ok = (memcmp(pattern, verify, sizeof(pattern)) == 0);
    ESP_LOGI(TAG, "Verify %s", ok ? "OK ✅" : "FAILED ❌");

    free(bulk);
}