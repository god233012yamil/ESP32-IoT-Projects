#ifndef DHT_H
#define DHT_H

#include "driver/gpio.h"
#include "esp_err.h"

#define DHT_TYPE_DHT11 0
#define DHT_TYPE_DHT22 1

typedef struct {
    gpio_num_t gpio_num;
    int type;
} dht_sensor_t;

esp_err_t dht_init(dht_sensor_t *sensor, gpio_num_t gpio_num, int type);
esp_err_t dht_read(dht_sensor_t *sensor, float *temperature, float *humidity);

#endif // DHT_H