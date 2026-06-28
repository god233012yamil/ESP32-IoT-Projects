#include "app_rgb_led.h"

#include "esp_check.h"
#include "led_strip.h"

static const char *TAG = "app_rgb_led";

static led_strip_handle_t s_strip;
static uint32_t s_led_count;
static uint8_t s_brightness = 32;

/**
 * @brief Scales an 8-bit color channel by the configured brightness value.
 *
 * Args:
 *     value: Original color channel value from 0 to 255.
 *
 * Returns:
 *     Brightness-scaled color channel value from 0 to 255.
 */
static uint8_t scale_channel(uint8_t value)
{
    return (uint8_t)(((uint16_t)value * s_brightness) / 255U);
}

/**
 * @brief Checks whether the RGB LED driver is initialized.
 *
 * Returns:
 *     True if the driver is initialized, otherwise false.
 */
static bool is_initialized(void)
{
    return s_strip != NULL;
}

/**
 * @brief Initializes the RGB LED driver.
 *
 * Args:
 *     config: Pointer to the configuration structure.
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_init(const app_rgb_led_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Configuration is NULL");
    ESP_RETURN_ON_FALSE(config->led_count > 0, ESP_ERR_INVALID_ARG, TAG, "LED count must be greater than zero");
    ESP_RETURN_ON_FALSE(!is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Driver already initialized");

    led_strip_config_t strip_config = {
        .strip_gpio_num = config->gpio_num,
        .max_leds = config->led_count,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = config->enable_dma,
        },
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip), TAG, "Failed to create LED strip device");

    s_led_count = config->led_count;
    s_brightness = config->brightness;

    return app_rgb_led_clear();
}

/**
 * @brief Deinitializes the RGB LED driver.
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_deinit(void)
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Driver is not initialized");

    esp_err_t err = led_strip_del(s_strip);
    if (err == ESP_OK) {
        s_strip = NULL;
        s_led_count = 0;
    }

    return err;
}

/**
 * @brief Sets the brightness of the RGB LED driver.
 *
 * Args:
 *     brightness: The desired brightness level (0-255).
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_set_brightness(uint8_t brightness)
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Driver is not initialized");

    s_brightness = brightness;
    return ESP_OK;
}

/**
 * @brief Gets the brightness of the RGB LED driver.
 *
 * Returns:
 *     The current brightness level (0-255).
 */
uint8_t app_rgb_led_get_brightness(void)
{
    return s_brightness;
}

/**
 * @brief Sets the color of a specific LED pixel.
 *
 * Args:
 *     index: The index of the LED pixel to set.
 *     red: The red channel value (0-255).
 *     green: The green channel value (0-255).
 *     blue: The blue channel value (0-255).
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Driver is not initialized");
    ESP_RETURN_ON_FALSE(index < s_led_count, ESP_ERR_INVALID_ARG, TAG, "LED index is out of range");

    ESP_RETURN_ON_ERROR(led_strip_set_pixel(s_strip,
                                            index,
                                            scale_channel(red),
                                            scale_channel(green),
                                            scale_channel(blue)),
                        TAG,
                        "Failed to set LED pixel");

    return app_rgb_led_refresh();
}

/**
 * @brief Sets the color of a specific LED pixel using an RGB structure.
 *
 * Args:
 *     index: The index of the LED pixel to set.
 *     color: The RGB color values.
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_set_pixel_rgb(uint32_t index, app_rgb_led_rgb_t color)
{
    return app_rgb_led_set_pixel(index, color.red, color.green, color.blue);
}

/**
 * @brief Sets the color of the first LED pixel.
 *
 * Args:
 *     color: The desired RGB color.
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_set_color(app_rgb_led_color_t color)
{
    return app_rgb_led_set_pixel_rgb(0, app_rgb_led_color_to_rgb(color));
}

/**
 * @brief Fills all LED pixels with a specific color.
 *
 * Args:
 *     red: The red channel value (0-255).
 *     green: The green channel value (0-255).
 *     blue: The blue channel value (0-255).
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_fill(uint8_t red, uint8_t green, uint8_t blue)
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Driver is not initialized");

    for (uint32_t index = 0; index < s_led_count; index++) {
        ESP_RETURN_ON_ERROR(led_strip_set_pixel(s_strip,
                                                index,
                                                scale_channel(red),
                                                scale_channel(green),
                                                scale_channel(blue)),
                            TAG,
                            "Failed to set LED pixel");
    }

    return app_rgb_led_refresh();
}

/**
 * @brief Clears all LED pixels.
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_clear(void)
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Driver is not initialized");

    return led_strip_clear(s_strip);
}

/**
 * @brief Refreshes the RGB LED driver.
 *
 * Returns:
 *     ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_rgb_led_refresh(void)
{
    ESP_RETURN_ON_FALSE(is_initialized(), ESP_ERR_INVALID_STATE, TAG, "Driver is not initialized");

    return led_strip_refresh(s_strip);
}

/**
 * @brief Converts a predefined RGB LED color to an RGB structure.
 *
 * Args:
 *     color: The predefined RGB LED color.
 *
 * Returns:
 *     The corresponding RGB color values.
 */
app_rgb_led_rgb_t app_rgb_led_color_to_rgb(app_rgb_led_color_t color)
{
    switch (color) {
    case APP_RGB_LED_COLOR_RED:
        return (app_rgb_led_rgb_t){ .red = 255, .green = 0, .blue = 0 };

    case APP_RGB_LED_COLOR_GREEN:
        return (app_rgb_led_rgb_t){ .red = 0, .green = 255, .blue = 0 };

    case APP_RGB_LED_COLOR_BLUE:
        return (app_rgb_led_rgb_t){ .red = 0, .green = 0, .blue = 255 };

    case APP_RGB_LED_COLOR_WHITE:
        return (app_rgb_led_rgb_t){ .red = 255, .green = 255, .blue = 255 };

    case APP_RGB_LED_COLOR_YELLOW:
        return (app_rgb_led_rgb_t){ .red = 255, .green = 255, .blue = 0 };

    case APP_RGB_LED_COLOR_CYAN:
        return (app_rgb_led_rgb_t){ .red = 0, .green = 255, .blue = 255 };

    case APP_RGB_LED_COLOR_MAGENTA:
        return (app_rgb_led_rgb_t){ .red = 255, .green = 0, .blue = 255 };

    case APP_RGB_LED_COLOR_ORANGE:
        return (app_rgb_led_rgb_t){ .red = 255, .green = 80, .blue = 0 };

    case APP_RGB_LED_COLOR_PURPLE:
        return (app_rgb_led_rgb_t){ .red = 128, .green = 0, .blue = 255 };

    case APP_RGB_LED_COLOR_OFF:
    default:
        return (app_rgb_led_rgb_t){ .red = 0, .green = 0, .blue = 0 };
    }
}
