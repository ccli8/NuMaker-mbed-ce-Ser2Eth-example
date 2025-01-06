# Copyright (c) 2022 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19)

# Set default path for Mbed OS library (no force-write for override)
set(MBED_OS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/mbed-os CACHE INTERNAL "")

# Set default path for mbed_app.json5 (no force-write for override)
set(MBED_APP_JSON_PATH mbed_app.json5 CACHE INTERNAL "")

# Set default path for custom_targets.json5 (no force-write for override)
set(CUSTOM_TARGETS_PATH custom_targets CACHE INTERNAL "")
set(CUSTOM_TARGETS_JSON_PATH ${CUSTOM_TARGETS_PATH}/custom_targets.json5 CACHE INTERNAL "")

# Include Mbed toolchain setup file
include(mbed-os/tools/cmake/mbed_toolchain_setup.cmake)

set(APP_PROJECT NUMAKER_MBED_CE_SER2ETH_EXAMPLE)
set(APP_TARGET NuMaker-mbed-ce-Ser2Eth-example)

# Set up project name
project(${APP_PROJECT})

# Include Mbed project setup file
include(mbed_project_setup)

# Add Mbed OS library
add_subdirectory(${MBED_OS_PATH})

# Add other libraries
if("COMPONENT_NUSD=1" IN_LIST MBED_TARGET_DEFINITIONS)
    add_subdirectory(drivers/COMPONENT_NUSD)
endif()

add_executable(${APP_TARGET})

target_include_directories(${APP_TARGET}
    PRIVATE
        .
)

target_sources(${APP_TARGET}
    PRIVATE
        main.cpp
        uweb_server.cpp
)

target_link_libraries(${APP_TARGET}
    PRIVATE
        mbed-os
        mbed-netsocket-api
        mbed-storage-fat
        $<$<IN_LIST:COMPONENT_NUSD=1,${MBED_TARGET_DEFINITIONS}>:nusd>
)

# Must call this for each target to set up bin file creation, code upload, etc
mbed_set_post_build(${APP_TARGET})
