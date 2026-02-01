/**
 * @file power_gating.c
 * @brief Power gating driver for external rails (ESP32-C6, ESP-IDF).
 *
 * This module demonstrates firmware-side control patterns for common power gating
 * techniques:
 *
 * - Regulator EN pin gating
 * - Load switch gating
 * - PFET high-side switch using a gate driver stage
 *
 * The firmware interface is intentionally the same across techniques: a single
 * enable GPIO is toggled, and the hardware does the actual gating.
 *
 * Important design rule:
 * The enable signal must default to OFF in hardware (resistor) because GPIO states
 * during reset and deep sleep are not a reliable "guarantee" on all boards.
 */

#include "power_gating.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "pg";

static pg_config_t s_cfg;

/**
 * @brief Convert an integer GPIO number to gpio_num_t safely.
 *
 * @param gpio_num GPIO number as configured.
 * @return gpio_num_t enum value.
 */
static gpio_num_t pg_to_gpio_num(int gpio_num)
{
    return (gpio_num_t)gpio_num;
}

/**
 * @brief Drive the enable GPIO to the "OFF" level.
 *
 * @param cfg Power gating configuration.
 */
static void pg_drive_off(const pg_config_t *cfg)
{
    int level = cfg->active_high ? 0 : 1;
    gpio_set_level(pg_to_gpio_num(cfg->enable_gpio), level);
}

/**
 * @brief Drive the enable GPIO to the "ON" level.
 *
 * @param cfg Power gating configuration.
 */
static void pg_drive_on(const pg_config_t *cfg)
{
    int level = cfg->active_high ? 1 : 0;
    gpio_set_level(pg_to_gpio_num(cfg->enable_gpio), level);
}

void pg_init(const pg_config_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "cfg is NULL");
        return;
    }

    s_cfg = *cfg;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << s_cfg.enable_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io));

    // Start in OFF state to avoid unintended rail power-up.
    pg_drive_off(&s_cfg);

    switch (s_cfg.technique) {
        case PG_TECH_REG_EN:
            ESP_LOGI(TAG, "Technique: Regulator EN pin");
            break;
        case PG_TECH_LOAD_SWITCH:
            ESP_LOGI(TAG, "Technique: Load switch EN");
            break;
        case PG_TECH_PFET_DRIVER:
            ESP_LOGI(TAG, "Technique: PFET driver EN");
            break;
        default:
            ESP_LOGW(TAG, "Technique: Unknown (%d)", (int)s_cfg.technique);
            break;
    }
}

void pg_set_enabled(bool enable)
{
    if (enable) {
        pg_drive_on(&s_cfg);
    } else {
        pg_drive_off(&s_cfg);
    }
}

const pg_config_t *pg_get_config(void)
{
    return &s_cfg;
}