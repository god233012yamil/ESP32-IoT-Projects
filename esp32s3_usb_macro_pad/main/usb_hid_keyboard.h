#ifndef USB_HID_KEYBOARD_H
#define USB_HID_KEYBOARD_H

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes TinyUSB as a USB HID keyboard device.
 *
 * Returns:
 *     ESP_OK: The TinyUSB driver was installed successfully.
 *     Other: An ESP-IDF or TinyUSB driver error code.
 */
esp_err_t usb_hid_keyboard_init(void);

/**
 * @brief Sends a keyboard shortcut followed by an all-keys-released report.
 *
 * Args:
 *     modifier: HID keyboard modifier bitmap, such as left Ctrl or Shift.
 *     keycode: HID usage ID for the non-modifier key.
 *
 * Returns:
 *     ESP_OK: Both press and release reports were submitted.
 *     ESP_ERR_INVALID_STATE: The USB device is not mounted.
 *     ESP_ERR_TIMEOUT: The HID endpoint did not become ready in time.
 */
esp_err_t usb_hid_keyboard_send_shortcut(uint8_t modifier, uint8_t keycode);

#ifdef __cplusplus
}
#endif

#endif
