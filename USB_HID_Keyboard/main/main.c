/*
 * ESP-IDF USB HID Keyboard Automation with FreeRTOS
 *
 * This project implements a USB HID keyboard using ESP-IDF framework with FreeRTOS tasks.
 * Compatible with ESP32-S2, ESP32-S3, and ESP32-C3 (boards with native USB support).
 *
 * Features:
 * - FreeRTOS task-based architecture
 * - 12+ automated keyboard commands
 * - Non-blocking LED control
 * - TinyUSB HID implementation
 * - Thread-safe command execution
 *
 * File Structure:
 * - main/main.c (this file)
 * - main/CMakeLists.txt
 * - CMakeLists.txt
 * - sdkconfig
 *
 * Build Instructions:
 * 1. Install ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/get-started/
 * 2. idf.py set-target esp32s3 (or esp32s2, esp32c3)
 * 3. idf.py menuconfig (configure USB settings)
 * 4. idf.py build
 * 5. idf.py flash monitor
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "tinyusb.h"
#include "tusb.h"
#include "class/hid/hid_device.h"

// ============================================================================
// Configuration and Constants
// ============================================================================

#define BUTTON_GPIO         GPIO_NUM_48 // Boot button
#define LED_GPIO            GPIO_NUM_2  // Built-in LED

#define BUTTON_ACTIVE_LEVEL 0           // Button pressed = LOW
#define DEBOUNCE_TIME_MS    5//50       // Debounce delay
#define COMMAND_DELAY_MS    500         // Delay between commands
#define LONG_DELAY_MS       1000        // Delay for slow operations
#define KEY_PRESS_DELAY_MS  100         // Key press duration

// Task priorities
#define AUTOMATION_TASK_PRIORITY    4
#define LED_TASK_PRIORITY           3

// Task stack sizes
#define AUTOMATION_TASK_STACK_SIZE  4096
#define LED_TASK_STACK_SIZE         2048

// HID Report ID
#define HID_REPORT_ID_KEYBOARD 1

// Keyboard modifier keys
#define KEYBOARD_MODIFIER_LEFTCTRL 0x01
#define KEYBOARD_MODIFIER_LEFTSHIFT 0x02
#define KEYBOARD_MODIFIER_LEFTALT 0x04
#define KEYBOARD_MODIFIER_LEFTGUI 0x08
#define KEYBOARD_MODIFIER_RIGHTCTRL 0x10
#define KEYBOARD_MODIFIER_RIGHTSHIFT 0x20
#define KEYBOARD_MODIFIER_RIGHTALT 0x40
#define KEYBOARD_MODIFIER_RIGHTGUI 0x80

// HID Keycodes
#define HID_KEY_A 0x04
#define HID_KEY_C 0x06
#define HID_KEY_N 0x11
#define HID_KEY_R 0x15
#define HID_KEY_S 0x16
#define HID_KEY_V 0x19
#define HID_KEY_ENTER 0x28
#define HID_KEY_ESC 0x29
#define HID_KEY_TAB 0x2B
#define HID_KEY_SPACE 0x2C
#define HID_KEY_PRINTSCREEN 0x46
#define HID_KEY_F4 0x3D

// Timeouts for USB/HID readiness (ms)
#define USB_READY_TIMEOUT_MS 500  //3000
#define HID_READY_TIMEOUT_MS 500

// ============================================================================
// Global Variables
// ============================================================================

static const char *TAG = "USB_HID_KEYBOARD";

// FreeRTOS handles
static QueueHandle_t button_event_queue = NULL;
static SemaphoreHandle_t keyboard_mutex = NULL;
static TaskHandle_t automation_task_handle = NULL;

// State variables
static volatile bool automation_running = false;

// LED control queue
typedef enum
{
    LED_CMD_ON,
    LED_CMD_OFF,
    LED_CMD_BLINK,
    LED_CMD_TOGGLE
} led_command_t;

typedef struct
{
    led_command_t command;
    uint32_t param; // For blink: number of times
} led_message_t;

static QueueHandle_t led_queue = NULL;

// ============================================================================
// USB HID Descriptor
// ============================================================================

/**
 * HID Keyboard Report Descriptor
 * Defines the format of HID reports sent to the host
 */
