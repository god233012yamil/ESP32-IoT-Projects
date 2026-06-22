idf_component_register(
    SRCS
        "app_main.c"
        "i2c_bus.c"
        "i2c_scanner.c"
        "tmp102.c"
        "ssd1306.c"
        "eeprom_24lc256.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)
