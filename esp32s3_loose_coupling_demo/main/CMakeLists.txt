idf_component_register(
    SRCS
        "app_main.c"
        "button_input.c"
        "climate_controller.c"
        "event_queue.c"
        "fake_temperature.c"
        "logger.c"
        "pwm_fan.c"
    INCLUDE_DIRS "."
)
