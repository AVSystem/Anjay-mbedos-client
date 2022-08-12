#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020-2022 AVSystem <avsystem@avsystem.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.19.0)

set(MBED_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed-os CACHE INTERNAL "")
set(MBED_CONFIG_PATH ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "")
set(APP_TARGET anjay-mbedos-client)

include(${MBED_PATH}/tools/cmake/app.cmake)

function(get_config_definition DEFNAME)
    set(RESULT "${MBED_CONFIG_DEFINITIONS}")
    list(FILTER RESULT INCLUDE REGEX "^${DEFNAME}=")
    string(REGEX REPLACE "^${DEFNAME}=" "" RESULT "${RESULT}")
    set("${DEFNAME}" "${RESULT}" PARENT_SCOPE)
endfunction()

# Always generate both *.bin and *.hex files
set(MBED_OUTPUT_EXT "")

get_config_definition(MBED_CONF_TARGET_APP_OFFSET)
if(NOT MBED_CONF_TARGET_APP_OFFSET STREQUAL "")
    message(FATAL_ERROR "Bootloaders are not yet supported in the Mbed CLI 2 build system")
endif()
project(${APP_TARGET})

add_subdirectory(${MBED_PATH})
add_subdirectory(anjay-mbedos)

add_executable(${APP_TARGET}
               accelerometer.cpp
               barometer.cpp
               conn_monitoring_object.cpp
               device_config_serial_menu.cpp
               device_object.cpp
               fw_update.cpp
               humidity.cpp
               joystick.cpp
               magnetometer.cpp
               main.cpp
               persistence.cpp
               serial_menu.cpp
               sms_driver.cpp)

target_link_libraries(${APP_TARGET}
                      mbed-os
                      mbed-storage-qspif
                      mbed-storage-tdbstore
                      anjay-mbedos)

get_config_definition(SENSORS_IKS01A2)
if(SENSORS_IKS01A2)
    file(GLOB_RECURSE IKS01A2_C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/X_NUCLEO_IKS01A2/*.c")
    file(GLOB_RECURSE IKS01A2_CXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/X_NUCLEO_IKS01A2/*.cpp")
    target_sources(${APP_TARGET} PRIVATE
                   ${IKS01A2_C_SOURCES}
                   ${IKS01A2_CXX_SOURCES})

    file(GLOB_RECURSE IKS01A2_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/X_NUCLEO_IKS01A2/*.h")
    string(REGEX REPLACE "[\\/][^\\/;]*[.]h" "" IKS01A2_INCLUDE_DIRS "${IKS01A2_INCLUDE_DIRS}")
    list(REMOVE_DUPLICATES IKS01A2_INCLUDE_DIRS)
    target_include_directories(${APP_TARGET} PRIVATE ${IKS01A2_INCLUDE_DIRS})
endif()

mbed_set_post_build(${APP_TARGET})

option(VERBOSE_BUILD "Have a verbose build process")
if(VERBOSE_BUILD)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()
