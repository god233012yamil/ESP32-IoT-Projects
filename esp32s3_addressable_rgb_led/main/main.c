#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "app_rgb_led.h"

#define RGB_LED_GPIO              48
#define RGB_LED_COUNT             1
#define RGB_LED_BRIGHTNESS        32
#define RGB_LED_DEMO_DELAY_MS     700
#define RGB_LED_STATUS_BLINK_MS   150

static const char *TAG = "rgb_led_app";

typedef enum {
    SYSTEM_STATUS_BOOTING = 0,
    SYSTEM_STATUS_CONNECTING,
    SYSTEM_STATUS_CONNECTED,
    SYSTEM_STATUS_OTA_UPDATE,
    SYSTEM_STATUS_ERROR,
} system_status_t;

/**
 * @brief Waits for the selected number of milliseconds.
 *
 * Args:
 *     delay_ms: Delay time in milliseconds.
 */
static void delay_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

/**
 * @brief Blinks the first RGB LED using the selected color.
 *
 * Args:
 *     color: Predefined RGB LED color.
 *     blink_count: Number of on/off blink cycles.
 *     delay_ms_value: Delay time for each on and off state in milliseconds.
 */
static void blink_status_color(app_rgb_led_color_t color, uint32_t blink_count, uint32_t delay_ms_value)
{
    for (uint32_t i = 0; i < blink_count; i++) {
        ESP_ERROR_CHECK(app_rgb_led_set_color(color));
        delay_ms(delay_ms_value);
        ESP_ERROR_CHECK(app_rgb_led_set_color(APP_RGB_LED_COLOR_OFF));
        delay_ms(delay_ms_value);
    }
}

/**
 * @brief Displays a firmware-style status color on the RGB LED.
 *
 * Args:
 *     status: Current system status value.
 */
static void show_system_status(system_status_t status)
{
    switch (status) {
    case SYSTEM_STATUS_BOOTING:
        ESP_LOGI(TAG, "Status: booting");
        blink_status_color(APP_RGB_LED_COLOR_BLUE, 2, RGB_LED_STATUS_BLINK_MS);
        break;

    case SYSTEM_STATUS_CONNECTING:
        ESP_LOGI(TAG, "Status: connecting");
        blink_status_color(APP_RGB_LED_COLOR_YELLOW, 3, RGB_LED_STATUS_BLINK_MS);
        break;

    case SYSTEM_STATUS_CONNECTED:
        ESP_LOGI(TAG, "Status: connected");
        ESP_ERROR_CHECK(app_rgb_led_set_color(APP_RGB_LED_COLOR_GREEN));
        delay_ms(600);
        break;

    case SYSTEM_STATUS_OTA_UPDATE:
        ESP_LOGI(TAG, "Status: OTA update");
        ESP_ERROR_CHECK(app_rgb_led_set_color(APP_RGB_LED_COLOR_PURPLE));
        delay_ms(600);
        break;

    case SYSTEM_STATUS_ERROR:
    default:
        ESP_LOGI(TAG, "Status: error");
        blink_status_color(APP_RGB_LED_COLOR_RED, 4, RGB_LED_STATUS_BLINK_MS);
        break;
    }
}

/**
 * @brief Runs a simple color wheel demonstration on one addressable RGB LED.
 */
static void run_color_cycle_demo(void)
{
    static const app_rgb_led_color_t colors[] = {
        APP_RGB_LED_COLOR_RED,
        APP_RGB_LED_COLOR_GREEN,
        APP_RGB_LED_COLOR_BLUE,
        APP_RGB_LED_COLOR_WHITE,
        APP_RGB_LED_COLOR_YELLOW,
        APP_RGB_LED_COLOR_CYAN,
        APP_RGB_LED_COLOR_MAGENTA,
        APP_RGB_LED_COLOR_ORANGE,
        APP_RGB_LED_COLOR_PURPLE,
        APP_RGB_LED_COLOR_OFF,
    };

    for (uint32_t i = 0; i < (sizeof(colors) / sizeof(colors[0])); i++) {
        ESP_ERROR_CHECK(app_rgb_led_set_color(colors[i]));
        delay_ms(RGB_LED_DEMO_DELAY_MS);
    }
}

/**
 * @brief Runs a brightness ramp demonstration using the green channel.
 */
static void run_brightness_demo(void)
{
    for (uint16_t brightness = 8; brightness <= 64; brightness += 8) {
        ESP_ERROR_CHECK(app_rgb_led_set_brightness((uint8_t)brightness));
        ESP_ERROR_CHECK(app_rgb_led_set_color(APP_RGB_LED_COLOR_GREEN));
        delay_ms(180);
    }

    ESP_ERROR_CHECK(app_rgb_led_set_brightness(RGB_LED_BRIGHTNESS));
    ESP_ERROR_CHECK(app_rgb_led_set_color(APP_RGB_LED_COLOR_OFF));
    delay_ms(400);
}

/**
 * @brief Main application entry point.
 */
void app_main(void)
{
    // Configure the RGB LED driver with the selected GPIO, LED count, and brightness.
    const app_rgb_led_config_t rgb_led_config = {
        .gpio_num = RGB_LED_GPIO,
        .led_count = RGB_LED_COUNT,
        .brightness = RGB_LED_BRIGHTNESS,
        .enable_dma = false,
    };

    // Initialize the RGB LED driver and run the demo sequence.
    ESP_LOGI(TAG, "Initializing RGB LED driver");
    ESP_ERROR_CHECK(app_rgb_led_init(&rgb_led_config));

    // Display system status colors to indicate the current state of the application.
    show_system_status(SYSTEM_STATUS_BOOTING);
    show_system_status(SYSTEM_STATUS_CONNECTING);
    show_system_status(SYSTEM_STATUS_CONNECTED);
    show_system_status(SYSTEM_STATUS_OTA_UPDATE);
    show_system_status(SYSTEM_STATUS_ERROR);

    // Run the color cycle and brightness demo in an infinite loop.
    while (1) {
        // Run the color cycle and brightness demo in an infinite loop.
        run_color_cycle_demo();
        
        // Run the brightness demo after the color cycle.
        run_brightness_demo();
    }
}
