#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "dht.h"

// WiFi Configuration
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASS      "YOUR_WIFI_PASSWORD"
#define WIFI_MAX_RETRY 5

// ThingsBoard Configuration
#define THINGSBOARD_HOST   "thingsboard.cloud"
#define THINGSBOARD_PORT   1883
#define THINGSBOARD_TOKEN  "YOUR_DEVICE_TOKEN"

// DHT Sensor Configuration
#define DHT_GPIO       GPIO_NUM_4
#define DHT_TYPE       DHT_TYPE_DHT11 //DHT_TYPE_DHT22  // Change to DHT_TYPE_DHT11 if using DHT11

// Reading interval in seconds
#define READING_INTERVAL_SEC 10

static const char *TAG = "MAIN";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static dht_sensor_t dht_sensor;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MQTT_CONNECTED_BIT BIT2

/**
 * @brief WiFi event handler to manage connection and disconnection events 
 * 
 * @param arg User-defined argument (not used in this case)   
 * @param event_base Event base for the WiFi events 
 * @param event_id ID of the WiFi event (e.g., start, disconnected) 
 * @param event_data Data associated with the WiFi event (e.g., connection details, error information) 
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * @brief Initialize WiFi in station mode and connect to the specified SSID and password 
 * 
 */
static esp_err_t wifi_init_sta(void)
{
    // Create an event group to manage WiFi connection events and states. This allows the main 
    // application to wait for specific events (e.g., connected, failed) before proceeding with MQTT initialization.
    s_wifi_event_group = xEventGroupCreate();

    // Initialize the TCP/IP stack and the default event loop, and create a default WiFi station network interface.
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Configure WiFi initialization settings and initialize the WiFi driver with the specified configuration.
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for WiFi and IP events to manage connection status 
    // and handle IP acquisition.
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

    // Configure WiFi connection settings (SSID and password) and set the 
    // authentication mode to WPA2-PSK.
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Set WiFi configuration and connect to the AP using the specified SSID and password. 
    // This will trigger the WiFi event handler to manage the connection
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Start WiFi and wait for connection
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // Wait for the connection to the AP to be established or failed, and set the 
    // appropriate event bits based on the outcome. This allows the main application to 
    // proceed with MQTT initialization only after a successful Wi
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    // Check the event bits to determine if the connection was successful or if it failed, 
    // and log the appropriate message. This provides feedback on the WiFi connection status 
    // and allows for troubleshooting if needed.
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
}

/**
 * @brief MQTT event handler to manage MQTT connection events and errors 
 * 
 * @param handler_args Arguments passed to the event handler (not used in this case) 
 * @param base Event base for the MQTT events 
 * @param event_id ID of the MQTT event (e.g., connected, disconnected, error) 
 * @param event_data Data associated with the MQTT event (e.g., connection details, error information) 
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        // Set MQTT_CONNECTED_BIT on successful connection to indicate that the client is 
        // connected to the MQTT broker. This allows the sensor task to start publishing telemetry data.
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
            break;
        // Clear MQTT_CONNECTED_BIT on disconnection to indicate that the client is no longer 
        // connected to the MQTT broker. This allows the sensor task to
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(s_wifi_event_group, MQTT_CONNECTED_BIT);
            break;
        // Handle other MQTT events as needed (e.g., published, subscribed, data received)
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            break;
    }
}

/**
 * @brief Initialize the MQTT client with the specified broker URI and credentials, 
 *        register event handlers, and start the MQTT client 
 * 
 */
static void mqtt_init(void)
{
    char uri[128];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", THINGSBOARD_HOST, THINGSBOARD_PORT);
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials.username = THINGSBOARD_TOKEN,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    
    // Register event handler for MQTT events
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // Start the MQTT client
    esp_mqtt_client_start(mqtt_client);
    
    ESP_LOGI(TAG, "MQTT client initialized and started");
}

/**
 * @brief Publish the temperature and humidity telemetry data to ThingsBoard using MQTT 
 * 
 * @param temperature The temperature value to be published 
 * @param humidity The humidity value to be published 
 */
static void publish_telemetry(float temperature, float humidity)
{
    char payload[128];

    // Create JSON payload with temperature and humidity values
    snprintf(payload, sizeof(payload), 
             "{\"temperature\":%.1f,\"humidity\":%.1f}", 
             temperature, humidity);
    
    // Publish telemetry data to ThingsBoard
    int msg_id = esp_mqtt_client_publish(mqtt_client, 
                                         "v1/devices/me/telemetry", 
                                         payload, 
                                         0, 
                                         1, 
                                         0);
    if (msg_id > 0) {
        ESP_LOGI(TAG, "Published telemetry: %s", payload);
    } else {
        ESP_LOGE(TAG, "Failed to publish telemetry");
    }
}

/**
 * @brief Task to read temperature and humidity from the DHT sensor at regular intervals, 
 * 
 * @param pvParameters 
 */
static void sensor_task(void *pvParameters)
{
    float temperature = 0.0;
    float humidity = 0.0;
    
    while (1) {
        // Wait for MQTT connection
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                                MQTT_CONNECTED_BIT,
                                                pdFALSE,
                                                pdFALSE,
                                                portMAX_DELAY);
        
        // If MQTT is connected, read sensor data and publish telemetry
        if (bits & MQTT_CONNECTED_BIT) {
            // Read data from DHT sensor
            esp_err_t result = dht_read(&dht_sensor, &temperature, &humidity);
            
            // If reading is successful, publish telemetry to ThingsBoard
            if (result == ESP_OK) {
                publish_telemetry(temperature, humidity);
            } else {
                ESP_LOGE(TAG, "Failed to read DHT sensor: %s", esp_err_to_name(result));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(READING_INTERVAL_SEC * 1000));
    }
}

/**
 * @brief Main application entry point: initializes NVS, WiFi, MQTT, and starts the sensor reading task 
 * 
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 ThingsBoard DHT Sensor");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize DHT sensor
    ESP_LOGI(TAG, "Initializing DHT sensor on GPIO %d", DHT_GPIO);
    dht_init(&dht_sensor, DHT_GPIO, DHT_TYPE);
    
    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi");
    // Initialize WiFi
    if (wifi_init_sta() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed");
        return;
    }
    
    // Initialize MQTT
    ESP_LOGI(TAG, "Initializing MQTT");
    mqtt_init();
    
    // Create sensor reading task
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Application started successfully");
}