static const uint8_t hid_keyboard_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD))};

// ============================================================================
// TinyUSB Device & Configuration Descriptors (HID Keyboard)
// ============================================================================

// -------- Device Descriptor --------
static const tusb_desc_device_t _device_desc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,                       // USB 2.0
    .bDeviceClass = TUSB_CLASS_UNSPECIFIED, // each iface specifies class
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE, // 64 for FS
    .idVendor = 0xCafe,                        // TEST VID (change later)
    .idProduct = 0x4001,                       // TEST PID (HID keyboard)
    .bcdDevice = 0x0100,                       // v1.0
    .iManufacturer = 0x01,                     // string index 1
    .iProduct = 0x02,                          // string index 2
    .iSerialNumber = 0x03,                     // string index 3
    .bNumConfigurations = 0x01};

// -------- Configuration Descriptor --------
// One configuration, one interface: HID Keyboard (boot protocol)
enum
{
    ITF_NUM_HID,
    ITF_NUM_TOTAL
};

#define EPNUM_HID 0x81 // EP1 IN
#define HID_REPORT_LEN 8
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t _config_desc[] = {
    // Config number, interface count, string index, total length, attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface, string index, protocol, report descriptor len, EP In address, size & polling interval (ms)
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0 /* iInterface */,
                       HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(hid_keyboard_report_descriptor),
                       EPNUM_HID,
                       HID_REPORT_LEN,
                       10)};

// -------- String Descriptors --------
// Tiny table of UTF-8 strings; IDF converts to UTF-16 internally.
static const char *_string_desc[] = {
    "",                          // 0: keep non-NULL to avoid crashes
    "LearningByTutorials",       // 1: Manufacturer
    "ESP32-S3 USB HID Keyboard", // 2: Product
    "S3-KEY-0001",               // 3: Serial
    "HID Keyboard Interface"     // 4: Interface (optional, not referenced above)
};
static const int _string_desc_count = sizeof(_string_desc) / sizeof(_string_desc[0]);

// ============================================================================
// Function prototypes
// ============================================================================

static void led_send_command(led_command_t cmd, uint32_t param);
static void automation_task(void *pvParameters);
static void delete_automation_task(void);

// ============================================================================
// TinyUSB Callbacks
// ============================================================================

/**
 * Invoked when USB device is mounted (connected and configured)
 * This callback is automatically called by TinyUSB when enumeration completes
 */
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "✓ USB Device Mounted - Keyboard ready!");
    // Signal that device is ready by blinking LED
    led_send_command(LED_CMD_BLINK, 5);

    // // Create automation task
    // xTaskCreate(automation_task, "automation_task",
    //             AUTOMATION_TASK_STACK_SIZE, NULL,
    //             AUTOMATION_TASK_PRIORITY, &automation_task_handle);
}

/**
 * Invoked when USB device is unmounted
 * This callback is automatically called by TinyUSB on disconnect
 */
void tud_umount_cb(void)
{
    ESP_LOGW(TAG, "✗ USB Device Unmounted");
}

/**
 * Invoked when USB is suspended (host went to sleep, cable unplugged, etc.)
 * This callback is automatically called by TinyUSB
 */
void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGI(TAG, "⏸ USB Suspended (remote_wakeup=%d)", remote_wakeup_en);
}

/**
 * Invoked when USB is resumed from suspend
 * This callback is automatically called by TinyUSB
 */
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "▶ USB Resumed");
}

/**
 * Invoked when received GET_REPORT control request
 * Application must fill buffer with report content and return its length
 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
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
 * Invoked when received SET_REPORT control request or
 * received data on OUT endpoint (Report ID = 0, Type = 0)
 */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

/**
 * Invoked when GET HID REPORT DESCRIPTOR is received
 * Application return pointer to descriptor
 * Descriptor contents must persist until the request is complete
 */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return hid_keyboard_report_descriptor;
}

// ============================================================================
// Keyboard Control Functions
// ============================================================================

/**
* @brief Check if the USB device is currently mounted and active.
*
* This inline helper verifies that the TinyUSB device is mounted (enumerated by
* the host) and not suspended. It provides a quick readiness check to ensure
* that USB operations are safe to perform.
*
* @return true if the USB is mounted and not suspended, false otherwise.
*/
static inline bool usb_is_ready(void) {
    return tud_mounted() && !tud_suspended();
}

