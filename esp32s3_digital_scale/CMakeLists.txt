cmake_minimum_required(VERSION 3.16)

# Set the IDF_PATH if not already set (for CI/CD environments)
# export IDF_PATH=~/esp/esp-idf

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32s3_digital_scale)
