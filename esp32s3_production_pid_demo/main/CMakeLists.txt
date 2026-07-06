idf_component_register(
    SRCS "main.c" "simulated_motor.c"
    INCLUDE_DIRS "."
    REQUIRES pid_controller esp_driver_ledc esp_driver_gpio esp_timer nvs_flash
)
