/**
 * @file wifi_scanner.c
 * @brief ESP32 WiFi Scanner Application using ESP-IDF and FreeRTOS
 * 
 * This application demonstrates how to scan for available WiFi networks using the ESP32
 * microcontroller. It utilizes the ESP-IDF framework with FreeRTOS support to create
 * a non-blocking WiFi scanner that periodically scans for networks and displays
 * detailed information about each discovered access point.
 * 
 * Features:
 * - Periodic WiFi scanning with configurable intervals
 * - Detailed network information display (SSID, BSSID, RSSI, Channel, Encryption)
 * - Proper resource management and error handling
 * - FreeRTOS task-based architecture
 * - Memory-safe operations with bounds checking
 * 
 * Hardware Requirements:
 * - ESP32 development board with WiFi capability
 * - Serial monitor for output display
 * 
 * @author ESP32 Developer
 * @version 1.0
 * @date 2024
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

// Configuration constants
#define WIFI_SCAN_INTERVAL_MS    10000  // Scan every 10 seconds
#define MAX_AP_COUNT            20      // Maximum number of APs to scan
#define SCAN_TIMEOUT_MS         5000    // Timeout for scan operation

// FreeRTOS task configuration
#define WIFI_SCANNER_TASK_STACK_SIZE    4096
#define WIFI_SCANNER_TASK_PRIORITY      5

// Logging tag
static const char *TAG = "WiFi_Scanner";

// Global variables
static bool wifi_initialized = false;

/**
 * @brief Convert WiFi authentication mode to human-readable string
 * 
 * This function maps the ESP-IDF WiFi authentication mode enumeration
 * to descriptive strings for better user understanding.
 * 
 * @param authmode The WiFi authentication mode from wifi_auth_mode_t enum
 * @return const char* Human-readable authentication mode string
 */
static const char* get_auth_mode_string(wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN:            return "Open";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3-PSK";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI-PSK";
        default:                        return "Unknown";
    }
}

/**
 * @brief Convert RSSI value to signal strength description
 * 
 * Converts the received signal strength indicator (RSSI) value
 * to a qualitative description for easier interpretation.
 * 
 * @param rssi The RSSI value in dBm
 * @return const char* Signal strength description
 */
static const char* get_signal_strength_string(int8_t rssi) {
    if (rssi >= -30) return "Excellent";
    if (rssi >= -50) return "Very Good";
    if (rssi >= -60) return "Good";
    if (rssi >= -70) return "Fair";
    return "Poor";
}

/**
 * @brief Display detailed information about a WiFi access point
 * 
 * This function formats and prints comprehensive information about
 * a discovered WiFi access point including SSID, BSSID, signal strength,
 * channel, and security information.
 * 
 * @param ap_info Pointer to wifi_ap_record_t structure containing AP information
 * @param index The index number of the access point in the scan results
 */
