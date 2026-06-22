#include "ssd1306.h"

#include <string.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#define SSD1306_CONTROL_COMMAND 0x00
#define SSD1306_CONTROL_DATA    0x40
#define SSD1306_TIMEOUT_MS      200
#define SSD1306_PAGE_COUNT      8

static const char *TAG = "ssd1306";

/**
 * @brief Sends a sequence of command bytes to the display.
 *
 * @param display Initialized display context.
 * @param commands Command buffer.
 * @param command_count Number of command bytes.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
static esp_err_t write_commands(const ssd1306_t *display,
                                const uint8_t *commands,
                                size_t command_count)
{
    ESP_RETURN_ON_FALSE(display != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Display context is NULL");
    ESP_RETURN_ON_FALSE(commands != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Command buffer is NULL");

    uint8_t buffer[32];
    ESP_RETURN_ON_FALSE(command_count <= (sizeof(buffer) - 1),
                        ESP_ERR_INVALID_SIZE,
                        TAG,
                        "Command sequence is too large");

    buffer[0] = SSD1306_CONTROL_COMMAND;
    memcpy(&buffer[1], commands, command_count);

    return i2c_master_transmit(display->device_handle,
                               buffer,
                               command_count + 1,
                               SSD1306_TIMEOUT_MS);
}

/**
 * @brief Sends one display page from the framebuffer.
 *
 * @param display Initialized display context.
 * @param page Page number from 0 through 7.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
static esp_err_t write_page(const ssd1306_t *display, uint8_t page)
{
    uint8_t page_buffer[SSD1306_WIDTH + 1];
    page_buffer[0] = SSD1306_CONTROL_DATA;
    memcpy(&page_buffer[1],
           &display->framebuffer[(size_t)page * SSD1306_WIDTH],
           SSD1306_WIDTH);

    const uint8_t page_commands[] = {
        (uint8_t)(0xB0U | page),
        0x00,
        0x10,
    };

    ESP_RETURN_ON_ERROR(write_commands(display,
                                       page_commands,
                                       sizeof(page_commands)),
                        TAG,
                        "Failed to select SSD1306 page");

    return i2c_master_transmit(display->device_handle,
                               page_buffer,
                               sizeof(page_buffer),
                               SSD1306_TIMEOUT_MS);
}

/**
 * @brief Initializes the SSD1306 display.
 *
 * @param display Initialized display context.
 * @param address The 7-bit I2C address of the display.
 * @param scl_speed_hz The SCL speed in Hertz.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t ssd1306_init(ssd1306_t *display,
                       uint8_t address,
                       uint32_t scl_speed_hz)
{
    ESP_RETURN_ON_FALSE(display != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Display context is NULL");

    memset(display, 0, sizeof(*display));
    ESP_RETURN_ON_ERROR(app_i2c_bus_add_device(address,
                                                scl_speed_hz,
                                                &display->device_handle),
                        TAG,
                        "Failed to add SSD1306 device");

    display->address = address;

    // Initialization sequence for a common 128x64 panel using internal charge pump.
    const uint8_t init_commands[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0x7F,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0x2E,
        0xAF,
    };

    const esp_err_t result = write_commands(display,
                                             init_commands,
                                             sizeof(init_commands));
    if (result != ESP_OK) {
        app_i2c_bus_remove_device(display->device_handle);
        memset(display, 0, sizeof(*display));
        return result;
    }

    display->initialized = true;
    ssd1306_clear(display);

    // Give the panel a short interval to stabilize after display-on.
    vTaskDelay(pdMS_TO_TICKS(20));
    return ssd1306_refresh(display);
}

/**
 * @brief Clears the SSD1306 display.
 *
 * @param display Initialized display context.
 */
void ssd1306_clear(ssd1306_t *display)
{
    if (display == NULL) {
        return;
    }

    memset(display->framebuffer, 0, sizeof(display->framebuffer));
}

/**
 * @brief Draws a pixel on the SSD1306 display.
 *
 * @param display Initialized display context.
 * @param x The x-coordinate of the pixel.
 * @param y The y-coordinate of the pixel.
 * @param enabled Whether the pixel is enabled.
 */
void ssd1306_draw_pixel(ssd1306_t *display,
                        uint8_t x,
                        uint8_t y,
                        bool enabled)
{
    if ((display == NULL) ||
        (x >= SSD1306_WIDTH) ||
        (y >= SSD1306_HEIGHT)) {
        return;
    }

    const size_t index = (size_t)x + ((size_t)(y / 8U) * SSD1306_WIDTH);
    const uint8_t mask = (uint8_t)(1U << (y % 8U));

    if (enabled) {
        display->framebuffer[index] |= mask;
    } else {
        display->framebuffer[index] &= (uint8_t)~mask;
    }
}

/**
 * @brief Draws a demo pattern on the SSD1306 display.
 *
 * @param display Initialized display context.
 */
void ssd1306_draw_demo_pattern(ssd1306_t *display)
{
    if (display == NULL) {
        return;
    }

    ssd1306_clear(display);

    // Draw a rectangular border around the display.
    for (uint8_t x = 0; x < SSD1306_WIDTH; ++x) {
        ssd1306_draw_pixel(display, x, 0, true);
        ssd1306_draw_pixel(display, x, SSD1306_HEIGHT - 1, true);
    }

    for (uint8_t y = 0; y < SSD1306_HEIGHT; ++y) {
        ssd1306_draw_pixel(display, 0, y, true);
        ssd1306_draw_pixel(display, SSD1306_WIDTH - 1, y, true);
    }

    // Draw diagonals to make orientation and pixel addressing easy to verify.
    for (uint8_t x = 0; x < SSD1306_HEIGHT; ++x) {
        ssd1306_draw_pixel(display, x + 16, x, true);
        ssd1306_draw_pixel(display, 111 - x, x, true);
    }

    // Draw a small checkerboard area in the center.
    for (uint8_t y = 24; y < 40; ++y) {
        for (uint8_t x = 48; x < 80; ++x) {
            if (((x + y) & 1U) == 0U) {
                ssd1306_draw_pixel(display, x, y, true);
            }
        }
    }
}

/**
 * @brief Refreshes the SSD1306 display.
 *
 * @param display Initialized display context.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t ssd1306_refresh(const ssd1306_t *display)
{
    ESP_RETURN_ON_FALSE(display != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Display context is NULL");
    ESP_RETURN_ON_FALSE(display->initialized,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "SSD1306 is not initialized");

    for (uint8_t page = 0; page < SSD1306_PAGE_COUNT; ++page) {
        ESP_RETURN_ON_ERROR(write_page(display, page),
                            TAG,
                            "Failed to update SSD1306 page %u",
                            page);
    }

    return ESP_OK;
}

/**
 * @brief Deinitializes the SSD1306 display.
 *
 * @param display Initialized display context.
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t ssd1306_deinit(ssd1306_t *display)
{
    ESP_RETURN_ON_FALSE(display != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Display context is NULL");

    if (!display->initialized) {
        return ESP_OK;
    }

    const uint8_t display_off = 0xAE;
    const esp_err_t command_result = write_commands(display,
                                                     &display_off,
                                                     sizeof(display_off));
    if (command_result != ESP_OK) {
        return command_result;
    }

    ESP_RETURN_ON_ERROR(app_i2c_bus_remove_device(display->device_handle),
                        TAG,
                        "Failed to remove SSD1306 device");

    memset(display, 0, sizeof(*display));
    return ESP_OK;
}
