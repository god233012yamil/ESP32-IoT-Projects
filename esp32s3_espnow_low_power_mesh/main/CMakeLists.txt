idf_component_register(
    SRCS "main.c" "mesh_protocol.c" "power_manager.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_timer
)
