/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_model.h"

const evist_menu_item_t evist_main_menu[] = {
    { MENU_DASHBOARD,    "Dashboard",     "Speed, assist, battery, live power" },
    { MENU_RIDE,         "Ride",          "Trip, cadence, range, ride data" },
    { MENU_MOTOR,        "Motor",         "Motor/controller temperature and electrical data" },
    { MENU_BATTERY,      "Battery",       "Voltage, state of charge, battery CAN pages" },
    { MENU_SENSOR,       "Sensor",        "Torque and cadence diagnostics" },
    { MENU_BAFANG_CONFIG,"Bafang Config", "Read configuration; writes gated by Expert mode" },
    { MENU_CONTROLS,     "Controls",      "ALS, physical keys and analog button-signal line" },
    { MENU_CAN_MONITOR,  "CAN Monitor",   "All received IDs, DLC, bytes and decoded address" },
    { MENU_BLE,          "BLE Debug",     "USART1 module monitor and DPC245 interface" },
    { MENU_EVISTDRIVE,   "eVistDrive",    "Motor adapter, extended telemetry and profiles" },
    { MENU_DIAGNOSTICS,  "Diagnostics",   "Errors, component versions and bus statistics" },
    { MENU_ABOUT,        "About",         "Firmware/hardware/build information" },
};
const size_t evist_main_menu_count = sizeof(evist_main_menu) / sizeof(evist_main_menu[0]);
