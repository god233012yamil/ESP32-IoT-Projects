#include "usb_descriptors.h"

#include "tinyusb.h"

#define USB_HID_TOTAL_DESCRIPTOR_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define USB_HID_INTERFACE_NUMBER     0
#define USB_HID_ENDPOINT_IN          0x81
#define USB_HID_ENDPOINT_SIZE        16
#define USB_HID_POLL_INTERVAL_MS     10
#define USB_HID_MAX_POWER_MA         100

const uint8_t usb_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(USB_HID_REPORT_ID_MOUSE))
};

const uint8_t usb_hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(
        1,
        1,
        0,
        USB_HID_TOTAL_DESCRIPTOR_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        USB_HID_MAX_POWER_MA
    ),

    TUD_HID_DESCRIPTOR(
        USB_HID_INTERFACE_NUMBER,
        4,
        false,
        sizeof(usb_hid_report_descriptor),
        USB_HID_ENDPOINT_IN,
        USB_HID_ENDPOINT_SIZE,
        USB_HID_POLL_INTERVAL_MS
    ),
};

const char *usb_hid_string_descriptor[] = {
    (const char[]){0x09, 0x04},
    "Yamil Embedded",
    "ESP32-S3 USB Mouse",
    "ESP32S3USBMOUSE01",
    "HID Mouse Interface",
};

const uint8_t usb_hid_string_descriptor_count =
    sizeof(usb_hid_string_descriptor) / sizeof(usb_hid_string_descriptor[0]);

/**
 * Returns the HID report descriptor used by the USB mouse interface.
 *
 * Args:
 *     instance: HID interface instance requested by the host. This project uses
 *         one HID interface, so this value is intentionally ignored.
 *
 * Returns:
 *     Pointer to the static HID report descriptor.
 */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return usb_hid_report_descriptor;
}

/**
 * Handles HID GET_REPORT control requests from the USB host.
 *
 * Args:
 *     instance: HID interface instance requested by the host.
 *     report_id: HID report ID requested by the host.
 *     report_type: HID report type requested by the host.
 *     buffer: Destination buffer provided by TinyUSB.
 *     reqlen: Requested report length in bytes.
 *
 * Returns:
 *     Zero because this demo does not provide feature or output reports through
 *     the control endpoint.
 */
uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

/**
 * Handles HID SET_REPORT requests from the USB host.
 *
 * Args:
 *     instance: HID interface instance that received the report.
 *     report_id: HID report ID sent by the host.
 *     report_type: HID report type sent by the host.
 *     buffer: Received report payload.
 *     bufsize: Number of bytes in the received report payload.
 */
void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const *buffer,
    uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}