static void print_ap_info(const wifi_ap_record_t *ap_info, int index) {
    // Convert BSSID to human-readable MAC address format
    char bssid_str[18];
    snprintf(bssid_str, sizeof(bssid_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             ap_info->bssid[0], ap_info->bssid[1], ap_info->bssid[2],
             ap_info->bssid[3], ap_info->bssid[4], ap_info->bssid[5]);
    
    // Print formatted access point information
    printf("┌─ Access Point #%d\n", index + 1);
    printf("├─ SSID: %s\n", ap_info->ssid);
    printf("├─ BSSID: %s\n", bssid_str);
    printf("├─ Channel: %d\n", ap_info->primary);
    printf("├─ RSSI: %d dBm (%s)\n", ap_info->rssi, get_signal_strength_string(ap_info->rssi));
    printf("├─ Security: %s\n", get_auth_mode_string(ap_info->authmode));
    printf("└─ Country: %c%c\n", ap_info->country.cc[0], ap_info->country.cc[1]);
    printf("\n");
}

/**
 * @brief Perform WiFi network scan and display results
 * 
 * This function initiates a WiFi scan operation, waits for completion,
 * retrieves the scan results, and displays detailed information about
 * all discovered access points. It includes proper error handling and
 * memory management.
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
static esp_err_t perform_wifi_scan(void) {
    esp_err_t ret;
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_records = NULL;
    
    ESP_LOGI(TAG, "Starting WiFi scan...");
    
    // Start WiFi scan with default configuration
    wifi_scan_config_t scan_config = {
        .ssid = NULL,           // Scan for all SSIDs
        .bssid = NULL,          // Scan for all BSSIDs  
        .channel = 0,           // Scan all channels
        .show_hidden = true,    // Include hidden networks
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,     // Minimum scan time per channel
                .max = 300      // Maximum scan time per channel
            }
        }
    };
    
    // Initiate the scan operation
    ret = esp_wifi_scan_start(&scan_config, true); // true = blocking scan
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Get the number of access points found
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP count: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Scan completed. Found %d access points", ap_count);
    
    if (ap_count == 0) {
        printf("No WiFi networks found.\n");
        return ESP_OK;
    }
    
    // Limit the number of APs to process to prevent memory issues
    if (ap_count > MAX_AP_COUNT) {
        ESP_LOGW(TAG, "Too many APs found (%d), limiting to %d", ap_count, MAX_AP_COUNT);
        ap_count = MAX_AP_COUNT;
    }
    
    // Allocate memory for AP records
    ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_records) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP records");
        return ESP_ERR_NO_MEM;
    }
    
    // Retrieve scan results
    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get AP records: %s", esp_err_to_name(ret));
        free(ap_records);
        return ret;
    }
    
    // Display header for scan results
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                            WiFi Scan Results\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("Found %d WiFi networks:\n\n", ap_count);
    
    // Display information for each access point
    for (int i = 0; i < ap_count; i++) {
        print_ap_info(&ap_records[i], i);
    }
    
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    // Clean up allocated memory
    free(ap_records);
    
    return ESP_OK;
}

/**
 * @brief Initialize WiFi subsystem for scanning operations
 * 
 * This function performs complete WiFi initialization required for
 * scanning operations, including NVS initialization, network interface
 * setup, event loop initialization, and WiFi driver configuration.
 * 
 * @return esp_err_t ESP_OK on success, error code on failure
 */
static esp_err_t wifi_scan_init(void) {
    esp_err_t ret;
    
    if (wifi_initialized) {
        ESP_LOGW(TAG, "WiFi already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing WiFi for scanning...");
    
    // Initialize NVS (Non-Volatile Storage) - required for WiFi
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_LOGW(TAG, "NVS partition needs to be erased, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize TCP/IP adapter (network interface)
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create default event loop
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create default WiFi station interface
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif) {
        ESP_LOGE(TAG, "Failed to create WiFi station interface");
        return ESP_FAIL;
    }
    
    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set WiFi mode to station (client mode)
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Start WiFi driver
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized successfully for scanning");
    
    return ESP_OK;
}

/**
 * @brief Main WiFi scanner task implementation
 * 
 * This FreeRTOS task runs continuously, performing periodic WiFi scans
 * at configured intervals. It handles initialization, error recovery,
 * and provides continuous monitoring of available WiFi networks.
 * 
 * @param pvParameters Task parameters (unused)
 */
static void wifi_scanner_task(void *pvParameters) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "WiFi Scanner Task started");
    
    // Initialize WiFi subsystem
    ret = wifi_scan_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi, terminating task");
        vTaskDelete(NULL);
        return;
    }
    
    // Main scanning loop
    while (1) {
        ESP_LOGI(TAG, "═══ Starting new scan cycle ═══");
        
        // Perform WiFi scan and display results
        ret = perform_wifi_scan();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(ret));
        }
        
        ESP_LOGI(TAG, "Next scan in %d seconds...", WIFI_SCAN_INTERVAL_MS / 1000);
        
        // Wait for the configured interval before next scan
        vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVAL_MS));
    }
}

/**
 * @brief Application entry point
 * 
 * Main application function that creates and starts the WiFi scanner task.
 * This function sets up the FreeRTOS environment and initiates the
 * continuous WiFi scanning operation.
 */
void app_main(void) {
    ESP_LOGI(TAG, "ESP32 WiFi Scanner Application Starting...");
    ESP_LOGI(TAG, "Scan interval: %d seconds", WIFI_SCAN_INTERVAL_MS / 1000);
    ESP_LOGI(TAG, "Maximum APs to display: %d", MAX_AP_COUNT);
    
    // Create WiFi scanner task
    BaseType_t task_created = xTaskCreate(
        wifi_scanner_task,                  // Task function
        "wifi_scanner",                     // Task name
        WIFI_SCANNER_TASK_STACK_SIZE,      // Stack size
        NULL,                              // Parameters
        WIFI_SCANNER_TASK_PRIORITY,        // Priority
        NULL                               // Task handle
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi scanner task");
        return;
    }
    
    ESP_LOGI(TAG, "WiFi scanner task created successfully");
    
    // The scheduler is already running, so the task will start automatically
    // This function returns and the task continues to run in the background
}