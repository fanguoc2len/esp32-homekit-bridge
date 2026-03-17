cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH is not set. Export the ESP-IDF environment before building esp32_homekit.")
endif()

if(NOT DEFINED ENV{ESP_HOMEKIT_SDK_PATH})
    message(FATAL_ERROR "ESP_HOMEKIT_SDK_PATH is not set. Point it to your Espressif esp-homekit-sdk checkout.")
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components"
    "$ENV{ESP_HOMEKIT_SDK_PATH}/components/homekit"
    "$ENV{ESP_HOMEKIT_SDK_PATH}/components/button"
    "$ENV{ESP_HOMEKIT_SDK_PATH}/examples/common/qrcode")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_homekit)
