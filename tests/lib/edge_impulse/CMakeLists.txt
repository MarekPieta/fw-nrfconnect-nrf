#
# Copyright (c) 2020 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#

cmake_minimum_required(VERSION 3.13.1)

file(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/src/edge_impulse_dummy.zip)
add_custom_target(create_zip COMMAND
    ${CMAKE_COMMAND} -E tar "cf"
    "../edge_impulse_dummy.zip" --format=zip
    "./"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/edge_impulse_zip/")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Ensure that zip will be packed before Edge Impulse archive is unzipped.
add_dependencies(edge_impulse_project_download create_zip)

project("Edge Impulse test")

target_sources(app PRIVATE src/main.cpp)
# Test uses ei_test_params.h file from edge_impuse_zip directory to verify if
# ei_ncs properly forwards the data between application and EI library.
target_include_directories(app PRIVATE src/edge_impulse_zip/)
