/**
 * @file main.c
 * @author Yamil Garcia (https://github.com/god233012yamil)
 * @brief 
 * @version 0.1
 * @date 2025-12-04
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "font_5x8.h"
#include "font_8x12.h"
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_jd9853.h"

// Tag for logging
static const char *TAG = "MAIN";

// WiFi Configuration - CHANGE THESE TO YOUR NETWORK
#define WIFI_SSID      "GodIsTheLord"
#define WIFI_PASSWORD  "08@God@2330"

// WiFi event group
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static EventGroupHandle_t wifi_event_group;
static int wifi_retry_num = 0;
#define MAX_WIFI_RETRY 5

// Pin definitions
#define PIN_MOSI        2
#define PIN_SCLK        1
#define PIN_MISO        3
#define PIN_CS          14
#define PIN_DC          15
#define PIN_RST         22
#define PIN_BL          23
#define PIN_BUTTON      9

// Display orientation settings
#define DISPLAY_PORTRAIT_MODE   1
#define DISPLAY_LANDSCAPE_MODE  2
#define DISPLAY_ORIENTATION     DISPLAY_LANDSCAPE_MODE

// Display settings
#if DISPLAY_ORIENTATION == DISPLAY_PORTRAIT_MODE
    #define LCD_WIDTH   172
    #define LCD_HEIGHT  320     
#elif DISPLAY_ORIENTATION == DISPLAY_LANDSCAPE_MODE
    #define LCD_WIDTH   320
    #define LCD_HEIGHT  172
#else
    #error "Invalid DISPLAY_ORIENTATION value. Use DISPLAY_PORTRAIT_MODE or DISPLAY_LANDSCAPE_MODE."
#endif
#define LCD_PIXEL_CLOCK (80 * 1000 * 1000)

// Colors (RGB565 format)
#define COLOR_BLACK     0x0000 
#define COLOR_WHITE     0xFFFF 
#define COLOR_RED       0x00F8
#define COLOR_GREEN     0xE007
#define COLOR_BLUE      0x1F00
#define COLOR_YELLOW    0xE0FF
#define COLOR_CYAN      0xFF07
#define COLOR_MAGENTA   0x1FF8
#define COLOR_ORANGE    0x20FD
#define COLOR_PURPLE    0x1080
#define COLOR_PURPLE_2  0xFFE0

// Set foreground and background colors
#define FOREGROUND_COLOR COLOR_YELLOW 
#define BACKGROUND_COLOR COLOR_BLACK 

// Scaling factor for fonts
#define FONT_SCALE  3//2  

#define FONT_8x5    1    
#define FONT_8x12   2
#define SELECTED_FONT FONT_8x5

// Character width based on selected font
#if SELECTED_FONT == FONT_8x5
    #define CHAR_WIDTH 5
    #define CHAR_HEIGHT 8
#elif SELECTED_FONT == FONT_8x12
    #define CHAR_WIDTH 8
    #define CHAR_HEIGHT 12
#else
    ESP_LOGE(TAG, "No font selected. Define SELECTED_FONT as FONT_8x5 or FONT_8x12.");
#endif 

// Global handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

// WiFi and time sync variables
static bool time_synced = false;

// Prototypes functions
static void fill_screen(uint16_t color);
static int char_to_index(char c);
static void draw_char(char c, int x, int y, uint16_t color, uint16_t bg_color, int scale);
static void draw_char_8x5(char c, int x, int y, uint16_t color, uint16_t bg_color, int scale);
static void draw_char_8x12(char c, int x, int y, uint16_t color, uint16_t bg_color, int scale);
static void draw_string(const char *str, int x, int y, uint16_t color, uint16_t bg_color, int scale);
static void fill_screen(uint16_t color);
static void backlight_init(void);
static esp_err_t display_portrait_init(void);
static esp_err_t display_landscape_init(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
static esp_err_t wifi_init_sta(void);
void time_sync_notification_cb(struct timeval *tv);
static void sntp_initialize(void);
static void get_formatted_time(char *date_str, char *time_str);
static void display_datetime(void);
static void display_connecting(void);
static void display_failed(void);
static void time_display_task(void *pvParameters);

/**
 * @brief Map a character to its corresponding font index.
 * 
 * @param c The character to map.
 * @return int The index of the character in the font array.
 */
static int char_to_index(char c) {
      if (c < 32 || c > 126) {
          return 0;  // Return space for invalid characters
      }
      return c - 32;
}

/**
 * @brief Draw a character at the specified position with given colors and scale.
 * 
 * @param c The character to draw.
 * @param x The x-coordinate of the top-left corner where the character will be drawn.
 * @param y The y-coordinate of the top-left corner where the character will be drawn.
 * @param color The color of the character pixels.
 * @param bg_color The background color for the character pixels.
 * @param scale The scaling factor for the character size.
 */
