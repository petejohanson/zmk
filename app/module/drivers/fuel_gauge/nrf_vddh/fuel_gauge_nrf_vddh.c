/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * This is a simplified version of battery_voltage_divider.c which always reads
 * the VDDHDIV5 channel of the &adc node and multiplies it by 5.
 */

#define DT_DRV_COMPAT zmk_battery_nrf_vddh

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/logging/log.h>

#include "battery_common.h"

#include <stdint.h>

LOG_MODULE_REGISTER(nrf_vddh, CONFIG_FUEL_GAUGE_LOG_LEVEL);

#define VDDHDIV (5)

static const struct device *adc = DEVICE_DT_GET(DT_NODELABEL(adc));

struct vddh_data {
    struct adc_channel_cfg acc;
    struct adc_sequence as;
    int32_t adc_raw;
};

static int vddh_get_battery_voltage_mv(const struct device *dev, int *millivolts) {
    struct vddh_data *data = dev->data;
    struct adc_sequence *as = &data->as;

    int ret = adc_read(adc, as);
    as->calibrate = false;

    if (ret != 0) {
        LOG_ERR("Failed to read ADC: %d", ret);
        return ret;
    }

    int32_t adc_mv = data->adc_raw;
    ret = adc_raw_to_millivolts(adc_ref_internal(adc), data->acc.gain, as->resolution, &adc_mv);

    if (ret != 0) {
        LOG_ERR("Failed to convert raw ADC to mV: %d", ret);
        return ret;
    }

    *millivolts = adc_mv * VDDHDIV;
    LOG_DBG("ADC raw %d -> %d mV", data->adc_raw, *millivolts);

    return 0;
}

static int vddh_get_battery_voltage_uv(const struct device *dev, int *microvolts) {
    int millivolts;
    const int ret = vddh_get_battery_voltage_mv(dev, &millivolts);

    if (ret != 0) {
        return ret;
    }

    *microvolts = millivolts * 1000;
    return 0;
}

static int vddh_get_state_of_charge(const struct device *dev, uint8_t *state_of_charge) {
    int millivolts;
    int ret = vddh_get_battery_voltage_mv(dev, &millivolts);

    if (ret != 0) {
        return ret;
    }

    *state_of_charge = lithium_ion_mv_to_pct(millivolts);
    LOG_DBG("Battery %d mV -> %u%%", millivolts, *state_of_charge);

    return 0;
}

static int vddh_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
                         union fuel_gauge_prop_val *val) {
    switch (prop) {
    case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE:
        return vddh_get_state_of_charge(dev, &val->absolute_state_of_charge);

    case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
        return vddh_get_state_of_charge(dev, &val->relative_state_of_charge);

    case FUEL_GAUGE_VOLTAGE:
        return vddh_get_battery_voltage_uv(dev, &val->voltage);

    default:
        return -ENOTSUP;
    }
}

static DEVICE_API(fuel_gauge, vddh_api) = {
    .get_property = vddh_get_prop,
};

static int vddh_init(const struct device *dev) {
    struct vddh_data *data = dev->data;

    if (!device_is_ready(adc)) {
        LOG_ERR("ADC device \"%s\" is not ready", adc->name);
        return -ENODEV;
    }

    data->as = (struct adc_sequence){
        .channels = BIT(0),
        .buffer = &data->adc_raw,
        .buffer_size = sizeof(data->adc_raw),
        .oversampling = 4,
        .calibrate = true,
    };

#ifdef CONFIG_ADC_NRFX_SAADC
    data->acc = (struct adc_channel_cfg){
        .gain = ADC_GAIN_1_2,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
        .input_positive = SAADC_CH_PSELN_PSELN_VDDHDIV5,
    };

    data->as.resolution = 12;
#else
#error Unsupported ADC
#endif

    const int ret = adc_channel_setup(adc, &data->acc);
    if (ret != 0) {
        LOG_ERR("Failed to set up VDDHDIV5 ADC channel: %d", ret);
        return ret;
    }

    return 0;
}

static struct vddh_data vddh_data;

DEVICE_DT_INST_DEFINE(0, &vddh_init, NULL, &vddh_data, NULL, POST_KERNEL,
                      CONFIG_FUEL_GAUGE_INIT_PRIORITY, &vddh_api);
