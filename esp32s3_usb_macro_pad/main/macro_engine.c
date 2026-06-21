#include "macro_engine.h"

#include <stddef.h>

#include "button_driver.h"
#include "class/hid/hid.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_hid_keyboard.h"

static const char *TAG = "macro_engine";

static const macro_id_t s_button_macro_map[BUTTON_DRIVER_BUTTON_COUNT] =
{
    MACRO_ID_COPY,
    MACRO_ID_PASTE,
    MACRO_ID_SAVE,
    MACRO_ID_TASK_MANAGER,
};

/**
 * @brief Executes one predefined keyboard macro.
 *
 * Args:
 *     macro_id: Logical macro to execute.
 *
 * Returns:
 *     ESP_OK: The macro report sequence was submitted.
 *     ESP_ERR_INVALID_ARG: The macro identifier is not valid.
 *     Other: Error returned by the USB HID keyboard module.
 */
static esp_err_t macro_engine_execute(macro_id_t macro_id)
{
    switch (macro_id)
    {
        case MACRO_ID_COPY:
            ESP_LOGI(TAG, "Executing Copy: Ctrl+C");
            return usb_hid_keyboard_send_shortcut(
                KEYBOARD_MODIFIER_LEFTCTRL,
                HID_KEY_C);

        case MACRO_ID_PASTE:
            ESP_LOGI(TAG, "Executing Paste: Ctrl+V");
            return usb_hid_keyboard_send_shortcut(
                KEYBOARD_MODIFIER_LEFTCTRL,
                HID_KEY_V);

        case MACRO_ID_SAVE:
            ESP_LOGI(TAG, "Executing Save: Ctrl+S");
            return usb_hid_keyboard_send_shortcut(
                KEYBOARD_MODIFIER_LEFTCTRL,
                HID_KEY_S);

        case MACRO_ID_TASK_MANAGER:
            ESP_LOGI(TAG, "Executing Task Manager: Ctrl+Shift+Esc");
            return usb_hid_keyboard_send_shortcut(
                KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT,
                HID_KEY_ESCAPE);

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

/**
 * @brief Task function for the macro engine.
 *
 * This task receives button events from the queue and executes the corresponding macros.
 *
 * Args:
 *     context: Pointer to the FreeRTOS queue handle.
 */
void macro_engine_task(void *context)
{
    QueueHandle_t event_queue = (QueueHandle_t)context;
    button_event_t event;

    configASSERT(event_queue != NULL);
    ESP_LOGI(TAG, "Macro engine started");

    while (true)
    {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        if (event.button_index >= BUTTON_DRIVER_BUTTON_COUNT)
        {
            ESP_LOGW(TAG, "Ignored invalid button index %u",
                     (unsigned int)event.button_index);
            continue;
        }

        const macro_id_t macro_id = s_button_macro_map[event.button_index];
        const esp_err_t result = macro_engine_execute(macro_id);

        if (result != ESP_OK)
        {
            ESP_LOGW(TAG, "Macro failed: %s", esp_err_to_name(result));
        }
    }
}
