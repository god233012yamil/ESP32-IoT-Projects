#include <stdbool.h>
#include <stdint.h>

#include "class/hid/hid_device.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"

#include "usb_descriptors.h"
#include "usb_mouse_config.h"

static const char *TAG = "usb_mouse";

static bool s_usb_suspended = false;
static bool s_remote_wakeup_enabled = false;

typedef enum {
    MOUSE_DIRECTION_RIGHT = 0,
    MOUSE_DIRECTION_DOWN,
    MOUSE_DIRECTION_LEFT,
    MOUSE_DIRECTION_UP,
    MOUSE_DIRECTION_COUNT,
} mouse_direction_t;

/**
 * Initializes the BOOT button as an active-low GPIO input.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     ESP_OK when the GPIO was configured successfully. Otherwise, returns the
 *     error code reported by the GPIO driver.
 */
static esp_err_t boot_button_init(void)
{
    const gpio_config_t button_config = {
        .pin_bit_mask = BIT64(USB_MOUSE_BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&button_config);
}

/**
 * Reads the BOOT button state.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     True when the button is pressed. Otherwise, false.
 */
static bool boot_button_is_pressed(void)
{
    return gpio_get_level(USB_MOUSE_BOOT_BUTTON_GPIO) == 0;
}

/**
 * Initializes the TinyUSB device stack as a USB HID mouse.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     ESP_OK when TinyUSB was installed successfully. Otherwise, returns the
 *     error code reported by the TinyUSB driver.
 */
static esp_err_t usb_mouse_init(void)
{
    tinyusb_config_t tusb_config = TINYUSB_DEFAULT_CONFIG();

    tusb_config.descriptor.device = NULL;
    tusb_config.descriptor.full_speed_config = usb_hid_configuration_descriptor;
    tusb_config.descriptor.string = usb_hid_string_descriptor;
    tusb_config.descriptor.string_count = usb_hid_string_descriptor_count;

#if (TUD_OPT_HIGH_SPEED)
    tusb_config.descriptor.high_speed_config = usb_hid_configuration_descriptor;
#endif

    return tinyusb_driver_install(&tusb_config);
}

/**
 * Returns whether a HID mouse report can be sent safely.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     True when the host has mounted the USB device, the USB bus is not
 *     suspended, and the HID interface is ready for a new input report.
 */
static bool mouse_report_is_ready(void)
{
    return tud_mounted() && !s_usb_suspended && tud_hid_ready();
}

/**
 * Sends one HID mouse report to the host.
 *
 * Args:
 *     buttons: Button bitmap. Bit 0 is left, bit 1 is right, and bit 2 is
 *         middle.
 *     x_delta: Relative movement on the X axis.
 *     y_delta: Relative movement on the Y axis.
 *     wheel_delta: Relative movement of the vertical scroll wheel.
 *
 * Returns:
 *     True when the report was queued. Otherwise, false.
 */
static bool mouse_send_report(uint8_t buttons, int8_t x_delta, int8_t y_delta, int8_t wheel_delta)
{
    if (!mouse_report_is_ready()) {
        return false;
    }

    tud_hid_mouse_report(
        USB_HID_REPORT_ID_MOUSE,
        buttons,
        x_delta,
        y_delta,
        wheel_delta,
        0
    );

    return true;
}

/**
 * Moves the mouse by one relative step.
 *
 * Args:
 *     x_delta: Relative movement on the X axis.
 *     y_delta: Relative movement on the Y axis.
 *
 * Returns:
 *     True when the movement report was queued. Otherwise, false.
 */
static bool mouse_move(int8_t x_delta, int8_t y_delta)
{
    return mouse_send_report(0, x_delta, y_delta, 0);
}

/**
 * Releases all mouse buttons and stops all mouse movement.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     True when the neutral report was queued. Otherwise, false.
 */
static bool mouse_release_all(void)
{
    return mouse_send_report(0, 0, 0, 0);
}

/**
 * Sends a left-click sequence to the USB host.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     True when both the press and release reports were queued. Otherwise,
 *     false.
 */
static bool mouse_left_click(void)
{
    if (!mouse_send_report(USB_MOUSE_LEFT_BUTTON, 0, 0, 0)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_CLICK_HOLD_MS));

    return mouse_release_all();
}

/**
 * Sends several relative movement reports in the same direction.
 *
 * Args:
 *     x_delta: Relative movement on the X axis for each report.
 *     y_delta: Relative movement on the Y axis for each report.
 *     steps: Number of movement reports to send.
 *
 * Returns:
 *     None.
 */
