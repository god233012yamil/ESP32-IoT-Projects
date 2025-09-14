idf_component_register(
    SRCS "test.c" "wifi_scanner.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        freertos
        esp_common
        log
)