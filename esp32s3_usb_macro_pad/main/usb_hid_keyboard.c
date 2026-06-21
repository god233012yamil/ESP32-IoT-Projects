#include "usb_hid_keyboard.h"

#include <stdbool.h>
#include <stddef.h>

#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"

#define HID_REPORT_ID_KEYBOARD 1U
#define HID_READY_TIMEOUT_MS 250U
#define HID_KEY_HOLD_TIME_MS 12U

#define USB_CONFIG_TOTAL_LENGTH \
    (TUD_CONFIG_DESC_LEN + (CFG_TUD_HID * TUD_HID_DESC_LEN))

static const char *TAG = "usb_hid_keyboard";

static const uint8_t s_hid_report_descriptor[] =
{
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD))
};

static const char *s_string_descriptors[] =
{
    (const char[]){0x09, 0x04},
    "Yamil Embedded",
    "ESP32-S3 USB Macro Pad",
    "MACROPAD-001",
    "Macro Pad Keyboard",
};

static const uint8_t s_configuration_descriptor[] =
{
    TUD_CONFIG_DESCRIPTOR(
        1,
        1,
        0,
        USB_CONFIG_TOTAL_LENGTH,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        100),

    TUD_HID_DESCRIPTOR(
        0,
        4,
        HID_ITF_PROTOCOL_KEYBOARD,
        sizeof(s_hid_report_descriptor),
        0x81,
        16,
        10),
};

/**
 * @brief Waits until the HID IN endpoint can accept another report.
 *
 * Args:
 *     timeout_ms: Maximum wait duration in milliseconds.
 *
 * Returns:
 *     ESP_OK: The HID endpoint is ready.
 *     ESP_ERR_INVALID_STATE: The USB device is no longer mounted.
 *     ESP_ERR_TIMEOUT: The endpoint did not become ready before timeout.
 */
static esp_err_t usb_hid_keyboard_wait_ready(uint32_t timeout_ms)
{
    const TickType_t start_tick = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    while (!tud_hid_ready())
    {
        if (!tud_mounted())
        {
            return ESP_ERR_INVALID_STATE;
        }

        if ((xTaskGetTickCount() - start_tick) >= timeout_ticks)
        {
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ESP_OK;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t requested_length)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)requested_length;

    // Returning zero stalls unsupported GET_REPORT requests as TinyUSB expects.
    return 0;
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const *buffer,
    uint16_t buffer_size)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)buffer_size;
}

void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB host mounted the macro pad");
}

void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB host unmounted the macro pad");
}

void tud_suspend_cb(bool remote_wakeup_enabled)
{
    ESP_LOGI(TAG, "USB bus suspended; remote wakeup=%s",
             remote_wakeup_enabled ? "enabled" : "disabled");
}

void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB bus resumed");
}

esp_err_t usb_hid_keyboard_init(void)
{
    tinyusb_config_t config = {
        .device_descriptor = NULL,
        .string_descriptor = s_string_descriptors,
        .string_descriptor_count =
            sizeof(s_string_descriptors) / sizeof(s_string_descriptors[0]),
#if TUD_OPT_HIGH_SPEED
        .fs_configuration_descriptor = s_configuration_descriptor,
        .hs_configuration_descriptor = s_configuration_descriptor,
#else
        .configuration_descriptor = s_configuration_descriptor,
#endif
        .external_phy = false,
        .self_powered = false,
        .vbus_monitor_io = -1,
    };

    ESP_LOGI(TAG, "Installing TinyUSB HID keyboard driver");
    return tinyusb_driver_install(&config);
}

esp_err_t usb_hid_keyboard_send_shortcut(uint8_t modifier, uint8_t keycode)
{
    uint8_t keycodes[6] = {0};
    esp_err_t result;

    if (!tud_mounted())
    {
        return ESP_ERR_INVALID_STATE;
    }

    result = usb_hid_keyboard_wait_ready(HID_READY_TIMEOUT_MS);
    if (result != ESP_OK)
    {
        return result;
    }

    keycodes[0] = keycode;
    tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, modifier, keycodes);

    // Give the host enough time to observe the pressed state.
    vTaskDelay(pdMS_TO_TICKS(HID_KEY_HOLD_TIME_MS));

    result = usb_hid_keyboard_wait_ready(HID_READY_TIMEOUT_MS);
    if (result != ESP_OK)
    {
        return result;
    }

    tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, 0, NULL);
    return ESP_OK;
}
