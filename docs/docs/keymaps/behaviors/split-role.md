---
title: Split Role Behavior
sidebar_label: Split Role
---

## Summary

The split role behavior switches which role a part of a switchable split keyboard plays: [central or peripheral](../../features/split-keyboards.md#central-and-peripheral-roles). It requires a firmware built with [`CONFIG_ZMK_SPLIT_ROLE_SWITCHABLE`](../../config/split.md#kconfig), which builds both roles into the same image and stores the active one in settings. Changing the role reboots the device.

This lets you move a keyboard between a direct Bluetooth connection and a [dongle](../../hardware-integration/dongle.mdx) setup without reflashing. See [Switching between dongle and direct modes without reflashing](../../hardware-integration/dongle.mdx#switching-between-dongle-and-direct-modes-without-reflashing) for the full setup.

## Split Role Command Defines

The `&split_role` behavior takes one parameter, defined in [`dt-bindings/zmk/split_role.h`](https://github.com/zmkfirmware/zmk/blob/main/app/include/dt-bindings/zmk/split_role.h):

```dts
#include <dt-bindings/zmk/split_role.h>
```

| Define                  | Description                                      |
| ----------------------- | ------------------------------------------------ |
| `SPLIT_ROLE_CENTRAL`    | Store the central role and reboot                |
| `SPLIT_ROLE_PERIPHERAL` | Store the peripheral role and reboot             |
| `SPLIT_ROLE_TOGGLE`     | Switch to whichever role is not currently active |

## Behavior Binding

- Reference: `&split_role`
- Parameter: one of the defines above

Examples:

```dts
&split_role SPLIT_ROLE_TOGGLE
```

```dts
&split_role SPLIT_ROLE_PERIPHERAL
```

```dts
&split_role SPLIT_ROLE_CENTRAL
```

## Locality

`&split_role` uses [global locality](../../features/split-keyboards.md#global-locality-behaviors): pressing it sends the command to every connected part of the keyboard, not just the part where the key was pressed. This matters in dongle mode, where the key event is processed on the dongle: global locality is what lets the command reach the switchable half instead of stopping at the dongle.

Only a part built with `CONFIG_ZMK_SPLIT_ROLE_SWITCHABLE` acts on the command. Parts built with a fixed role log the command at debug level and ignore it, so the same keymap binding can be shared across all parts of the keyboard.

Storing a new role reboots the part that stored it. A part already running the requested role does nothing.

To switch back from dongle mode to a direct connection, press `&split_role SPLIT_ROLE_CENTRAL` (the dongle forwards it to the switchable half, which reboots as central), then unplug the dongle so the other half advertises to the newly central half instead.
