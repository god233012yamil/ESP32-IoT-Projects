#include "dht.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rom/ets_sys.h"

static const char *TAG = "DHT";

/**
 * @brief Initialize the DHT sensor with the specified GPIO pin and sensor type. 
 *        This function sets up the sensor structure with the provided configuration and 
 *        prepares it for reading data. The GPIO pin will be used to 
 * 
 * @param sensor Pointer to the DHT sensor structure to be initialized 
 * @param gpio_num The GPIO pin number to which the DHT sensor is connected 
 * @param type The type of DHT sensor (e.g., DHT11, DHT22) to be initialized 
 * @return esp_err_t ESP_OK on successful initialization, or an appropriate error code on failure 
 */
esp_err_t dht_init(dht_sensor_t *sensor, gpio_num_t gpio_num, int type) {
    sensor->gpio_num = gpio_num;
    sensor->type = type;
    return ESP_OK;
}

/**
 * @brief Wait for the specified GPIO pin to reach the desired state (HIGH or LOW) within a given timeout period. 
 * 
 * @param pin The GPIO pin number to monitor for the specified state 
 * @param state The desired state to wait for (0 for LOW, 1 for HIGH) 
 * @param timeout_us The maximum time to wait for the pin to reach the desired state, in microseconds 
 * @return esp_err_t ESP_OK if the pin reaches the desired state within the timeout, 
 *         or ESP_ERR_TIMEOUT if the timeout is exceeded without reaching the state 
 */
static esp_err_t dht_await_pin_state(gpio_num_t pin, int state, int timeout_us) {
    int elapsed = 0;
    while (gpio_get_level(pin) != state) {
        if (elapsed > timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        ets_delay_us(1);
        elapsed++;
    }
    return ESP_OK;
}

/**
 * @brief Read temperature and humidity data from the DHT sensor. This function sends the start signal 
 *        to the sensor, waits for the response, and then reads the 40 bits of data representing humidity 
 *        and temperature. It also verifies the checksum to ensure data integrity before parsing the values 
 *        based on the sensor type (DHT11 or DHT22). The read values are returned through the provided pointers. 
 * 
 * @param sensor Pointer to the DHT sensor structure containing the GPIO pin and sensor type information 
 * @param temperature Pointer to a float variable where the read temperature value will be stored 
 * @param humidity Pointer to a float variable where the read humidity value will be stored 
 * @return esp_err_t ESP_OK if the reading is successful and the checksum is valid, or an appropriate error 
 *                   code on failure (e.g., timeout, checksum failure) 
 */
esp_err_t dht_read(dht_sensor_t *sensor, float *temperature, float *humidity) {
    uint8_t data[5] = {0};
    int bit_idx = 0;
    
    // Send start signal
    gpio_set_direction(sensor->gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(sensor->gpio_num, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(sensor->gpio_num, 1);
    ets_delay_us(40);
    
    // Switch to input mode
    gpio_set_direction(sensor->gpio_num, GPIO_MODE_INPUT);
    
    // Wait for DHT response
    if (dht_await_pin_state(sensor->gpio_num, 0, 80) != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for DHT response LOW");
        return ESP_ERR_TIMEOUT;
    }
    if (dht_await_pin_state(sensor->gpio_num, 1, 80) != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for DHT response HIGH");
        return ESP_ERR_TIMEOUT;
    }
    if (dht_await_pin_state(sensor->gpio_num, 0, 80) != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for DHT data start");
        return ESP_ERR_TIMEOUT;
    }
    
    // Read 40 bits of data
    for (int i = 0; i < 40; i++) {
        if (dht_await_pin_state(sensor->gpio_num, 1, 50) != ESP_OK) {
            ESP_LOGE(TAG, "Timeout waiting for bit start");
            return ESP_ERR_TIMEOUT;
        }
        
        ets_delay_us(30);
        
        if (gpio_get_level(sensor->gpio_num) == 1) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
        
        if (dht_await_pin_state(sensor->gpio_num, 0, 50) != ESP_OK) {
            ESP_LOGE(TAG, "Timeout waiting for bit end");
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // Verify checksum
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGE(TAG, "Checksum failed: calculated=0x%02x, received=0x%02x", checksum, data[4]);
        return ESP_ERR_INVALID_CRC;
    }
    
    // Parse data based on sensor type
    if (sensor->type == DHT_TYPE_DHT11) {
        *humidity = data[0];
        *temperature = data[2];
    } else { // DHT22
        *humidity = ((data[0] << 8) | data[1]) / 10.0;
        *temperature = (((data[2] & 0x7F) << 8) | data[3]) / 10.0;
        if (data[2] & 0x80) {
            *temperature = -*temperature;
        }
    }
    
    ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", *temperature, *humidity);
    return ESP_OK;
}