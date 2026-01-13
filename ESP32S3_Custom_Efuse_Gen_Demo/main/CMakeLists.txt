idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "include"
)

# -----------------------------------------------------------------------------
# Auto-generate esp_efuse_custom_table.c/.h from esp_efuse_custom_table.csv.
#
# This project demonstrates using the eFuse table generator as part of the build.
# The generator invocation matches the ESP-IDF documentation:
#
#   cd $IDF_PATH/components/efuse/
#   ./efuse_table_gen.py --idf_target esp32s3 esp32s3/esp_efuse_table.csv PROJECT_PATH/main/esp_efuse_custom_table.csv
#
# The generator writes:
#   - PROJECT_PATH/main/esp_efuse_custom_table.c
#   - PROJECT_PATH/main/include/esp_efuse_custom_table.h
# -----------------------------------------------------------------------------

set(CUSTOM_EFUSE_CSV "${CMAKE_CURRENT_SOURCE_DIR}/esp_efuse_custom_table.csv")
set(GEN_C           "${CMAKE_CURRENT_SOURCE_DIR}/esp_efuse_custom_table.c")
set(GEN_H           "${CMAKE_CURRENT_SOURCE_DIR}/include/esp_efuse_custom_table.h")

# Script and common table paths inside ESP-IDF.
# IDF_PATH and IDF_TARGET are provided by the ESP-IDF build environment.
set(EFUSE_GEN_SCRIPT "$ENV{IDF_PATH}/components/efuse/efuse_table_gen.py")
set(EFUSE_COMMON_CSV "$ENV{IDF_PATH}/components/efuse/${IDF_TARGET}/esp_efuse_table.csv")

add_custom_command(
    OUTPUT
        "${GEN_C}"
        "${GEN_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/include"
    COMMAND ${PYTHON} "${EFUSE_GEN_SCRIPT}" --idf_target ${IDF_TARGET} "${EFUSE_COMMON_CSV}" "${CUSTOM_EFUSE_CSV}"
    DEPENDS
        "${CUSTOM_EFUSE_CSV}"
    WORKING_DIRECTORY
        "$ENV{IDF_PATH}/components/efuse"
    VERBATIM
)

# Ensure generation runs before compiling this component.
add_custom_target(gen_custom_efuse_table ALL DEPENDS "${GEN_C}" "${GEN_H}")
add_dependencies(${COMPONENT_LIB} gen_custom_efuse_table)

# Compile the generated C file as part of this component.
target_sources(${COMPONENT_LIB} PRIVATE "${GEN_C}")

# Ensure the generated header is visible.
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

set_source_files_properties("${GEN_C}" PROPERTIES GENERATED TRUE)
set_source_files_properties("${GEN_H}" PROPERTIES GENERATED TRUE)