# Copyright (c) 2025 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_include_directories(.)

zephyr_library()

zephyr_library_sources(battery_common.c)
add_subdirectory_ifdef(CONFIG_ZMK_MAX17048 max17048)
add_subdirectory_ifdef(CONFIG_ZMK_BATTERY_NRF_VDDH nrf_vddh)
add_subdirectory_ifdef(CONFIG_ZMK_FUEL_GAUGE_SENSOR sensor)
add_subdirectory_ifdef(CONFIG_ZMK_FUEL_GAUGE_VOLTAGE battery_voltage)
