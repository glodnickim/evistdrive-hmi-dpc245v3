/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MENU_MODEL_H
#define MENU_MODEL_H

#include <stddef.h>

typedef enum {
    MENU_DASHBOARD = 0,
    MENU_RIDE,
    MENU_MOTOR,
    MENU_BATTERY,
    MENU_SENSOR,
    MENU_BAFANG_CONFIG,
    MENU_CONTROLS,
    MENU_CAN_MONITOR,
    MENU_BLE,
    MENU_EVISTDRIVE,
    MENU_DIAGNOSTICS,
    MENU_ABOUT
} evist_menu_id_t;

typedef struct {
    evist_menu_id_t id;
    const char *title;
    const char *description;
} evist_menu_item_t;

extern const evist_menu_item_t evist_main_menu[];
extern const size_t evist_main_menu_count;

#endif
