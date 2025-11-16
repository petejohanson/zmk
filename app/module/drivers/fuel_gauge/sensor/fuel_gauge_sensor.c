/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_fuel_gauge_sensor

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(fuel_gauge_sensor, CONFIG_FUEL_GAUGE_LOG_LEVEL);

struct fuel_gauge_sensor_config {
    const struct device *sensor;
};

static int get_battery_voltage_uv(const struct device *dev, int *microvolts) {
    const struct fuel_gauge_sensor_config *config = dev->config;

    int ret = sensor_sample_fetch_chan(config->sensor, SENSOR_CHAN_GAUGE_VOLTAGE);
    if (ret != 0) {
        return ret;
    }

    struct sensor_value val;
    ret = sensor_channel_get(config->sensor, SENSOR_CHAN_GAUGE_VOLTAGE, &val);
    if (ret != 0) {
        LOG_ERR("Failed to get voltage: %d", ret);
        return ret;
    }

    *microvolts = val.val1 * 1000000 + val.val2;
    return 0;
}

static int get_state_of_charge(const struct device *dev, uint8_t *state_of_charge) {
    const struct fuel_gauge_sensor_config *config = dev->config;

    int ret = sensor_sample_fetch_chan(config->sensor, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
    if (ret != 0) {
        return ret;
    }

    struct sensor_value val;
    ret = sensor_channel_get(config->sensor, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &val);
    if (ret != 0) {
        LOG_ERR("Failed to get state of charge: %d", ret);
        return ret;
    }

    *state_of_charge = val.val1;
    return 0;
}

static int fuel_gauge_sensor_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
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

static DEVICE_API(fuel_gauge, fuel_gauge_sensor_api) = {
    .get_property = fuel_gauge_sensor_get_prop,
};

static int fuel_gauge_sensor_init(const struct device *dev) {
    const struct fuel_gauge_sensor_config *config = dev->config;

    if (!device_is_ready(config->sensor)) {
        LOG_ERR("Fuel gauge sensor \"%s\" is not ready", config->sensor->name);
        return -ENODEV;
    }

    return 0;
}

#define FUEL_GAUGE_SENSOR_INIT(n)                                                                  \
    static const struct fuel_gauge_sensor_config fuel_gauge_sensor_config_##n = {                  \
        .sensor = DEVICE_DT_GET(DT_INST_PHANDLE(n, sensor)),                                       \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, &fuel_gauge_sensor_init, NULL, NULL, &fuel_gauge_sensor_config_##n,   \
                          POST_KERNEL, CONFIG_FUEL_GAUGE_INIT_PRIORITY, &fuel_gauge_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(FUEL_GAUGE_SENSOR_INIT)