static void draw_char(char c, int x, int y, uint16_t color, uint16_t bg_color, int scale) {
#if SELECTED_FONT == FONT_8x5
    draw_char_8x5(c, x, y, color, bg_color, scale);
#elif SELECTED_FONT == FONT_8x12
    draw_char_8x12(c, x, y, color, bg_color, scale);
#endif
}

/**
 * @brief Draw a character using the 8x5 font at the specified position with given colors and scale.
 * 
 * @param c The character to draw.
 * @param x The x-coordinate of the top-left corner where the character will be drawn.
 * @param y The y-coordinate of the top-left corner where the character will be drawn.
 * @param color The color of the character pixels.
 * @param bg_color The background color for the character pixels.
 * @param scale The scaling factor for the character size.
 */
static void draw_char_8x5(char c, int x, int y, uint16_t color, uint16_t bg_color, int scale) {
    int idx = char_to_index(c);
    
    for (int col = 0; col < 5; col++) {
        uint8_t line = font_5x8[idx][col];
        for (int row = 0; row < 8; row++) {
            uint16_t pixel_color = (line & (1 << row)) ? color : bg_color;
            
            // Draw scaled pixel
            for (int sx = 0; sx < scale; sx++) {
                for (int sy = 0; sy < scale; sy++) {
                    int px = x + col * scale + sx;
                    int py = y + row * scale + sy;
                    if (px < LCD_WIDTH && py < LCD_HEIGHT) {
                        esp_lcd_panel_draw_bitmap(panel_handle, px, py, px + 1, py + 1, &pixel_color);
                    }
                }
            }
        }
    }
}

/**
 * @brief Draw a character using the 8x12 font at the specified position with given colors and scale.
 * 
 * @param c The character to draw.
 * @param x The x-coordinate of the top-left corner where the character will be drawn.
 * @param y The y-coordinate of the top-left corner where the character will be drawn.
 * @param color The color of the character pixels.
 * @param bg_color The background color for the character pixels.
 * @param scale The scaling factor for the character size.
 */
static void draw_char_8x12(char c, int x, int y, uint16_t color, uint16_t bg_color, int scale) {
    int idx = char_to_index(c);
    
    for (int row = 0; row < 12; row++) {
        uint8_t line = font_8x12[idx][row];
        for (int col = 0; col < 8; col++) {
            uint16_t pixel_color = (line & (1 << col)) ? color : bg_color;
            
            for (int sx = 0; sx < scale; sx++) {
                for (int sy = 0; sy < scale; sy++) {
                    int px = x + col * scale + sx;
                    int py = y + row * scale + sy;
                    if (px < LCD_WIDTH && py < LCD_HEIGHT) {
                        esp_lcd_panel_draw_bitmap(panel_handle, px, py, px + 1, py + 1, &pixel_color);
                    }
                }
            }
        }
    }
}

/**
 * @brief Draw a string at the specified position with given colors and scale.
 * 
 * @param str The string to draw.
 * @param x The x-coordinate of the top-left corner where the string will start.
 * @param y The y-coordinate of the top-left corner where the string will start.
 * @param color The color of the character pixels.
 * @param bg_color The background color for the character pixels.
 * @param scale The scaling factor for the character size.
 */
static void draw_string(const char *str, int x, int y, uint16_t color, uint16_t bg_color, int scale) {
    int cursor_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(str[i], cursor_x, y, color, bg_color, scale);
        cursor_x += (6 * scale); // 5 pixels + 1 pixel spacing
    }
}

/**
 * @brief Fill the entire screen with a specified color.
 * 
 * @param color The color to fill the screen with.
 */
static void fill_screen(uint16_t color)
{
    const int lines_per_chunk = 1;//10;
    uint16_t *buffer = malloc(LCD_WIDTH * lines_per_chunk * sizeof(uint16_t));
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return;
    }
    
    for (int i = 0; i < LCD_WIDTH * lines_per_chunk; i++) {
        buffer[i] = color;
    }
    
    for (int y = 0; y < LCD_HEIGHT; y += lines_per_chunk) {
        int lines = (y + lines_per_chunk <= LCD_HEIGHT) ? lines_per_chunk : (LCD_HEIGHT - y);
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_WIDTH, y + lines, buffer);
    }
    
    free(buffer);
}

/**
 * @brief Initialize the backlight with PWM control.
 * 
 */
static void backlight_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = PIN_BL,
        .duty = 1024,  // 100% brightness
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    ESP_LOGI(TAG, "Backlight initialized on GPIO %d", PIN_BL);
}

/**
 * @brief Initialize the display in portrait mode
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise 
 */
