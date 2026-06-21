#include "button_driver.h"
#include "macro_engine.h"
#include "usb_hid_keyboard.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define MACRO_EVENT_QUEUE_LENGTH 8U
#define BUTTON_TASK_STACK_SIZE 3072U
#define MACRO_TASK_STACK_SIZE 4096U
#define BUTTON_TASK_PRIORITY 5U
#define MACRO_TASK_PRIORITY 4U

static const char *TAG = "macro_pad";

/**
 * @brief Initializes the USB macro pad and starts its worker tasks.
 *
 * The application uses one task for periodic button scanning and one task for
 * serialized macro execution. A FreeRTOS queue decouples input processing
 * from USB report timing.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-S3 USB macro pad");
    ESP_LOGI(TAG, "Buttons: GPIO4=Copy, GPIO5=Paste, GPIO6=Save, GPIO7=Task Manager");

    // Initialize the button driver. This configures the GPIO pins and sets up any necessary interrupts for button scanning.
    ESP_ERROR_CHECK(button_driver_init());
    
    // Initialize the USB HID keyboard interface. This sets up the necessary USB descriptors and starts the USB device stack.
    ESP_ERROR_CHECK(usb_hid_keyboard_init());

    // Create a FreeRTOS queue to hold button events. This allows the button scanning task 
    // to send events to the macro engine task without blocking.
    QueueHandle_t macro_event_queue = xQueueCreate(
        MACRO_EVENT_QUEUE_LENGTH,
        sizeof(button_event_t));
    configASSERT(macro_event_queue != NULL);

    // Create the button scanning task. This task will periodically read the state of the buttons 
    // and send events to the macro engine task via the queue.
    BaseType_t task_created = xTaskCreate(
        button_driver_task,
        "button_scan",
        BUTTON_TASK_STACK_SIZE,
        macro_event_queue,
        BUTTON_TASK_PRIORITY,
        NULL);
    configASSERT(task_created == pdPASS);

    // Create the macro engine task. This task will receive button events from the queue and execute 
    // the corresponding macros, sending USB HID reports as needed.
    task_created = xTaskCreate(
        macro_engine_task,
        "macro_engine",
        MACRO_TASK_STACK_SIZE,
        macro_event_queue,
        MACRO_TASK_PRIORITY,
        NULL);
    configASSERT(task_created == pdPASS);
}
