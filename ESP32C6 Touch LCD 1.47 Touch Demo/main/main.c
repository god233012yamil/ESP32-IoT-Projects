/**
 * @file main.c
 * 
 * @author Yamil Garcia (https://github.com/god233012yamil)
 * 
 * @brief A clean, minimal touch screen demonstration for the WaveShare ESP32-C6-Touch-LCD-1.47 
 *        development board featuring the AXS5106 capacitive touch controller.
 * 
 * @version 0.1
 * 
 * @date 2025-12-04
 * 
 */
#include <stdio.h>
#include <string.h>
#include "font_5x8.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_jd9853.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_axs5106.h"

// Tag for logging
static const char *TAG = "MAIN";

// Dispplay pin definitions
#define PIN_MOSI        2
#define PIN_SCLK        1
#define PIN_MISO        3
#define PIN_CS          14
#define PIN_DC          15
#define PIN_RST         22
#define PIN_BL          23

// Touch I2C pin definitions
#define PIN_I2C_SDA     18
#define PIN_I2C_SCL     19

// Touch controller pin definitions
#define PIN_TOUCH_INT   21
#define PIN_TOUCH_RST   20

// LCD parameters
#define LCD_WIDTH       172
#define LCD_HEIGHT      320
#define LCD_PIXEL_CLOCK (80 * 1000 * 1000)

// Color definitions in RGB565 format
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F

// Global handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;
static i2c_master_bus_handle_t i2c_bus_handle = NULL;

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
 * @brief Draw a character on the LCD panel
 * 
 * @param c Character to draw
 * @param x X coordinate of the top-left corner
 * @param y Y coordinate of the top-left corner
 * @param color Color of the character
 * @param bg_color Background color of the character
 * @param scale Scale factor for the character size
 */
static void draw_char(char c, int x, int y, uint16_t color, uint16_t bg_color, int scale) {
    int idx = char_to_index(c);
    
    for (int col = 0; col < 5; col++) {
        uint8_t line = font_5x8[idx][col];
        for (int row = 0; row < 8; row++) {
            uint16_t pixel_color = (line & (1 << row)) ? color : bg_color;
            
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
 * @brief Draw a string on the LCD panel
 * 
 * @param str String to draw
 * @param x X coordinate of the top-left corner
 * @param y Y coordinate of the top-left corner
 * @param color Color of the string
 * @param bg_color Background color of the string
 * @param scale Scale factor for the string size
 */
static void draw_string(const char *str, int x, int y, uint16_t color, uint16_t bg_color, int scale) {
    int cursor_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(str[i], cursor_x, y, color, bg_color, scale);
        cursor_x += (6 * scale);
    }
}

/**
 * @brief Fill the entire screen with a single color
 * 
 * @param color Color to fill the screen with
 */
static void fill_screen(uint16_t color)
{
    const int lines_per_chunk = 10;
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
 * @brief Draw a filled circle on the LCD panel
 * 
 * @param cx X coordinate of the circle center
 * @param cy Y coordinate of the circle center
 * @param radius Radius of the circle
 * @param color Color of the circle
 */
static void draw_circle(int cx, int cy, int radius, uint16_t color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                int px = cx + x;
                int py = cy + y;
                if (px >= 0 && px < LCD_WIDTH && py >= 0 && py < LCD_HEIGHT) {
                    esp_lcd_panel_draw_bitmap(panel_handle, px, py, px + 1, py + 1, &color);
                }
            }
        }
    }
}

/**
 * @brief Initialize the backlight using LEDC PWM
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
        .duty = 1024,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    ESP_LOGI(TAG, "Backlight initialized");
}

/**
 * @brief Initialize the display
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise 
 */