static esp_err_t display_portrait_init(void)
{
    // Initialize SPI bus
    spi_bus_config_t bus_config = JD9853_PANEL_BUS_SPI_CONFIG(PIN_SCLK, PIN_MOSI, LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    bus_config.miso_io_num = PIN_MISO;
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");
    
    // Initialize LCD panel IO
    esp_lcd_panel_io_spi_config_t io_config = JD9853_PANEL_IO_SPI_CONFIG(PIN_CS, PIN_DC, NULL, NULL);
    io_config.pclk_hz = LCD_PIXEL_CLOCK;
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));
    ESP_LOGI(TAG, "LCD IO initialized");
    
    // Initialize LCD panel
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9853(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "LCD panel created");
    
    // Reset and initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_LOGI(TAG, "Display orientation: portrait (swap_xy=false)");
    
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
    ESP_LOGI(TAG, "Display gap set: x=34, y=0 (portrait mode)");
    
    ESP_LOGI(TAG, "Display initialized successfully!");
    
    return ESP_OK;
}

/**
 * @brief Initialize the display in landscape mode
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise 
 */
static esp_err_t display_landscape_init(void)
{
    // Initialize SPI bus
    spi_bus_config_t bus_config = JD9853_PANEL_BUS_SPI_CONFIG(PIN_SCLK, PIN_MOSI, LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    bus_config.miso_io_num = PIN_MISO;
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");
    
    // Initialize LCD panel IO
    esp_lcd_panel_io_spi_config_t io_config = JD9853_PANEL_IO_SPI_CONFIG(PIN_CS, PIN_DC, NULL, NULL);
    io_config.pclk_hz = LCD_PIXEL_CLOCK;
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));
    ESP_LOGI(TAG, "LCD IO initialized");
    
    // Initialize LCD panel
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9853(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "LCD panel created");
    
    // Reset and initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    
    // LANDSCAPE MODE CONFIGURATION
    // For 90-degree rotation (landscape), we need:
    // 1. swap_xy = true (swap X and Y axes)
    // 2. mirror_x = true (flip horizontally)
    // 3. mirror_y = false (no vertical flip)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_LOGI(TAG, "Display orientation: landscape (swap_xy=true, mirror_x=true)");
    
    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    
    // CRITICAL: Set gap for landscape mode
    // Landscape uses (0, 34) instead of portrait's (34, 0)
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 34));
    ESP_LOGI(TAG, "Display gap set: x=0, y=34 (landscape mode)");
    
    ESP_LOGI(TAG, "Display initialized successfully in LANDSCAPE mode (320×172)");
    
    return ESP_OK;
}

/**
 * @brief Handle WiFi events
 * 
 * @param arg User-defined argument
 * @param event_base Event base
 * @param event_id Event ID
 * @param event_data Event data
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < MAX_WIFI_RETRY) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (attempt %d/%d)", wifi_retry_num, MAX_WIFI_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Connect to the AP failed");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize WiFi in station mode   
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise 
 */
static esp_err_t wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization finished.");

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
}

/**
 * @brief SNTP time synchronization notification callback
 * 
 * @param tv Pointer to timeval structure
 */
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized!");
    time_synced = true;
}

/**
 * @brief Initialize SNTP
 * 
 */
