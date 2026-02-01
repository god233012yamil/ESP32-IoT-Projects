/**
 * @file bus_safe.c
 * @brief Bus-safe GPIO handling to prevent phantom powering when gating rails.
 *
 * When a peripheral is unpowered but still connected to ESP32-C6 GPIOs (I2C/SPI/UART),
 * current can flow through the peripheral's ESD diodes and partially power it.
 *
 * This module provides a simple mitigation: set bus pins to INPUT and disable
 * internal pull resistors before cutting the peripheral rail.
 *
 * This does not replace correct hardware design (pullups on gated rails, bus switches),
 * but it is a practical and often necessary firmware step.
 */

#include "bus_safe.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "bus_safe";

static bus_safe_config_t s_cfg;

/**
 * @brief Configure one GPIO into a safe (high-Z) input state.
 *
 * @param gpio_num GPIO number to configure.
 */
static void bus_safe_gpio_to_hiz(int gpio_num)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
}

void bus_safe_init(const bus_safe_config_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "cfg is NULL");
        return;
    }

    s_cfg = *cfg;

    // Set bus GPIOs to high-Z inputs to avoid phantom powering.
    bus_safe_gpio_to_hiz(s_cfg.i2c_scl_gpio);
    bus_safe_gpio_to_hiz(s_cfg.i2c_sda_gpio);

    ESP_LOGI(TAG, "Bus-safe init: SCL=%d SDA=%d", s_cfg.i2c_scl_gpio, s_cfg.i2c_sda_gpio);
}

void bus_safe_apply_before_power_off(void)
{
    // Set bus GPIOs to high-Z inputs to avoid phantom powering.
    bus_safe_gpio_to_hiz(s_cfg.i2c_scl_gpio);
    bus_safe_gpio_to_hiz(s_cfg.i2c_sda_gpio);

    ESP_LOGI(TAG, "Bus-safe applied before power-off");
}