# SPDX-License-Identifier: Apache-2.0

target_include_directories(app PRIVATE .)

FILE(GLOB ade_sources *.c)
target_sources(app PRIVATE ${ade_sources})