static void sntp_initialize(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

/**
 * @brief Get current time and date formatted for display
 * 
 * @param date_str Pointer to buffer for formatted date string
 * @param time_str Pointer to buffer for formatted time string
 */
static void get_formatted_time(char *date_str, char *time_str)
{
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format date: "Dec 03, 2024"
    strftime(date_str, 32, "%b %d %Y", &timeinfo);
    
    // Format time: "03:45:30 PM"
    strftime(time_str, 32, "%I:%M:%S %p", &timeinfo);
}

/**
 * @brief Display the current date and time on the screen
 * 
 */
static void display_datetime(void)
{
    char date_str[32];
    char time_str[32];
    
    // Get formatted date and time
    get_formatted_time(date_str, time_str);

    // Calculate starting Y position to center the text block
    int num_lines = 2;
    int text_height = 8 * FONT_SCALE; // Each character is 8 pixels tall in the 5x8 font
    int line_spacing = 3; // Spacing between lines
    int total_text_height = (text_height * num_lines) + (line_spacing * (num_lines - 1));
    int start_y = (LCD_HEIGHT - total_text_height) / 2;

    // Display line 1 centered
    int line_1_len = strlen(date_str);
    int line_1_x = (LCD_WIDTH - (line_1_len * (CHAR_WIDTH * FONT_SCALE))) / 2; 
    draw_string(date_str, line_1_x, start_y, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);

    // Display line 2 centered
    int line_2_len = strlen(time_str);
    int line_2_x = (LCD_WIDTH - (line_2_len * (CHAR_WIDTH * FONT_SCALE))) / 2; 
    draw_string(time_str, line_2_x, start_y + text_height + line_spacing, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);
}

/**
 * @brief Display connecting message on the screen
 * 
 */
static void display_connecting(void)
{
    // Clear screen
    fill_screen(BACKGROUND_COLOR);

    // Calculate starting Y position to center the text block
    int num_lines = 2;
    int text_height = 8 * FONT_SCALE; // Each character is 8 pixels tall in the 5x8 font
    int line_spacing = 3; // Spacing between lines
    int total_text_height = (text_height * num_lines) + (line_spacing * (num_lines - 1));
    int start_y = (LCD_HEIGHT - total_text_height) / 2;

    // Display line 1 centered
    char line_1[] = "Connecting";
    int line_1_len = strlen(line_1);
    int line_1_x = (LCD_WIDTH - (line_1_len * (CHAR_WIDTH * FONT_SCALE))) / 2; 
    draw_string(line_1, line_1_x, start_y, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);

    // Display line 2 centered
    char line_2[] = "to WiFi...";
    int line_2_len = strlen(line_2);
    int line_2_x = (LCD_WIDTH - (line_2_len * (CHAR_WIDTH * FONT_SCALE))) / 2; 
    draw_string(line_2, line_2_x, start_y + text_height + line_spacing, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);
}

/**
 * @brief Display connection failed message on the screen
 * 
 */
static void display_failed(void)
{
    // Clear screen
    fill_screen(BACKGROUND_COLOR);

    // Calculate starting Y position to center the text block
    int num_lines = 3;
    int text_height = 8 * FONT_SCALE; // Each character is 8 pixels tall in the 5x8 font
    int line_spacing = 3; // Spacing between lines
    int total_text_height = (text_height * num_lines) + (line_spacing * (num_lines - 1));
    int start_y = (LCD_HEIGHT - total_text_height) / 2;

    // Display line 1 centered
    char line_1[] = "WiFi";
    int line_1_len = strlen(line_1);
    int line_1_x = (LCD_WIDTH - (line_1_len * (CHAR_WIDTH * FONT_SCALE))) / 2; 
    draw_string(line_1, line_1_x, start_y, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);

    // Display line 2 centered
    char line_2[] = "Connection";   
    int line_2_len = strlen(line_2);
    int line_2_x = (LCD_WIDTH - (line_2_len * (CHAR_WIDTH * FONT_SCALE))) / 2; 
    draw_string(line_2, line_2_x, start_y + text_height + line_spacing, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);

    // Display line 3 centered
    char line_3[] = "Failed!";
    int line_3_len = strlen(line_3);
    int line_3_x = (LCD_WIDTH - (line_3_len * (CHAR_WIDTH * FONT_SCALE))) / 2; 
    draw_string(line_3, line_3_x, start_y + 2 * (text_height + line_spacing), FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);
}

/**
 * @brief Task to update time display every second  
 * 
 * @param pvParameters Pointer to task parameters (not used) 
 */
static void time_display_task(void *pvParameters)
{
    fill_screen(BACKGROUND_COLOR);
    while (1) {
        if (time_synced) {
            display_datetime();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
    }
}

/**
 * @brief Main application entry point
 * 
 */
void app_main(void)
{
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "ESP32-C6 WiFi Clock");
    ESP_LOGI(TAG, "====================================");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize display based on orientation
    #if DISPLAY_ORIENTATION == DISPLAY_PORTRAIT_MODE
        // Initialize display in portrait mode
        ESP_ERROR_CHECK(display_portrait_init());
    #elif DISPLAY_ORIENTATION == DISPLAY_LANDSCAPE_MODE
        // Initialize display in landscape mode
        ESP_ERROR_CHECK(display_landscape_init());
    #else   
        ESP_LOGE(TAG, "Invalid DISPLAY_ORIENTATION value");
    #endif 

    // Initialize backlight
    backlight_init();
    
    // Show connecting message
    display_connecting();
    
    // Initialize WiFi
    ret = wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed");
        display_failed();
        return;
    }
    
    // Set timezone to Miami, USA (EST/EDT)
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to Miami, USA (EST/EDT)");
    
    // Initialize SNTP
    sntp_initialize();
    
    // Wait for time to be synchronized
    ESP_LOGI(TAG, "Waiting for time synchronization...");
    int retry = 0;
    while (!time_synced && retry < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry++;
    }
    
    if (time_synced) {
        ESP_LOGI(TAG, "Time synchronized successfully");
        fill_screen(BACKGROUND_COLOR);
        // Create task to update display
        xTaskCreate(time_display_task, "time_display", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "Time synchronization failed");
        fill_screen(BACKGROUND_COLOR);
        draw_string("Time Sync", 30, 120, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);
        draw_string("Failed!", 40, 150, FOREGROUND_COLOR, BACKGROUND_COLOR, FONT_SCALE);
    }
    
    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}