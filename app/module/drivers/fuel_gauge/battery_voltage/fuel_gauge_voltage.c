/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_fuel_gauge_voltage

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "battery_common.h"

LOG_MODULE_REGISTER(battery_voltage, CONFIG_FUEL_GAUGE_LOG_LEVEL);

struct battery_voltage_config {
    const struct device *sensor;
};

static int get_battery_voltage_uv(const struct device *dev, int *microvolts) {
    const struct battery_voltage_config *config = dev->config;

    int ret = sensor_sample_fetch_chan(config->sensor, SENSOR_CHAN_VOLTAGE);
    if (ret != 0) {
        LOG_ERR("Failed to fetch voltage sample: %d", ret);
        return ret;
    }

    struct sensor_value val;
    ret = sensor_channel_get(config->sensor, SENSOR_CHAN_VOLTAGE, &val);
    if (ret != 0) {
        LOG_ERR("Failed to get voltage: %d", ret);
        return ret;
    }

    *microvolts = val.val1 * 1000000 + val.val2;
    return 0;
}

static int get_state_of_charge(const struct device *dev, uint8_t *state_of_charge) {
    int microvolts;
    int ret = get_battery_voltage_uv(dev, &microvolts);

    if (ret != 0) {
        return ret;
    }

    const int millivolts = microvolts / 1000;

    *state_of_charge = lithium_ion_mv_to_pct(millivolts);
    LOG_DBG("Battery %d mV -> %u%%", millivolts, *state_of_charge);

    return 0;
}

static int battery_voltage_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
                                    union fuel_gauge_prop_val *val) {
    switch (prop) {
    case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE:
        return get_state_of_charge(dev, &val->absolute_state_of_charge);

    case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
        return get_state_of_charge(dev, &val->relative_state_of_charge);

    case FUEL_GAUGE_VOLTAGE:
        return get_battery_voltage_uv(dev, &val->voltage);

    default:
        return -ENOTSUP;
    }
}

static DEVICE_API(fuel_gauge, battery_voltage_api) = {
    .get_property = battery_voltage_get_prop,
};

static int battery_voltage_init(const struct device *dev) {
    const struct battery_voltage_config *config = dev->config;

    if (!device_is_ready(config->sensor)) {
        LOG_ERR("Battery voltage sensor \"%s\" is not ready", config->sensor->name);
        return -ENODEV;
    }

    return 0;
}

#define BATTERY_VOLTAGE_INIT(n)                                                                    \
    static const struct battery_voltage_config battery_voltage_config_##n = {                      \
        .sensor = DEVICE_DT_GET(DT_INST_PHANDLE(n, sensor)),                                       \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, &battery_voltage_init, NULL, NULL, &battery_voltage_config_##n,       \
                          POST_KERNEL, CONFIG_FUEL_GAUGE_INIT_PRIORITY, &battery_voltage_api);

DT_INST_FOREACH_STATUS_OKAY(BATTERY_VOLTAGE_INIT)