static esp_err_t display_init(void)
{
    spi_bus_config_t bus_config = JD9853_PANEL_BUS_SPI_CONFIG(PIN_SCLK, PIN_MOSI, LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    bus_config.miso_io_num = PIN_MISO;
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");
    
    esp_lcd_panel_io_spi_config_t io_config = JD9853_PANEL_IO_SPI_CONFIG(PIN_CS, PIN_DC, NULL, NULL);
    io_config.pclk_hz = LCD_PIXEL_CLOCK;
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));
    ESP_LOGI(TAG, "LCD IO initialized");
    
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_jd9853(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "LCD panel created");
    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 34, 0));
    
    ESP_LOGI(TAG, "Display initialized successfully");
    
    return ESP_OK;
}

/**
 * @brief Initialize the I2C bus for touch controller
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise 
 */
static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = PIN_I2C_SCL,
        .sda_io_num = PIN_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));
    ESP_LOGI(TAG, "I2C bus initialized (SDA=%d, SCL=%d)", PIN_I2C_SDA, PIN_I2C_SCL);
    
    return ESP_OK;
}

/**
 * @brief Initialize the touch controller
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise 
 */
static esp_err_t touch_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ESP_LCD_TOUCH_IO_I2C_AXS5106_ADDRESS,
        .scl_speed_hz = 400000,
    };
    
    i2c_master_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle));
    
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_WIDTH,
        .y_max = LCD_HEIGHT,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };
    
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_axs5106(dev_handle, &tp_cfg, &touch_handle));
    ESP_LOGI(TAG, "Touch initialized (INT=%d, RST=%d)", PIN_TOUCH_INT, PIN_TOUCH_RST);
    
    return ESP_OK;
}

/**
 * @brief Display the initial touch test screen
 * 
 */
static void display_touch_test(void)
{
    fill_screen(COLOR_WHITE);
    draw_string("Touch Test", 25, 80, COLOR_BLACK, COLOR_WHITE, 2);
    draw_string("Mode", 45, 110, COLOR_BLACK, COLOR_WHITE, 2);
    draw_string("Tap anywhere", 10, 160, COLOR_BLUE, COLOR_WHITE, 2);
    draw_string("on screen", 20, 190, COLOR_BLUE, COLOR_WHITE, 2);
}

/**
 * @brief Touch task to handle touch input and update display
 * 
 * @param pvParameters Pointer to task parameters (not used) 
 */
static void touch_task(void *pvParameters)
{
    esp_lcd_touch_point_data_t touchpad_data[1];
    uint8_t touchpad_cnt = 0;
    char coord_str[32];
    int last_x = -1, last_y = -1;
    
    ESP_LOGI(TAG, "Touch task started");
    display_touch_test();
    
    while (1) {
        esp_lcd_touch_read_data(touch_handle);
        
        // Get touch data from touch controller
        esp_err_t ret = esp_lcd_touch_get_data(touch_handle, touchpad_data, &touchpad_cnt, 1);
        
        if (ret == ESP_OK && touchpad_cnt > 0) {
            int x = touchpad_data[0].x;
            int y = touchpad_data[0].y;
            
            if (x != last_x || y != last_y) {
                ESP_LOGI(TAG, "Touch at X=%d, Y=%d", x, y);
                
                fill_screen(COLOR_WHITE);
                draw_string("Touch at:", 25, 80, COLOR_BLACK, COLOR_WHITE, 2);
                
                sprintf(coord_str, "X: %d", x);
                draw_string(coord_str, 40, 120, COLOR_BLUE, COLOR_WHITE, 2);
                
                sprintf(coord_str, "Y: %d", y);
                draw_string(coord_str, 40, 150, COLOR_BLUE, COLOR_WHITE, 2);
                
                draw_circle(x, y, 12, COLOR_RED);
                
                last_x = x;
                last_y = y;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief Main application entry point
 * 
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C6 Touch Demo v6.1");
    
    // Initialize display
    ESP_ERROR_CHECK(display_init());

    // Initialize backlight
    backlight_init();
    
    // Initialize I2C bus
    ESP_ERROR_CHECK(i2c_init());

    // Initialize touch controller
    ESP_ERROR_CHECK(touch_init());
    
    // Create touch handling task
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
