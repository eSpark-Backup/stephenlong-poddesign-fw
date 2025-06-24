# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(POD_v1.0)
target_include_directories(app PRIVATE src)

# # Include application application event headers
# zephyr_library_include_directories(src/events)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

add_subdirectory(src/App)
add_subdirectory(src/MAX17048)
# add_subdirectory(libs)
# add_subdirectory(drivers)


# set(DTC_EXTRA_BINDINGS_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/dts/bindings)

