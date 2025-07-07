# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# add_subdirectory(sensor)
# add_subdirectory_ifdef(CONFIG_ADE9000 ADE9000_POD)

add_subdirectory(drivers)

zephyr_include_directories(../include)
zephyr_include_directories(ADE9000_POD)