/**
* @brief Wait until the USB device is mounted and not suspended.
*
* This function repeatedly checks if the USB device is ready by calling
* `usb_is_ready()`. It waits in 10 ms intervals up to the specified timeout.
* If the USB becomes ready within the timeout period, it returns true.
* Otherwise, it returns false.
*
* @param timeout_ms Maximum wait time in milliseconds. If 0, it waits indefinitely.
* @return true if the USB became ready before timeout, false if the timeout expired.
*/
static bool usb_wait_ready(uint32_t timeout_ms) {
    uint32_t waited = 0;
    while (!usb_is_ready())
    {
        if (timeout_ms && waited >= timeout_ms)
            return false;
        vTaskDelay(pdMS_TO_TICKS(10));
        waited += 10;
    }
    return true;
}

/**
* @brief Wait until the HID interface is ready to send or receive reports.
*
* This function polls TinyUSB’s `tud_hid_ready()` in 5 ms intervals until the HID
* endpoint becomes ready or the timeout expires. It prevents attempts to send
* HID reports while the interface is not available (for example, during
* enumeration or when the USB cable is disconnected).
*
* @param timeout_ms Maximum wait time in milliseconds. If 0, it waits indefinitely.
* @return true if the HID interface became ready before timeout, false otherwise.
*/
static bool hid_wait_ready(uint32_t timeout_ms) {
    uint32_t waited = 0;
    // Wait until device is ready
    while (!tud_hid_ready())
    {
        if (timeout_ms && waited >= timeout_ms)
            return false;
        vTaskDelay(pdMS_TO_TICKS(5));
        waited += 5;
    }
    return true;
}

/**
 * Sends a keyboard HID report to the host
 *
 * @param report Pointer to the keyboard report structure
 * @return true if successful, false otherwise
 */
