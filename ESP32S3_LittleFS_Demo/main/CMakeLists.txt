idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES littlefs esp_partition vfs spi_flash esp_timer
)