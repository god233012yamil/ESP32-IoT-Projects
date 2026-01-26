idf_component_register(
    SRCS "main.c" "wifi_station.c" "ota_manager.c"
    INCLUDE_DIRS "."
)

target_add_binary_data(${COMPONENT_LIB} "server_root_cert.pem" TEXT)