static void mouse_move_steps(int8_t x_delta, int8_t y_delta, uint16_t steps)
{
    for (uint16_t i = 0; i < steps; i++) {
        mouse_move(x_delta, y_delta);
        vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_REPORT_DELAY_MS));
    }
}

/**
 * Draws a square-like cursor trajectory on the host computer.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     None.
 */
static void mouse_draw_square(void)
{
    for (mouse_direction_t direction = MOUSE_DIRECTION_RIGHT;
         direction < MOUSE_DIRECTION_COUNT;
         direction++) {
        switch (direction) {
        case MOUSE_DIRECTION_RIGHT:
            mouse_move_steps(USB_MOUSE_MOVE_DELTA, 0, USB_MOUSE_SQUARE_SIDE_STEPS);
            break;

        case MOUSE_DIRECTION_DOWN:
            mouse_move_steps(0, USB_MOUSE_MOVE_DELTA, USB_MOUSE_SQUARE_SIDE_STEPS);
            break;

        case MOUSE_DIRECTION_LEFT:
            mouse_move_steps(-USB_MOUSE_MOVE_DELTA, 0, USB_MOUSE_SQUARE_SIDE_STEPS);
            break;

        case MOUSE_DIRECTION_UP:
            mouse_move_steps(0, -USB_MOUSE_MOVE_DELTA, USB_MOUSE_SQUARE_SIDE_STEPS);
            break;

        default:
            break;
        }
    }
}

/**
 * Runs the demonstration movement used by the article project.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     None.
 */
static void mouse_run_demo(void)
{
    ESP_LOGI(TAG, "Running USB mouse demo");

    mouse_draw_square();
    vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_REPORT_DELAY_MS));
    mouse_left_click();
    vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_DEMO_REPEAT_DELAY_MS));
}

/**
 * TinyUSB callback invoked when the USB bus is suspended by the host.
 *
 * Args:
 *     remote_wakeup_en: True when the host allows the device to request remote
 *         wakeup.
 *
 * Returns:
 *     None.
 */
void tud_suspend_cb(bool remote_wakeup_en)
{
    s_usb_suspended = true;
    s_remote_wakeup_enabled = remote_wakeup_en;

    ESP_LOGI(TAG, "USB suspended. Remote wakeup: %s", remote_wakeup_en ? "enabled" : "disabled");
}

/**
 * TinyUSB callback invoked when the USB bus resumes.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     None.
 */
void tud_resume_cb(void)
{
    s_usb_suspended = false;
    ESP_LOGI(TAG, "USB resumed");
}

/**
 * Main ESP-IDF application entry point.
 *
 * Args:
 *     None.
 *
 * Returns:
 *     None.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 USB mouse example starting");

    // Initialize the BOOT button before USB to ensure the button is responsive as soon as possible, 
    // even if USB initialization takes a while.
    ESP_ERROR_CHECK(boot_button_init());
    
    // initialize TinyUSB after the button to ensure the button is responsive as soon as possible, 
    // even if USB initialization takes a while
    ESP_ERROR_CHECK(usb_mouse_init());

    ESP_LOGI(TAG, "TinyUSB initialized");
    ESP_LOGI(TAG, "Waiting %d ms before sending reports", USB_MOUSE_STARTUP_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_STARTUP_DELAY_MS));

    bool demo_has_run = false;

    while (true) {
        // Wait until the host has mounted the USB device before sending reports. 
        // If the host unmounts the device, return to waiting for it to mount again.
        if (!tud_mounted()) {
            demo_has_run = false;
            vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_IDLE_DELAY_MS));
            continue;
        }

        // If the USB bus is suspended, wait until it resumes. If the BOOT button is pressed while
        // suspended and the host allows remote wakeup, request the host to resume the USB bus.
        if (s_usb_suspended) {
            if (boot_button_is_pressed() && s_remote_wakeup_enabled) {
                ESP_LOGI(TAG, "Requesting USB remote wakeup");
                tud_remote_wakeup();
            }

            vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_IDLE_DELAY_MS));
            continue;
        }

        // Run once after enumeration, then repeat only when the BOOT button is pressed.
        if (!demo_has_run || boot_button_is_pressed()) {
            mouse_run_demo();
            demo_has_run = true;
        }

        vTaskDelay(pdMS_TO_TICKS(USB_MOUSE_IDLE_DELAY_MS));
    }
}
