#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_library(fp_storage STATIC "")

if(CONFIG_BT_FAST_PAIR_STORAGE)
target_sources(fp_storage PRIVATE fp_storage.c)
endif()

target_include_directories(fp_storage PUBLIC include)
target_include_directories(fp_storage PUBLIC ../include/common)

target_link_libraries(fp_storage PRIVATE zephyr_interface)
