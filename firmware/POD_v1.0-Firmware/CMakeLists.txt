# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(QEMU_EXTRA_FLAGS -s)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(Base_Project)

target_sources(app PRIVATE src/main.c)

# add_library(pod_libraries STATIC
#     my_lib/src/file1.c
#     my_lib/src/file2.c
# )
# target_include_directories(pod_libraries PUBLIC my_lib/include)
# target_link_libraries(app pod_libraries)

