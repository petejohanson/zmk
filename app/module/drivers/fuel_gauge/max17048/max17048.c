/*
 * Copyright (c) 2022 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_maxim_max17048

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/fuel_gauge.h>

#include "max17048.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(max17048, CONFIG_FUEL_GAUGE_LOG_LEVEL);

static int read_register(const struct device *dev, uint8_t reg, uint16_t *value) {

    if (k_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    struct max17048_config *config = (struct max17048_config *)dev->config;

    uint8_t data[2] = {0};
    int ret = i2c_burst_read_dt(&config->i2c_bus, reg, &data[0], sizeof(data));
    if (ret != 0) {
        LOG_DBG("i2c_write_read FAIL %d\n", ret);
        return ret;
    }

    // the register values are returned in big endian (MSB first)
    *value = sys_get_be16(data);
    return 0;
}

static int write_register(const struct device *dev, uint8_t reg, uint16_t value) {

    if (k_is_in_isr()) {
        return -EWOULDBLOCK;
    }

    struct max17048_config *config = (struct max17048_config *)dev->config;

    uint8_t data[2] = {0};
    sys_put_be16(value, &data[0]);

    return i2c_burst_write_dt(&config->i2c_bus, reg, &data[0], sizeof(data));
}

static int set_rcomp_value(const struct device *dev, uint8_t rcomp_value) {

    struct max17048_drv_data *const drv_data = (struct max17048_drv_data *const)dev->data;
    k_sem_take(&drv_data->lock, K_FOREVER);

    uint16_t tmp = 0;
    int err = read_register(dev, REG_CONFIG, &tmp);
    if (err != 0) {
        goto done;
    }

    tmp = ((uint16_t)rcomp_value << 8) | (tmp & 0xFF);
    err = write_register(dev, REG_CONFIG, tmp);
    if (err != 0) {
        goto done;
    }

    LOG_DBG("set RCOMP to %d", rcomp_value);

done:
    k_sem_give(&drv_data->lock);
    return err;
}

static int set_sleep_enabled(const struct device *dev, bool sleep) {

    struct max17048_drv_data *const drv_data = (struct max17048_drv_data *const)dev->data;
    k_sem_take(&drv_data->lock, K_FOREVER);

    uint16_t tmp = 0;
    int err = read_register(dev, REG_CONFIG, &tmp);
    if (err != 0) {
        goto done;
    }

    if (sleep) {
        tmp |= 0x80;
    } else {
        tmp &= ~0x0080;
    }

    err = write_register(dev, REG_CONFIG, tmp);
    if (err != 0) {
        goto done;
    }

    LOG_DBG("sleep mode %s", sleep ? "enabled" : "disabled");

done:
    k_sem_give(&drv_data->lock);
    return err;
}

static int max17048_get_state_of_charge(const struct device *dev, uint8_t *state_of_charge) {
    struct max17048_drv_data *const data = dev->data;
    k_sem_take(&data->lock, K_FOREVER);

    uint16_t raw_state_of_charge;
    int err = read_register(dev, REG_STATE_OF_CHARGE, &raw_state_of_charge);
    if (err != 0) {
        LOG_WRN("failed to read state of charge: %d", err);
        goto done;
    }

    LOG_DBG("read soc: %u", raw_state_of_charge);

    *state_of_charge = raw_state_of_charge >> 8;

done:
    k_sem_give(&data->lock);
    return err;
}

static int max17048_get_voltage(const struct device *dev, int *microvolts) {
    struct max17048_drv_data *const data = dev->data;
    k_sem_take(&data->lock, K_FOREVER);

    uint16_t raw_vcell;
    int err = read_register(dev, REG_VCELL, &raw_vcell);
    if (err != 0) {
        LOG_WRN("failed to read vcell: %d", err);
        goto done;
    }

    LOG_DBG("read vcell: %u", raw_vcell);

    // 1250 / 16 = 78.125
    *microvolts = raw_vcell * 1250 / 16;

done:
    k_sem_give(&data->lock);
    return err;
}

static int max17048_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
                             union fuel_gauge_prop_val *val) {
    switch (prop) {
    case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE:
        return max17048_get_state_of_charge(dev, &val->absolute_state_of_charge);

    case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
        return max17048_get_state_of_charge(dev, &val->relative_state_of_charge);

    case FUEL_GAUGE_VOLTAGE:
        return max17048_get_voltage(dev, &val->voltage);

    default:
        return -ENOTSUP;
    }
}

static int max17048_init(const struct device *dev) {
    struct max17048_drv_data *drv_data = dev->data;
    const struct max17048_config *config = dev->config;

    if (!device_is_ready(config->i2c_bus.bus)) {
        LOG_WRN("i2c bus not ready!");
        return -EINVAL;
    }

    uint16_t ic_version = 0;
    int err = read_register(dev, REG_VERSION, &ic_version);
    if (err != 0) {
        LOG_WRN("could not get IC version!");
        return err;
    }

    // the functions below need the semaphore, so initialise it here
    k_sem_init(&drv_data->lock, 1, 1);

    // bring the device out of sleep
    set_sleep_enabled(dev, false);

    // set the default rcomp value -- 0x97, as stated in the datasheet
    set_rcomp_value(dev, 0x97);

    LOG_INF("device initialised at 0x%x (version %d)", config->i2c_bus.addr, ic_version);

    return 0;
}

static DEVICE_API(fuel_gauge, max17048_api) = {
    .get_property = max17048_get_prop,
};

#define MAX17048_INIT(inst)                                                                        \
    static const struct max17048_config max17048_##inst##_config = {                               \
        .i2c_bus = I2C_DT_SPEC_INST_GET(inst),                                                     \
    };                                                                                             \
                                                                                                   \
    static struct max17048_drv_data max17048_##inst##_drvdata;                                     \
                                                                                                   \
    /* This has to init after SPI master */                                                        \
    DEVICE_DT_INST_DEFINE(inst, max17048_init, NULL, &max17048_##inst##_drvdata,                   \
                          &max17048_##inst##_config, POST_KERNEL, CONFIG_FUEL_GAUGE_INIT_PRIORITY, \
                          &max17048_api);

DT_INST_FOREACH_STATUS_OKAY(MAX17048_INIT)
