#include "i2c_bus.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "i2c_bus";
static i2c_master_bus_handle_t s_bus_handle;

/**
 * @brief Converts the configured integer port into an ESP-IDF port value.
 *
 * @return Configured I2C controller number.
 */
static i2c_port_num_t get_i2c_port(void)
{
    return (i2c_port_num_t)CONFIG_APP_I2C_PORT;
}

/**
 * @brief Initializes the I2C bus.
 *
 * @return ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_i2c_bus_init(void)
{
    if (s_bus_handle != NULL) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = get_i2c_port(),
        .sda_io_num = (gpio_num_t)CONFIG_APP_I2C_SDA_GPIO,
        .scl_io_num = (gpio_num_t)CONFIG_APP_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = CONFIG_APP_I2C_ENABLE_INTERNAL_PULLUPS,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_bus_handle),
                        TAG,
                        "Failed to create I2C master bus");

    ESP_LOGI(TAG,
             "I2C bus initialized: port=%d SDA=%d SCL=%d",
             CONFIG_APP_I2C_PORT,
             CONFIG_APP_I2C_SDA_GPIO,
             CONFIG_APP_I2C_SCL_GPIO);
    return ESP_OK;
}

/**
 * @brief Deinitializes the I2C bus.
 *
 * @return ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_i2c_bus_deinit(void)
{
    if (s_bus_handle == NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(i2c_del_master_bus(s_bus_handle),
                        TAG,
                        "Failed to delete I2C master bus");

    s_bus_handle = NULL;
    return ESP_OK;
}

/**
 * @brief Gets the handle to the I2C bus.
 *
 * @return Handle to the I2C bus.
 */
i2c_master_bus_handle_t app_i2c_bus_get_handle(void)
{
    return s_bus_handle;
}

/**
 * @brief Adds a device to the I2C bus.
 *
 * @param address The 7-bit I2C address of the device.
 * @param scl_speed_hz The SCL speed in Hertz.
 * @param device_handle Output pointer to the device handle.
 * @return ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_i2c_bus_add_device(uint16_t address,
                                 uint32_t scl_speed_hz,
                                 i2c_master_dev_handle_t *device_handle)
{
    ESP_RETURN_ON_FALSE(s_bus_handle != NULL,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "I2C bus is not initialized");
    ESP_RETURN_ON_FALSE(device_handle != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Device handle output pointer is NULL");
    ESP_RETURN_ON_FALSE(address <= 0x7F,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Invalid seven-bit I2C address");

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = scl_speed_hz,
    };

    return i2c_master_bus_add_device(s_bus_handle,
                                     &device_config,
                                     device_handle);
}

/**
 * @brief Removes a device from the I2C bus.
 *
 * @param device_handle The handle to the device to remove.
 * @return ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_i2c_bus_remove_device(i2c_master_dev_handle_t device_handle)
{
    ESP_RETURN_ON_FALSE(device_handle != NULL,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Device handle is NULL");
    return i2c_master_bus_rm_device(device_handle);
}

/**
 * @brief Probes an I2C address to check if a device is present.
 *
 * @param address The 7-bit I2C address to probe.
 * @param timeout_ms The timeout in milliseconds.
 * @return ESP_OK if successful, otherwise an error code.
 */
esp_err_t app_i2c_bus_probe(uint16_t address, int timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_bus_handle != NULL,
                        ESP_ERR_INVALID_STATE,
                        TAG,
                        "I2C bus is not initialized");
    ESP_RETURN_ON_FALSE(address <= 0x7F,
                        ESP_ERR_INVALID_ARG,
                        TAG,
                        "Invalid seven-bit I2C address");

    return i2c_master_probe(s_bus_handle, address, timeout_ms);
}
