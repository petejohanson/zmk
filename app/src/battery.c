/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/activity.h>
#include <zmk/workqueue.h>

enum battery_device_api {
    BATTERY_DEVICE_API_UNKNOWN,
    BATTERY_DEVICE_API_SENSOR,
    BATTERY_DEVICE_API_FUEL_GAUGE,
};

static enum battery_device_api battery_device_api = BATTERY_DEVICE_API_UNKNOWN;

static uint8_t last_state_of_charge = 0;

uint8_t zmk_battery_state_of_charge(void) { return last_state_of_charge; }

#if DT_HAS_CHOSEN(zmk_battery)
static const struct device *const battery = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
#else
#warning                                                                                           \
    "Using a node labeled BATTERY for the battery sensor is deprecated. Set a zmk,battery chosen node instead. (Ignore this if you don't have a battery sensor.)"
static const struct device *battery;
#endif

#if IS_ENABLED(CONFIG_FUEL_GAUGE)

static int get_state_of_charge_fuel_gauge(uint8_t *state_of_charge) {
    union fuel_gauge_prop_val value;
    int ret = fuel_gauge_get_prop(battery, FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE, &value);
    if (ret != 0) {
        LOG_WRN("Failed to read battery state of charge: %d", ret);
        return ret;
    }

    *state_of_charge = value.absolute_state_of_charge;
    return ret;
}

#endif // IS_ENABLED(CONFIG_FUEL_GAUGE)

#if IS_ENABLED(CONFIG_SENSOR)

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING_FETCH_MODE_LITHIUM_VOLTAGE)
static uint8_t lithium_ion_mv_to_pct(int16_t bat_mv) {
    // Simple linear approximation of a battery based off adafruit's discharge graph:
    // https://learn.adafruit.com/li-ion-and-lipoly-batteries/voltages

    if (bat_mv >= 4200) {
        return 100;
    } else if (bat_mv <= 3450) {
        return 0;
    }

    return bat_mv * 2 / 15 - 459;
}

#endif // IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING_FETCH_MODE_LITHIUM_VOLTAGE)

static int get_state_of_charge_sensor(uint8_t *state_of_charge) {
    struct sensor_value value;
    enum sensor_channel channel;

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING_FETCH_MODE_STATE_OF_CHARGE)
    channel = SENSOR_CHAN_GAUGE_STATE_OF_CHARGE;
#elif IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING_FETCH_MODE_LITHIUM_VOLTAGE)
    channel = SENSOR_CHAN_VOLTAGE;
#else
#error "Not a supported reporting fetch mode"
#endif

    int ret = sensor_sample_fetch_chan(battery, channel);
    if (ret != 0) {
        LOG_WRN("Failed to fetch battery values: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(battery, channel, &value);
    if (ret != 0) {
        LOG_WRN("Failed to read battery sensor: %d", ret);
        return ret;
    }

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING_FETCH_MODE_STATE_OF_CHARGE)
    *state_of_charge = value.val1;
#elif IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING_FETCH_MODE_LITHIUM_VOLTAGE)
    uint16_t mv = value.val1 * 1000 + (value.val2 / 1000);
    *state_of_charge = lithium_ion_mv_to_pct(mv);

    LOG_DBG("State of charge = %d%% from %d mv", state_of_charge, mv);
#endif

    return ret;
}

#endif // IS_ENABLED(CONFIG_SENSOR)

static int get_state_of_charge(uint8_t *state_of_charge) {
    switch (battery_device_api) {
    case BATTERY_DEVICE_API_FUEL_GAUGE:
#if IS_ENABLED(CONFIG_FUEL_GAUGE)
        return get_state_of_charge_fuel_gauge(state_of_charge);
#else
        return -ENOTSUP;
#endif

    case BATTERY_DEVICE_API_SENSOR:
#if IS_ENABLED(CONFIG_SENSOR)
        return get_state_of_charge_sensor(state_of_charge);
#else
        return -ENOTSUP;
#endif

    default:
        return -ENOTSUP;
    }
}

static int zmk_battery_update(void) {
    uint8_t new_state_of_charge;
    int rc = get_state_of_charge(&new_state_of_charge);

    if (rc != 0) {
        return rc;
    }

    if (last_state_of_charge != new_state_of_charge) {
        last_state_of_charge = new_state_of_charge;

        rc = raise_zmk_battery_state_changed(
            (struct zmk_battery_state_changed){.state_of_charge = last_state_of_charge});

        if (rc != 0) {
            LOG_ERR("Failed to raise battery state changed event: %d", rc);
            return rc;
        }
    }

#if IS_ENABLED(CONFIG_BT_BAS)
    if (bt_bas_get_battery_level() != last_state_of_charge) {
        LOG_DBG("Setting BAS GATT battery level to %d.", last_state_of_charge);

        rc = bt_bas_set_battery_level(last_state_of_charge);

        if (rc != 0) {
            LOG_WRN("Failed to set BAS GATT battery level (err %d)", rc);
            return rc;
        }
    }
#endif

    return rc;
}

static void zmk_battery_work(struct k_work *work) {
    int rc = zmk_battery_update();

    if (rc != 0) {
        LOG_DBG("Failed to update battery value: %d.", rc);
    }
}

K_WORK_DEFINE(battery_work, zmk_battery_work);

static void zmk_battery_timer(struct k_timer *timer) {
    k_work_submit_to_queue(zmk_workqueue_lowprio_work_q(), &battery_work);
}

K_TIMER_DEFINE(battery_timer, zmk_battery_timer, NULL);

static void zmk_battery_start_reporting() {
    if (device_is_ready(battery)) {
        k_timer_start(&battery_timer, K_NO_WAIT, K_SECONDS(CONFIG_ZMK_BATTERY_REPORT_INTERVAL));
    }
}

static int init_battery_device_api(void) {
    if (DEVICE_API_IS(fuel_gauge, battery)) {
        battery_device_api = BATTERY_DEVICE_API_FUEL_GAUGE;
        LOG_DBG("Using fuel gauge API for battery");
        return 0;
    }

    if (DEVICE_API_IS(sensor, battery)) {
        battery_device_api = BATTERY_DEVICE_API_SENSOR;
        LOG_DBG("Using sensor API for battery");
        return 0;
    }

    LOG_ERR("Battery device \"%s\" has unsupported API", battery->name);
    return -ENOTSUP;
}

static int zmk_battery_init(void) {
#if !DT_HAS_CHOSEN(zmk_battery)
    battery = device_get_binding("BATTERY");

    if (battery == NULL) {
        return -ENODEV;
    }

    LOG_WRN("Finding battery device labeled BATTERY is deprecated. Use zmk,battery chosen node.");
#endif

    if (!device_is_ready(battery)) {
        LOG_ERR("Battery device \"%s\" is not ready", battery->name);
        return -ENODEV;
    }

    int ret = init_battery_device_api();
    if (ret != 0) {
        return ret;
    }

    zmk_battery_start_reporting();
    return 0;
}

static int battery_event_listener(const zmk_event_t *eh) {

    if (as_zmk_activity_state_changed(eh)) {
        switch (zmk_activity_get_state()) {
        case ZMK_ACTIVITY_ACTIVE:
            zmk_battery_start_reporting();
            return 0;
        case ZMK_ACTIVITY_IDLE:
        case ZMK_ACTIVITY_SLEEP:
            k_timer_stop(&battery_timer);
            return 0;
        default:
            break;
        }
    }
    return -ENOTSUP;
}

ZMK_LISTENER(battery, battery_event_listener);

ZMK_SUBSCRIPTION(battery, zmk_activity_state_changed);

SYS_INIT(zmk_battery_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
