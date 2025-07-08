# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# zephyr_library_sources(ade9000_POD.c)
zephyr_library_sources_ifdef(CONFIG_ADE9000 ade9000_POD.c)

# Ensure this library waits for offsets.h to be generated
add_dependencies(${ZEPHYR_CURRENT_LIBRARY} offsets_h)