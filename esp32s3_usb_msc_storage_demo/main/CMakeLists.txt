idf_component_register(
    SRCS "usb_msc_storage_demo.c"
    INCLUDE_DIRS "."
    REQUIRES esp_partition fatfs wear_levelling
)
