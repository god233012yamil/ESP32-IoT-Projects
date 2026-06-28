#ifndef APP_RGB_LED_H
#define APP_RGB_LED_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_RGB_LED_COLOR_OFF = 0,
    APP_RGB_LED_COLOR_RED,
    APP_RGB_LED_COLOR_GREEN,
    APP_RGB_LED_COLOR_BLUE,
    APP_RGB_LED_COLOR_WHITE,
    APP_RGB_LED_COLOR_YELLOW,
    APP_RGB_LED_COLOR_CYAN,
    APP_RGB_LED_COLOR_MAGENTA,
    APP_RGB_LED_COLOR_ORANGE,
    APP_RGB_LED_COLOR_PURPLE,
} app_rgb_led_color_t;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} app_rgb_led_rgb_t;

typedef struct {
    int gpio_num;
    uint32_t led_count;
    uint8_t brightness;
    bool enable_dma;
} app_rgb_led_config_t;

/**
 * @brief Initializes the addressable RGB LED driver.
 *
 * Args:
 *     config: Pointer to the RGB LED configuration structure.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_ARG if the configuration is invalid.
 *     ESP_ERR_INVALID_STATE if the driver has already been initialized.
 *     Another ESP-IDF error code if the LED strip driver cannot be created.
 */
esp_err_t app_rgb_led_init(const app_rgb_led_config_t *config);

/**
 * @brief Releases the addressable RGB LED driver resources.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 */
esp_err_t app_rgb_led_deinit(void);

/**
 * @brief Sets the global brightness used by the RGB LED driver.
 *
 * Args:
 *     brightness: Brightness value from 0 to 255.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 */
esp_err_t app_rgb_led_set_brightness(uint8_t brightness);

/**
 * @brief Gets the current global brightness value.
 *
 * Returns:
 *     Brightness value from 0 to 255.
 */
uint8_t app_rgb_led_get_brightness(void);

/**
 * @brief Sets one LED pixel using RGB values and refreshes the strip.
 *
 * Args:
 *     index: LED index starting from 0.
 *     red: Red intensity from 0 to 255.
 *     green: Green intensity from 0 to 255.
 *     blue: Blue intensity from 0 to 255.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_ARG if the LED index is outside the configured range.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 *     Another ESP-IDF error code if the LED strip update fails.
 */
esp_err_t app_rgb_led_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Sets one LED pixel using an RGB structure and refreshes the strip.
 *
 * Args:
 *     index: LED index starting from 0.
 *     color: RGB color structure.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_ARG if the LED index is outside the configured range.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 *     Another ESP-IDF error code if the LED strip update fails.
 */
esp_err_t app_rgb_led_set_pixel_rgb(uint32_t index, app_rgb_led_rgb_t color);

/**
 * @brief Sets the first LED to a predefined color and refreshes the strip.
 *
 * Args:
 *     color: Predefined RGB LED color.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 *     Another ESP-IDF error code if the LED strip update fails.
 */
esp_err_t app_rgb_led_set_color(app_rgb_led_color_t color);

/**
 * @brief Sets all LEDs to the same RGB color and refreshes the strip once.
 *
 * Args:
 *     red: Red intensity from 0 to 255.
 *     green: Green intensity from 0 to 255.
 *     blue: Blue intensity from 0 to 255.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 *     Another ESP-IDF error code if the LED strip update fails.
 */
esp_err_t app_rgb_led_fill(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Turns off all LEDs and refreshes the strip.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 *     Another ESP-IDF error code if the LED strip update fails.
 */
esp_err_t app_rgb_led_clear(void);

/**
 * @brief Refreshes the physical LED strip using the current pixel buffer.
 *
 * Returns:
 *     ESP_OK on success.
 *     ESP_ERR_INVALID_STATE if the driver was not initialized.
 *     Another ESP-IDF error code if the LED strip refresh fails.
 */
esp_err_t app_rgb_led_refresh(void);

/**
 * @brief Converts a predefined color into RGB values.
 *
 * Args:
 *     color: Predefined RGB LED color.
 *
 * Returns:
 *     RGB color structure.
 */
app_rgb_led_rgb_t app_rgb_led_color_to_rgb(app_rgb_led_color_t color);

#ifdef __cplusplus
}
#endif

#endif