static bool send_keyboard_report(hid_keyboard_report_t *report) {
    if (xSemaphoreTake(keyboard_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {

        // Bail out early if USB isn't present
        if (!usb_wait_ready(USB_READY_TIMEOUT_MS)) {
            xSemaphoreGive(keyboard_mutex);
            ESP_LOGW(TAG, "USB not ready; skipping HID report");

            // If automation task is running, terminate it
            if (automation_running) {
                delete_automation_task();
            }            

            return false;
        }

        // Bounded wait for HID endpoint readiness
        if (!hid_wait_ready(HID_READY_TIMEOUT_MS)) {
            xSemaphoreGive(keyboard_mutex);
            ESP_LOGW(TAG, "HID endpoint not ready; skipping HID report");

            // If automation task is running, terminate it
            if (automation_running) {
                delete_automation_task();
            }

            return false;
        }

        bool result = tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD,
                                              report->modifier,
                                              report->keycode);
        xSemaphoreGive(keyboard_mutex);
        return result;
    }
    return false;
}

/**
 * Releases all keys (sends empty report)
 */
static void keyboard_release_all(void)
{
    hid_keyboard_report_t report = {0};
    send_keyboard_report(&report);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/**
 * Presses a key with optional modifier
 *
 * @param modifier Modifier keys (0 for none)
 * @param keycode HID keycode to press
 */
static void keyboard_press_key(uint8_t modifier, uint8_t keycode)
{
    hid_keyboard_report_t report = {0};
    report.modifier = modifier;
    if (keycode != 0)
    {
        report.keycode[0] = keycode;
    }
    send_keyboard_report(&report);
    vTaskDelay(pdMS_TO_TICKS(KEY_PRESS_DELAY_MS));
    keyboard_release_all();
}

/**
 * Types a string character by character
 *
 * @param str String to type
 */
static void keyboard_type_string(const char *str)
{
    for (size_t i = 0; str[i] != '\0'; i++)
    {
        uint8_t keycode = 0;
        uint8_t modifier = 0;

        // Convert ASCII to HID keycode
        char c = str[i];

        if (c >= 'a' && c <= 'z')
        {
            keycode = HID_KEY_A + (c - 'a');
        }
        else if (c >= 'A' && c <= 'Z')
        {
            keycode = HID_KEY_A + (c - 'A');
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        }
        else if (c >= '1' && c <= '9')
        {
            keycode = 0x1E + (c - '1'); // 1-9 keys
        }
        else if (c == '0')
        {
            keycode = 0x27;
        }
        else if (c == ' ')
        {
            keycode = HID_KEY_SPACE;
        }
        else if (c == '\n')
        {
            keycode = HID_KEY_ENTER;
        }
        else if (c == '.')
        {
            keycode = 0x37;
        }
        else if (c == ':')
        {
            keycode = 0x33;
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        }
        else if (c == '/')
        {
            keycode = 0x38;
        }
        else if (c == '-')
        {
            keycode = 0x2D;
        }
        else if (c == '=')
        {
            keycode = 0x2E;
        }
        else if (c == '!')
        {
            keycode = 0x1E; // 1 key
            modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
        }
        else
        {
            continue; // Skip unsupported characters
        }

        keyboard_press_key(modifier, keycode);
        vTaskDelay(pdMS_TO_TICKS(50)); // Human-like typing speed
    }
}

// ============================================================================
// LED Control Functions
// ============================================================================

/**
 * LED control task
 * Handles LED blinking and status indication without blocking other tasks
 *
 * @param pvParameters Task parameters (unused)
 */
static void led_task(void *pvParameters)
{
    led_message_t msg;
    bool led_state = false;

    ESP_LOGI(TAG, "LED task started");

    while (1)
    {
        if (xQueueReceive(led_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            switch (msg.command)
            {
            case LED_CMD_ON:
                gpio_set_level(LED_GPIO, 1);
                led_state = true;
                break;

            case LED_CMD_OFF:
                gpio_set_level(LED_GPIO, 0);
                led_state = false;
                break;

            case LED_CMD_TOGGLE:
                led_state = !led_state;
                gpio_set_level(LED_GPIO, led_state);
                break;

            case LED_CMD_BLINK:
                for (uint32_t i = 0; i < msg.param; i++)
                {
                    gpio_set_level(LED_GPIO, 1);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    gpio_set_level(LED_GPIO, 0);
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                break;
            }
        }
    }
}

/**
 * Send LED command to LED task
 *
 * @param cmd LED command
 * @param param Command parameter (for blink count)
 */
static void led_send_command(led_command_t cmd, uint32_t param)
{
    led_message_t msg = {
        .command = cmd,
        .param = param};
    xQueueSend(led_queue, &msg, 0);
}

// ============================================================================
// Automation Command Functions
// ============================================================================

/**
 * Command 1: Open Run dialog (Windows) or Spotlight (macOS)
 */
static void cmd_open_run_dialog(void)
{
    ESP_LOGI(TAG, "Command 1: Opening Run/Spotlight...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 2: Open text editor (Notepad)
 */
static void cmd_open_text_editor(void)
{
    ESP_LOGI(TAG, "Command 2: Opening text editor...");
    keyboard_type_string("notepad");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
}

/**
 * Command 3: Type sample formatted text
 */
static void cmd_type_sample_text(void)
{
    ESP_LOGI(TAG, "Command 3: Typing sample text...");

    keyboard_type_string("ESP32 USB HID Keyboard Automation Demo\n");
    keyboard_type_string("======================================\n");
    keyboard_type_string("\n");
    keyboard_type_string("This text was typed by ESP-IDF with FreeRTOS!\n");
    keyboard_type_string("\n");
    keyboard_type_string("Features demonstrated:\n");
    keyboard_type_string("- FreeRTOS task-based architecture\n");
    keyboard_type_string("- TinyUSB HID implementation\n");
    keyboard_type_string("- Non-blocking LED control\n");
    keyboard_type_string("- Thread-safe keyboard operations\n");

    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 4: Select all text (Ctrl+A)
 */
static void cmd_select_all(void)
{
    ESP_LOGI(TAG, "Command 4: Selecting all text...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_A);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 5: Copy to clipboard (Ctrl+C)
 */
static void cmd_copy_text(void)
{
    ESP_LOGI(TAG, "Command 5: Copying text to clipboard...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_C);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 6: Create new document (Ctrl+N)
 */
static void cmd_new_document(void)
{
    ESP_LOGI(TAG, "Command 6: Creating new document...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_N);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
}

/**
 * Command 7: Paste from clipboard (Ctrl+V)
 */
static void cmd_paste_text(void)
{
    ESP_LOGI(TAG, "Command 7: Pasting clipboard content...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_V);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 8: Save document (Ctrl+S)
 */
static void cmd_save_document(void)
{
    ESP_LOGI(TAG, "Command 8: Saving document...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_S);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 9: Open browser with URL
 */
static void cmd_open_browser_url(void)
{
    ESP_LOGI(TAG, "Command 9: Opening browser with URL...");

    // Open Run dialog
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));

    // Type URL
    keyboard_type_string("https://www.example.com");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
}

/**
 * Command 10: Take screenshot (Print Screen)
 */
static void cmd_take_screenshot(void)
{
    ESP_LOGI(TAG, "Command 10: Taking screenshot...");
    keyboard_press_key(0, HID_KEY_PRINTSCREEN);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 11: Open Task Manager (Ctrl+Shift+Esc)
 */
static void cmd_open_task_manager(void)
{
    ESP_LOGI(TAG, "Command 11: Opening Task Manager...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT,
                       HID_KEY_ESC);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
}

/**
 * Command 12: Close window (Alt+F4)
 */
static void cmd_close_window(void)
{
    ESP_LOGI(TAG, "Command 12: Closing current window...");
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTALT, HID_KEY_F4);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
}

/**
 * Command 13: Open CMD and execute a command
 * Opens Windows Command Prompt and runs a specified command
 */
static void cmd_open_and_execute_cmd(void) {
    ESP_LOGI(TAG, "Command 13: Opening CMD and executing command...");
    
    // Step 1: Open Run dialog (Win+R)
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Step 2: Type "cmd" to open Command Prompt
    keyboard_type_string("cmd");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Step 3: Press Enter to open CMD
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));  // Wait for CMD to open
    
    // Step 4: Execute a command (example: ipconfig)
    keyboard_type_string("ipconfig");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Step 5: Press Enter to execute the command
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    ESP_LOGI(TAG, "Command executed in CMD");
}

/**
 * Command 14: Open CMD as Administrator and execute command
 * Opens Command Prompt with elevated privileges
 */
static void cmd_open_cmd_as_admin(void) {
    ESP_LOGI(TAG, "Command 14: Opening CMD as Administrator...");
    
    // Step 1: Open Run dialog (Win+R)
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Step 2: Type "cmd"
    keyboard_type_string("cmd");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    
    // Step 3: Press Ctrl+Shift+Enter (opens as administrator)
    hid_keyboard_report_t report = {0};
    report.modifier = KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT;
    report.keycode[0] = HID_KEY_ENTER;
    send_keyboard_report(&report);
    vTaskDelay(pdMS_TO_TICKS(KEY_PRESS_DELAY_MS));
    keyboard_release_all();
    
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));  // Wait for UAC prompt
    
    // Step 4: Press Left Arrow and Enter to accept UAC prompt (if needed)
    // Note: This assumes UAC prompt appears and "Yes" is already highlighted
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Step 5: Execute an admin command (example: systeminfo)
    keyboard_type_string("systeminfo");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    ESP_LOGI(TAG, "Admin command executed");
}

/**
 * Command 15: Execute multiple CMD commands in sequence
 * Demonstrates chaining multiple commands with && operator
 */
static void cmd_execute_multiple_commands(void) {
    ESP_LOGI(TAG, "Command 15: Executing multiple CMD commands...");
    
    // Open CMD
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_type_string("cmd");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Execute multiple commands chained with &&
    // Example: Create directory, navigate to it, and create a file
    keyboard_type_string("cd Desktop && mkdir TestFolder && cd TestFolder && echo Hello World > test.txt");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Show directory contents
    keyboard_type_string("dir");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    ESP_LOGI(TAG, "Multiple commands executed");
}

/**
 * Command 16: Open CMD and run a PowerShell command
 * Demonstrates running PowerShell from CMD
 */
static void cmd_run_powershell_from_cmd(void) {
    ESP_LOGI(TAG, "Command 16: Running PowerShell command from CMD...");
    
    // Open CMD
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_type_string("cmd");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    // Run PowerShell command from CMD
    keyboard_type_string("powershell Get-Date");
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));
    
    ESP_LOGI(TAG, "PowerShell command executed from CMD");
}

/**
 * Command N: Execute a CMD command in INVISIBLE mode (no console window).
 *
 * Approach:
 *   Win+R → type: powershell -WindowStyle Hidden -Command cmd /c <cmdline> → Enter
 *
 * Notes:
 *  - Uses only characters supported by your keyboard_type_string():
 *      letters, digits, space, '.', ':', '/', '-', '=', '!' (already in your mapper).
 *  - Works well for commands without quotes. If you need quoting or parentheses
 *    in the future, extend your ASCII→HID mapper accordingly.
 *  - Example call: cmd_execute_hidden("ipconfig") or cmd_execute_hidden("whoami /all")
 */
static void cmd_execute_hidden(const char *cmdline)
{
    ESP_LOGI(TAG, "Command: Execute hidden CMD → %s", (cmdline && *cmdline) ? cmdline : "(null)");

    // Bail out if USB/HID aren't ready (mid-sequence safe with your wrappers)
    if (!usb_is_ready() || !tud_hid_ready()) {
        ESP_LOGW(TAG, "USB/HID not ready; skipping hidden CMD execution");
        return;
    }

    // 1) Open Run dialog
    keyboard_press_key(KEYBOARD_MODIFIER_LEFTGUI, HID_KEY_R);
    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    if (!usb_is_ready() || !tud_hid_ready()) return;

    // 2) Type PowerShell hidden launcher that calls cmd /c <cmdline>
    //    (No quotes used; keep <cmdline> simple to match your current mapper.)
    keyboard_type_string("powershell -WindowStyle Hidden -Command cmd /c ");
    vTaskDelay(pdMS_TO_TICKS(50));
    if (!usb_is_ready() || !tud_hid_ready()) return;

    // 3) Append the user command (fallback to ipconfig if NULL/empty)
    if (cmdline && *cmdline) {
        keyboard_type_string(cmdline);
    } else {
        keyboard_type_string("ipconfig");
    }

    vTaskDelay(pdMS_TO_TICKS(COMMAND_DELAY_MS));
    if (!usb_is_ready() || !tud_hid_ready()) return;

    // 4) Execute
    keyboard_press_key(0, HID_KEY_ENTER);
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS));

    ESP_LOGI(TAG, "Hidden CMD execution requested");
}

/* Optional convenience wrapper consistent with your existing style */
static void cmd_execute_ipconfig_hidden(void)
{
    cmd_execute_hidden("ipconfig");
}

// ============================================================================
// Automation Task
// ============================================================================

/**
* @brief Safely delete the currently running automation task.
*
* This function checks if the automation task handle is valid and deletes the
* task if it exists. After deletion, the task handle is set to NULL to prevent
* dangling references and accidental re-deletion.
*
* This function is typically invoked when an automation sequence completes or
* needs to be stopped due to an error or USB disconnection.
*/
static void delete_automation_task(void) {
    if (automation_task_handle != NULL) {
        vTaskDelete(automation_task_handle);
        automation_task_handle = NULL;
    }
}

/**
 * Automation task - executes the complete command sequence
 *
 * @param pvParameters Task parameters (unused)
 */
static void automation_task(void *pvParameters) {

    // Don't start if USB isn't mounted/ready (prevents WDT)
    if (!usb_wait_ready(USB_READY_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "USB not connected/mounted. Aborting automation.");
        // Visual feedback: quick triple blink
        for (int i = 0; i < 3; ++i) {
            gpio_set_level(LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(80));
            gpio_set_level(LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        automation_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "=== Starting Automation Sequence ===");

    automation_running = true;

    // Turn on LED to indicate automation in progress
    led_send_command(LED_CMD_ON, 0);

    // Initial delay
    vTaskDelay(pdMS_TO_TICKS(LONG_DELAY_MS)); 

    // Execute all commands
    cmd_open_run_dialog();
    cmd_open_text_editor();
    cmd_type_sample_text();
    // cmd_select_all();
    // cmd_copy_text();
    // cmd_new_document();
    // cmd_paste_text();
    // cmd_save_document();
    // cmd_open_browser_url();
    // cmd_take_screenshot();
    // cmd_open_task_manager();
    cmd_close_window();

    // // NEW CMD automation commands
    // cmd_open_and_execute_cmd();
    // // cmd_open_cmd_as_admin();  // Commented - requires UAC interaction
    // cmd_execute_multiple_commands();
    // cmd_run_powershell_from_cmd();

    ESP_LOGI(TAG, "=== Automation Sequence Complete ===");

    // Turn off LED to indicate completion
    automation_running = false;
    led_send_command(LED_CMD_OFF, 0);

    // Delete this task
    automation_task_handle = NULL;
    vTaskDelete(NULL);
}

// ============================================================================
// Button Handling
// ============================================================================

/**
 * GPIO interrupt handler for button press
 * 
 * @param arg Interrupt argument (unused)
 */
static void IRAM_ATTR button_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(button_event_queue, &gpio_num, NULL);
}

/**
 * Button monitoring task
 * Handles button debouncing and triggers automation
 * 
 * @param pvParameters Task parameters (unused)
 */
static void button_task(void *pvParameters) {
    uint32_t io_num;
    uint32_t last_trigger_time = 0;
    
    ESP_LOGI(TAG, "Button task started");
    
    while (1) {
        if (xQueueReceive(button_event_queue, &io_num, portMAX_DELAY)) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Debounce check
            if ((current_time - last_trigger_time) < DEBOUNCE_TIME_MS) {
                continue;
            }
            last_trigger_time = current_time;
            
            // Wait for button release
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
            
            // Verify button is still pressed
            if (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL) {
                // Don't start new automation if one is running
                if (!automation_running && automation_task_handle == NULL) {
                    ESP_LOGI(TAG, "Button pressed - starting automation");
                    
                    // Create automation task
                    xTaskCreate(automation_task, "automation_task", 
                               AUTOMATION_TASK_STACK_SIZE, NULL, 
                               AUTOMATION_TASK_PRIORITY, &automation_task_handle);
                } else {
                    ESP_LOGW(TAG, "Automation already running");
                }
            }
        }
    }
}

// ============================================================================
// Initialization Functions
// ============================================================================

/**
 * Initialize GPIO pins for button and LED
 */
static void init_gpio(void)
{
    // Configure LED GPIO
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&led_conf);
    
    // Turn off LED initially
    gpio_set_level(LED_GPIO, 0);

    // Configure button GPIO
    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE};
    gpio_config(&button_conf);

    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, (void *)BUTTON_GPIO);

    ESP_LOGI(TAG, "GPIO initialized");
}

/**
 * Initialize TinyUSB and HID device
 */
static void init_usb_hid(void)
{
    ESP_LOGI(TAG, "Initializing USB...");

    // Small settle helps on some hosts
    vTaskDelay(pdMS_TO_TICKS(100));

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &_device_desc,
        .configuration_descriptor = _config_desc,
        .string_descriptor = _string_desc,
        .string_descriptor_count = _string_desc_count,
        .external_phy = false,
    };

    // Install TinyUSB driver 
    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(err));
        // avoid instant reboot loop while debugging
        vTaskDelay(pdMS_TO_TICKS(1000));
        return;
    }

    ESP_LOGI(TAG, "USB HID initialized");
}

// ============================================================================
// Main Application Entry Point
// ============================================================================

/**
 * @brief Main application entry point 
 * 
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 USB HID Keyboard with FreeRTOS Starting...");

    // Create synchronization objects
    button_event_queue = xQueueCreate(10, sizeof(uint32_t));
    led_queue = xQueueCreate(10, sizeof(led_message_t));
    keyboard_mutex = xSemaphoreCreateMutex();    
    if (button_event_queue == NULL || led_queue == NULL || keyboard_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create queues or mutex");
        return;
    }

    // Initialize GPIO
    init_gpio();

    // Initialize USB HID
    init_usb_hid();

    // Start LED control task
    xTaskCreate(led_task, "led_task", LED_TASK_STACK_SIZE,
                NULL, LED_TASK_PRIORITY, NULL);

    // Blink LED to indicate ready
    vTaskDelay(pdMS_TO_TICKS(500));
    led_send_command(LED_CMD_BLINK, 3);

    ESP_LOGI(TAG, "System ready! Press button to start automation.");
}