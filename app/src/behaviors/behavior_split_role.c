/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_split_role

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/split_role.h>
#include <zmk/behavior.h>
#include <zmk/split/role.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata split_role_param_values[] = {
    {
        .display_name = "Central",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = SPLIT_ROLE_CENTRAL_CMD,
    },
    {
        .display_name = "Peripheral",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = SPLIT_ROLE_PERIPHERAL_CMD,
    },
    {
        .display_name = "Toggle",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = SPLIT_ROLE_TOGGLE_CMD,
    },
};

static const struct behavior_parameter_metadata_set split_role_param_set = {
    .param1_values = split_role_param_values,
    .param1_values_len = ARRAY_SIZE(split_role_param_values),
};

static const struct behavior_parameter_metadata_set metadata_sets[] = {split_role_param_set};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(metadata_sets),
    .sets = metadata_sets,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int resolve_role(uint32_t param1, enum zmk_split_role *role) {
    switch (param1) {
    case SPLIT_ROLE_CENTRAL_CMD:
        *role = ZMK_SPLIT_ROLE_CENTRAL;
        return 0;
    case SPLIT_ROLE_PERIPHERAL_CMD:
        *role = ZMK_SPLIT_ROLE_PERIPHERAL;
        return 0;
    case SPLIT_ROLE_TOGGLE_CMD:
        *role = (zmk_split_role_get() == ZMK_SPLIT_ROLE_CENTRAL) ? ZMK_SPLIT_ROLE_PERIPHERAL
                                                                 : ZMK_SPLIT_ROLE_CENTRAL;
        return 0;
    default:
        return -ENOTSUP;
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    enum zmk_split_role role;

    int ret = resolve_role(binding->param1, &role);
    if (ret < 0) {
        LOG_ERR("Unknown split role command: %d", binding->param1);
        return ret;
    }

    ret = zmk_split_role_set(role);
    if (ret == -ENOTSUP) {
        LOG_DBG("Split role is fixed on this device");
        return ZMK_BEHAVIOR_OPAQUE;
    } else if (ret < 0) {
        LOG_ERR("Failed to store split role (%d)", ret);
        return ret;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_split_role_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_split_role_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
