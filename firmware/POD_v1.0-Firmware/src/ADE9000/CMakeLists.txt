# SPDX-License-Identifier: Apache-2.0

target_include_directories(app PRIVATE .)

# target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ade9000.c)
FILE(GLOB ade_sources *.c)
target_sources(app PRIVATE ${ade_sources})