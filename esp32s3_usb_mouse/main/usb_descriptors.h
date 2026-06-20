#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>
#include "class/hid/hid_device.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    USB_HID_REPORT_ID_MOUSE = 1,
};

extern const uint8_t usb_hid_report_descriptor[];
extern const uint8_t usb_hid_configuration_descriptor[];
extern const char *usb_hid_string_descriptor[];
extern const uint8_t usb_hid_string_descriptor_count;

#ifdef __cplusplus
}
#endif

#endif /* USB_DESCRIPTORS_H */
