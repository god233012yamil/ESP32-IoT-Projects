#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_FRAMEBUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

/**
 * @brief SSD1306 display context for a 128x64 monochrome panel.
 */
typedef struct {
    i2c_master_dev_handle_t device_handle;
    uint8_t address;
    uint8_t framebuffer[SSD1306_FRAMEBUFFER_SIZE];
    bool initialized;
} ssd1306_t;

/**
 * @brief Initializes a 128x64 SSD1306 OLED display.
 *
 * @param display Driver context to initialize.
 * @param address Seven-bit display address, usually 0x3C or 0x3D.
 * @param scl_speed_hz I2C clock speed in hertz.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t ssd1306_init(ssd1306_t *display,
                       uint8_t address,
                       uint32_t scl_speed_hz);

/**
 * @brief Clears the local framebuffer.
 *
 * @param display Initialized display context.
 */
void ssd1306_clear(ssd1306_t *display);

/**
 * @brief Draws one pixel in the local framebuffer.
 *
 * @param display Initialized display context.
 * @param x Horizontal coordinate from 0 to 127.
 * @param y Vertical coordinate from 0 to 63.
 * @param enabled True to set the pixel, false to clear it.
 */
void ssd1306_draw_pixel(ssd1306_t *display,
                        uint8_t x,
                        uint8_t y,
                        bool enabled);

/**
 * @brief Draws a simple demonstration pattern in the framebuffer.
 *
 * @param display Initialized display context.
 */
void ssd1306_draw_demo_pattern(ssd1306_t *display);

/**
 * @brief Sends the complete framebuffer to the display.
 *
 * @param display Initialized display context.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t ssd1306_refresh(const ssd1306_t *display);

/**
 * @brief Releases the SSD1306 device handle.
 *
 * @param display Driver context to deinitialize.
 *
 * @return ESP_OK on success. Otherwise, an ESP-IDF error code is returned.
 */
esp_err_t ssd1306_deinit(ssd1306_t *display);

#endif
