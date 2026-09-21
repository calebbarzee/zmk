/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <errno.h>
#include <stdbool.h>

#include <zephyr/sys/util.h>

/**
 * @brief The role a device plays in a split keyboard.
 *
 * The values are stored in settings, so they must not change.
 */
enum zmk_split_role {
    ZMK_SPLIT_ROLE_CENTRAL = 0,
    ZMK_SPLIT_ROLE_PERIPHERAL = 1,
};

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_SWITCHABLE)

/**
 * @brief Get the role this device is running with.
 *
 * Until settings are loaded this returns the default role selected by
 * CONFIG_ZMK_SPLIT_ROLE_CENTRAL, or the role read from the split role switch GPIO
 * when one is present.
 */
enum zmk_split_role zmk_split_role_get(void);

/**
 * @brief Store a new role and reboot into it.
 *
 * @retval 0 the requested role is already active, nothing was done.
 * @retval -errno the role could not be stored.
 * @return does not return when the role was stored; the device reboots.
 */
int zmk_split_role_set(enum zmk_split_role role);

#else

static inline enum zmk_split_role zmk_split_role_get(void) {
    return (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))
               ? ZMK_SPLIT_ROLE_CENTRAL
               : ZMK_SPLIT_ROLE_PERIPHERAL;
}

static inline int zmk_split_role_set(enum zmk_split_role role) {
    ARG_UNUSED(role);
    return -ENOTSUP;
}

#endif /* IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_SWITCHABLE) */

/**
 * @brief Whether this device currently acts as the split central (or is not split at all).
 *
 * Constant folds to the compile time role when CONFIG_ZMK_SPLIT_ROLE_SWITCHABLE is off.
 */
static inline bool zmk_split_role_is_central(void) {
    return zmk_split_role_get() == ZMK_SPLIT_ROLE_CENTRAL;
}
