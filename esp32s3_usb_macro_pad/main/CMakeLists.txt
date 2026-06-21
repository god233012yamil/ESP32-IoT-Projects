idf_component_register(
    SRCS
        "app_main.c"
        "button_driver.c"
        "macro_engine.c"
        "usb_hid_keyboard.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio
)
