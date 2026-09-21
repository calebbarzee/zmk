# Copyright (c) 2022 The ZMK Contributors
# SPDX-License-Identifier: MIT

if (CONFIG_ZMK_SPLIT_BLE)
    add_subdirectory(bluetooth)
endif()

if (CONFIG_ZMK_SPLIT_WIRED)
    add_subdirectory(wired)
endif()

if (CONFIG_ZMK_SPLIT_ROLE_CENTRAL_CAPABLE)
    target_sources(app PRIVATE central.c)
    zephyr_linker_sources(SECTIONS ../../include/linker/zmk-split-transport-central.ld)
endif()

if (CONFIG_ZMK_SPLIT_ROLE_PERIPHERAL_CAPABLE)
    target_sources(app PRIVATE peripheral.c)
    zephyr_linker_sources(SECTIONS ../../include/linker/zmk-split-transport-peripheral.ld)
endif()

target_sources_ifdef(CONFIG_ZMK_SPLIT_ROLE_SWITCHABLE app PRIVATE role.c)