cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "main.c" "rylr896.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_system freertos ulp
)

#
# ULP RISC-V application — must be declared AFTER idf_component_register().
#
# IMPORTANT: ulp_sources MUST use a relative path (not CMAKE_CURRENT_LIST_DIR
# or any absolute path).  ulp_embed_binary() passes the source path to a
# CMake ExternalProject sub-build; if the path contains spaces (e.g. the
# project lives under "F:/Visual Studio Code Projects/...") an absolute path
# is split by ninja at each space, producing the error:
#
#   ninja: error: 'F:/Visual', needed by '...Visual.ulp.S', missing and no
#   known rule to make it
#
# Using a relative path avoids this because ulp_embed_binary() resolves it
# with get_filename_component(... ABSOLUTE BASE_DIR CMAKE_CURRENT_SOURCE_DIR)
# internally, keeping the quoting intact inside the sub-build.
#
set(ulp_app_name    ulp_lora_scheduler)
set(ulp_sources     "ulp/ulp_lora_scheduler.c")
set(ulp_exp_dep_srcs "main.c")

ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")