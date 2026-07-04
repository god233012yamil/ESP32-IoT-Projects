idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES pid_controller esp_driver_gpio esp_driver_ledc
)
