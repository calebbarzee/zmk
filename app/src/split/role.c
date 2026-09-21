/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>

#include <zephyr/logging/log.h>

#if DT_HAS_ZMK_SPLIT_ROLE_SWITCH_ENABLED
#include <zephyr/drivers/gpio.h>
#endif

#include <zmk/split/role.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static enum zmk_split_role role =
    IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) ? ZMK_SPLIT_ROLE_CENTRAL : ZMK_SPLIT_ROLE_PERIPHERAL;

/* Set at boot, before settings load, when a role switch GPIO is present. Its
 * reading wins over any role stored in settings for the rest of this boot. */
static bool role_forced_by_switch;

static const char *role_name(enum zmk_split_role r) {
    return (r == ZMK_SPLIT_ROLE_CENTRAL) ? "central" : "peripheral";
}

enum zmk_split_role zmk_split_role_get(void) { return role; }

int zmk_split_role_set(enum zmk_split_role new_role) {
    if (new_role == role) {
        return 0;
    }

    uint8_t stored = (uint8_t)new_role;
    int err = settings_save_one("split/role", &stored, sizeof(stored));
    if (err) {
        LOG_ERR("Failed to store split role %s (%d)", role_name(new_role), err);
        return err;
    }

    LOG_INF("Storing split role %s and rebooting", role_name(new_role));

    sys_reboot(SYS_REBOOT_WARM);

    return 0; /* unreachable: sys_reboot() does not return */
}

static bool role_loaded_from_settings;

static int split_role_handle_set(const char *name, size_t len, settings_read_cb read_cb,
                                 void *cb_arg) {
    if (!settings_name_steq(name, "role", NULL)) {
        return 0;
    }

    uint8_t stored;

    if (len != sizeof(stored)) {
        return -EINVAL;
    }

    int ret = read_cb(cb_arg, &stored, sizeof(stored));
    if (ret < 0) {
        return ret;
    }

    if (stored != ZMK_SPLIT_ROLE_CENTRAL && stored != ZMK_SPLIT_ROLE_PERIPHERAL) {
        LOG_WRN("Ignoring invalid stored split role %d", stored);
        return -EINVAL;
    }

    if (role_forced_by_switch) {
        /* The GPIO switch already decided the role for this boot. */
        return 0;
    }

    role = (enum zmk_split_role)stored;
    role_loaded_from_settings = true;

    return 0;
}

static int split_role_handle_commit(void) {
    if (!role_forced_by_switch) {
        LOG_INF("Split role: %s (%s)", role_name(role),
                role_loaded_from_settings ? "from settings" : "default");
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(split, "split", NULL, split_role_handle_set,
                               split_role_handle_commit, NULL);

#if DT_HAS_ZMK_SPLIT_ROLE_SWITCH_ENABLED

#define ROLE_SWITCH_NODE DT_INST(0, zmk_split_role_switch)

static const struct gpio_dt_spec role_switch_gpio = GPIO_DT_SPEC_GET(ROLE_SWITCH_NODE, gpios);

static void role_switch_work_handler(struct k_work *work) {
    int val = gpio_pin_get_dt(&role_switch_gpio);
    if (val < 0) {
        LOG_ERR("Failed to read split role switch (%d)", val);
        return;
    }

    enum zmk_split_role switch_role = val ? ZMK_SPLIT_ROLE_CENTRAL : ZMK_SPLIT_ROLE_PERIPHERAL;

    if (switch_role != role) {
        zmk_split_role_set(switch_role);
    }
}

static K_WORK_DELAYABLE_DEFINE(role_switch_work, role_switch_work_handler);

static struct gpio_callback role_switch_cb_data;

static void role_switch_gpio_callback(const struct device *dev, struct gpio_callback *cb,
                                      uint32_t pins) {
    /* Debounce a mechanical slide switch before acting on it. */
    k_work_reschedule(&role_switch_work, K_MSEC(200));
}

#endif /* DT_HAS_ZMK_SPLIT_ROLE_SWITCH_ENABLED */

static int zmk_split_role_init(void) {
    /* Dynamic settings handlers registered before the subsystem is initialized
     * are discarded by settings_subsys_init(). Initialize it now, ahead of the
     * BLE inits at CONFIG_ZMK_BLE_INIT_PRIORITY that register theirs, since
     * on builds without BT_SETTINGS nothing else does so before main(). */
    int err = settings_subsys_init();
    if (err) {
        LOG_ERR("Failed to initialize settings (%d)", err);
    }

#if DT_HAS_ZMK_SPLIT_ROLE_SWITCH_ENABLED
    if (!device_is_ready(role_switch_gpio.port)) {
        LOG_ERR("Split role switch GPIO device not ready");
    } else {
        err = gpio_pin_configure_dt(&role_switch_gpio, GPIO_INPUT);
        if (err) {
            LOG_ERR("Failed to configure split role switch GPIO (%d)", err);
        } else {
            int val = gpio_pin_get_dt(&role_switch_gpio);
            if (val < 0) {
                LOG_ERR("Failed to read split role switch GPIO (%d)", val);
            } else {
                role = val ? ZMK_SPLIT_ROLE_CENTRAL : ZMK_SPLIT_ROLE_PERIPHERAL;
                role_forced_by_switch = true;

                LOG_INF("Split role: %s (from switch)", role_name(role));

                err = gpio_pin_interrupt_configure_dt(&role_switch_gpio, GPIO_INT_EDGE_BOTH);
                if (err) {
                    LOG_ERR("Failed to configure split role switch interrupt (%d)", err);
                } else {
                    gpio_init_callback(&role_switch_cb_data, role_switch_gpio_callback,
                                       BIT(role_switch_gpio.pin));
                    gpio_add_callback(role_switch_gpio.port, &role_switch_cb_data);
                }
            }
        }
    }
#endif /* DT_HAS_ZMK_SPLIT_ROLE_SWITCH_ENABLED */

    return 0;
}

SYS_INIT(zmk_split_role_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
