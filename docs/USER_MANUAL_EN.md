# EVistDrive HMI – Bafang DPC245 V3

## User Manual and Complete Menu Reference

**Documentation version:** Draft 1.0, October 15, 2026

**Reference firmware:** EVistDrive HMI **10.24**, tag `v10.24` of `glodnickim/evistdrive-hmi-dpc245v3`, inspected October 21, 2026

**Interface language:** English. This manual preserves the exact option names displayed on the device.

**Intended audience:** Riders using a Bafang DPC245 V3 display running custom EVistDrive HMI firmware.

> **Important:** This manual was prepared from the firmware implementation and project documentation. It still requires final review against the latest source code by the project maintainer. It is not a substitute for testing with a physical display and compatible motor controller. Different controller versions may implement some settings differently. Use `OFFROAD` settings only where and how permitted by applicable law.

## Contents

1. [Getting started](#1-getting-started)
2. [Buttons and navigation](#2-buttons-and-navigation)
3. [Main dashboard](#3-main-dashboard)
4. [Complete menu structure](#4-complete-menu-structure)
5. [DISPLAY – display settings](#5-display--display-settings)
6. [BIKE – bicycle and assistance settings](#6-bike--bicycle-and-assistance-settings)
7. [INFORMATION – data and diagnostics](#7-information--data-and-diagnostics)
8. [Saving settings](#8-saving-settings)
9. [Messages and warnings](#9-messages-and-warnings)
10. [Troubleshooting](#10-troubleshooting)
11. [Safety and firmware updates](#11-safety-and-firmware-updates)
12. [Glossary](#12-glossary)
13. [Documentation sources and verification](#13-documentation-sources-and-verification)

---

## 1. Getting started

When the bicycle starts, the display shows the EVistDrive HMI dashboard. The main reading is speed. You can also see the assistance level, battery status, current power and two selected riding parameters.

**Quick operation:**

1. Briefly press `UP (+)` to increase the assistance level.
2. Briefly press `DOWN (-)` to decrease the assistance level.
3. Briefly press `POWER` to cycle through the information pairs at the bottom of the screen. You can choose which pairs are available in the settings menu.
4. Hold `UP` and `DOWN` together for about **0.5 seconds** to open the settings menu.
5. To leave settings, choose `EXIT` or leave the menu inactive for approximately **30 seconds**. Uncommitted changes in advanced submenus are discarded. A write operation already in progress may continue.

![STANDARD dashboard](images/standard_dashboard.png)

*STANDARD dashboard in the project's simulator. The actual appearance depends on the selected theme, active fields and data received from the controller.*

## 2. Buttons and navigation

The display has three buttons: `POWER`, `UP (+)` and `DOWN (-)`.

| Action | On the riding dashboard | In menus / while editing |
|---|---|---|
| Short `UP` press | Increase assistance level | Previous row or increase a value |
| Short `DOWN` press | Decrease assistance level; Standard Walk Assist on level 0 | Next row or decrease a value |
| Short `POWER` press | Next enabled lower information pair | Enter, select, begin/end editing, or confirm on specific screens |
| Hold `UP + DOWN` for approx. 0.5 s | Open the `SETTING` menu | Shortcut intended for the riding dashboard |
| Hold `UP` for approx. 4 s | Toggle the bicycle light | Does not adjust LCD brightness |
| Hold `DOWN` | Walk Assist, depending on assistance level and configuration | Repeated value changes when editing, not Walk |

**Typical settings-page workflow:** `UP/DOWN` selects a row → `POWER` starts editing → `UP/DOWN` adjusts the value → `POWER` ends editing → select `SAVE` and press `POWER`. Choosing `BACK` without `SAVE` generally discards edits. Some simple pages (such as `UNIT`) apply changes immediately and have no separate `SAVE` option.

Holding `UP/DOWN` while editing can accelerate repeated value changes: repetition starts after approximately 0.4 seconds and becomes faster after approximately 2 seconds. Some values stop at the ends of their permitted ranges rather than wrapping around.

**About the `POWER` button:** A long press is not a software shutdown command. Switching the device's physical power supply on and off is handled by the display/bicycle electronics.

### 2.1. Walk Assist – pushing the bicycle

**Standard Walk at assistance level `0`:** A brief `DOWN` press starts a **five-second Walk-request window**. Holding `DOWN` again can sustain the request while the button remains held. Actual motor movement also depends on the physical button signal and controller support. A single brief press therefore does not guarantee that the motor will run. A held Walk request is not latched: releasing the held button cancels the hold-based request. The short level-0 authorization window has its own expiry logic.

**Quick Walk at assistance levels `1` and above:** Enable the feature in `BIKE → QUICK WALK`. Hold `DOWN` for the configured `HOLD` time (0.5–5.0 seconds). Walk is then requested only while you keep the button held. Releasing it after activation stops the request without changing the assistance level. If `DOWN` is released before the `HOLD` time elapses, the press retains its normal function and decreases the assistance level.

Do not test Walk Assist with a freely spinning raised wheel unless the test rig is properly secured. Always control the bicycle's direction and potential movement.

## 3. Main dashboard

Four themes are available: `STANDARD`, `YELLOW`, `ARC` and `WEDGE`. Every theme displays the same essential information, with a different layout.

| Theme | Description |
|---|---|
| `STANDARD` | Large speed readout, colored assistance indicator, battery data and vertical power bar |
| `YELLOW` | Standard-style layout with yellow accents |
| `ARC` | Curved power indicator, large speed readout and information tiles |
| `WEDGE` | Wedge-shaped mode indicator, speed and tapered power graph |

| | |
|---|---|
| ![YELLOW](images/yellow_dashboard.png) | ![ARC](images/arc_dashboard.png) |
| ![WEDGE](images/wedge_dashboard.png) | ![Theme selection](images/themes_page.png) |

### 3.1. Speed

Speed is shown in `km/h` or `MPH`, according to `DISPLAY → UNIT`, normally to one decimal place. The value comes from the motor controller over CAN. If a motor or controller temperature warning is active, the speed digits turn red.

### 3.2. Assistance levels and colors

| Display | Name | Indicator color |
|---|---|---|
| `0` | Assistance off | Gray / neutral |
| `E` | ECO | Green |
| `T` | TOUR | Cyan / turquoise |
| `S` | SPORT | Indigo / blue |
| `S+` | SPORT+ | Red |
| `B` | BOOST | Purple |
| `W` / `WALK` | Active push-assistance request | Walk indicator |

These are **HMI level names and colors**, not guaranteed motor power outputs. Actual assistance depends on controller settings and firmware. Configurations providing more than five levels may display additional levels numerically.

An **additional orange outline** indicates `OFFROAD` mode; `LEGAL` has no such outline.

### 3.3. Icons and battery

The upper area can show lighting, Bluetooth and controller-error icons. Battery display elements are configurable: `ICON`, charge percentage (`SOC %`), voltage (`VOLT`) and a vertical charge bar (`BAR`).

The battery bar changes its appearance at low charge: warning colors appear at approximately 15%, it turns red at approximately 9%, and it also flashes at 5% or below. **SOC is a value reported by the battery/controller system; the display does not independently measure the true percentage of available energy.**

### 3.4. Power indicator

The colored graph represents current power calculated from received telemetry. Set its upper scale in `DISPLAY → POWER GRAPH`. **Changing the graph scale does not change the motor power limit** — it only changes how the reading is displayed.

### 3.5. Lower information fields

A short `POWER` press switches an entire **pair of fields**, not an individual metric. Ten pairs are available:

| No. | Menu label | Screen fields | Meaning |
|---:|---|---|---|
| 1 | `TRIP+ODO` | `TRIP` / `ODO` | Current trip distance / total odometer |
| 2 | `MAX+AVG` | `MAX` / `AVG` | Maximum / average speed |
| 3 | `CAD+CAL` | `CADENCE` / `CAL` | Pedal cadence [rpm] / estimated kilocalories |
| 4 | `CAD+TRQ` | `CADENCE` / `TORQUE` | Cadence [rpm] / torque-sensor signal **in mV, not Nm** |
| 5 | `TIME+TRP` | `TIME` / `TRIP` | Riding time [min] / trip distance |
| 6 | `WATT+TMP` | `WATT` / `MOTOR` or `CTRL` | Motor power [W] / selected temperature [°C] |
| 7 | `PDL+MOT` | `PEDAL` / `MOTOR` | Estimated rider power [W] / motor power [W] |
| 8 | `RNG+VOLT` | `RANGE` / `VOLT` | Estimated remaining range / battery voltage [V] |
| 9 | `VOLT+TMP` | `VOLT` / `MOTOR` or `CTRL` | Battery voltage [V] / selected temperature [°C] |
| 10 | `TEMP` | `MOTOR` / `CTRL` (or power) | Temperature view determined by `TEMP VIEW` |

When `TEMP VIEW = BOTH`, the `TEMP` panel shows both temperatures. With other settings, the panel may show one temperature and power. If a data source is unavailable, `--` (or a similar missing-value marker) is shown.

By default, EVistDrive enables the first **nine** pairs. The tenth pair (`TEMP`) is available but initially disabled. Configure the list via `DISPLAY → LOWER FIELDS`.

**About `PEDAL`:** The value is a moving average of about the last ten seconds (the last five controller readings), shown in 5 W steps. It shows `--` when no reading is available, and drops to 0 when pedalling stops. Rider power is estimated by the firmware using sensor data, not measured by a dedicated external power meter. Because the controller reports one instantaneous torque reading about every two seconds, at some steady cadences the value can read too high or too low. Calories are also estimated from calculated work and may differ from fitness-device readings. The same averaged value is sent to connected apps in the periodic status bytes `26..27`.

## 4. Complete menu structure

On the riding dashboard, hold `UP + DOWN` for approximately 0.5 seconds to open the menu.

```text
SETTING
├── DISPLAY
│   ├── UNIT
│   ├── BACKLIGHT
│   ├── BATTERY VIEW
│   ├── LOWER FIELDS
│   ├── TEMP VIEW
│   ├── POWER GRAPH
│   ├── THEMES
│   ├── SERVICE TIPS
│   ├── AUTO OFF
│   └── BACK
├── BIKE
│   ├── ASSIST LEVELS
│   ├── ACCELERATION
│   ├── ASSIST PROFILE
│   ├── WHEEL
│   ├── RIDE MODE
│   ├── QUICK WALK
│   └── BACK
├── INFORMATION
│   ├── HMI
│   ├── BATTERY
│   ├── CONTROLLER
│   ├── SENSOR
│   ├── ERRORS
│   ├── DIAGNOSTICS
│   ├── TRIP RESET
│   └── BACK
└── EXIT
```

![Main settings menu](images/main_menu.png)

*Simulator screenshot. If later firmware changes the menu order or labels, update this figure based on the actual source code.*

Most advanced submenus provide separate `SAVE` and `BACK` entries. The screen also shows a button hint, such as `UP/DN POWER SELECT`, `UP/DN VALUE POWER DONE` or `POWER BACK`.

## 5. DISPLAY – display settings

### 5.1. `UNIT` – measurement units

Choose `METRIC` (kilometers, km/h) or `IMPERIAL` (miles, mph). `UP/DOWN` switches the units and `POWER` returns to the menu. The change is submitted for preference storage without a separate `SAVE` row.

**Default:** `METRIC`.

### 5.2. `BACKLIGHT` – automatic and manual brightness

| Option | Function | Range | Default |
|---|---|---|---|
| `MODE` | `AUTO` uses the ambient light sensor; `MANUAL` uses a fixed setting | `AUTO` / `MANUAL` | `AUTO` |
| `GAIN` | Sensor influence on backlight level in automatic mode | 1–9 | 5 |
| `DWELL` | Consecutive sensor samples required to confirm a state change | 1–30 | 3 |
| `THR` | Light-sensor signal threshold for automatic lighting switching | 1800–5000, step 100 | 3200 |
| `LEVEL` | Manual backlight level setting | 0–15 | 2 |
| `SAVE` | Save the settings | — | — |
| `BACK` | Return without saving | — | — |

The display previews `ALS` (ambient light sensor signal) and `BL` (current backlight driver position, 1–16). In this implementation, editing `BACKLIGHT` affects live behavior; choosing `BACK` restores the last confirmed configuration. Automatic adjustment uses a filtered sensor signal, so changes are not necessarily immediate.

**Do not confuse display brightness with bicycle lighting:** Holding `UP` toggles the bicycle lamp. `BACKLIGHT` settings primarily control the display and automatic-light configuration; automatic lamp behavior also depends on the light sensor and threshold.

### 5.3. `BATTERY VIEW` – battery elements on the dashboard

| Option | Meaning |
|---|---|
| `ICON` | Show the graphical battery icon |
| `SOC %` | Show state of charge as a percentage |
| `VOLT` | Show battery voltage |
| `BAR` | Show the vertical battery bar |
| `SAVE` | Save the selection |
| `BACK` | Discard changes |

Set each item to `ON` or `OFF`. By default, `ICON`, `SOC %` and `VOLT` are enabled, while `BAR` is disabled. Hiding all four can leave the dashboard without battery status information and is not recommended when riding.

### 5.4. `LOWER FIELDS` – selecting information pairs

The menu contains all ten pairs listed in [section 3.5](#35-lower-information-fields), followed by `SAVE` and `BACK`.

Each pair can be independently set to `ON` or `OFF`. **At least one pair must remain enabled.** Attempting to disable the last active pair produces `AT LEAST ONE PANEL`. On the dashboard, a short `POWER` press cycles only through pairs set to `ON`.

Example: To cycle only among `TRIP+ODO`, `WATT+TMP` and `RNG+VOLT`, leave only those three entries set to `ON`, select `SAVE` and return to the dashboard.

### 5.5. `TEMP VIEW` – temperatures and alerts

| Option | Available settings | Description |
|---|---|---|
| `VIEW` | `BOTH`, `ALTERNATE`, `TEMP 1`, `TEMP 2` | Select temperature presentation in lower fields |
| `MOTOR LIMIT` | `OFF` or 40–150°C, step 1°C | Motor temperature warning threshold |
| `CTRL LIMIT` | `OFF` or 40–150°C, step 1°C | Controller temperature warning threshold |
| `SAVE` | — | Save |
| `BACK` | — | Cancel |

`BOTH` allows the `TEMP` panel to show motor and controller temperatures together. `ALTERNATE` changes the selected source about every 3 seconds in single-temperature fields. `TEMP 1` refers to the controller and `TEMP 2` to the motor according to the implemented UI selector; recheck this mapping whenever updating the manual.

**Defaults:** `BOTH` and both warning limits `OFF`. The thresholds activate a visual alert; **they do not control motor current or power reduction**. Use values appropriate to the available sensors and the drive manufacturer's specifications.

If an enabled threshold is exceeded for approximately **2 seconds**, `MOTOR HOT`, `CTRL HOT` or `MOTOR+CTRL HOT` appears and the speed digits turn red. The text disappears after approximately 3 seconds; the red speed remains until the alert clears. An alert for a particular source clears after the temperature stays at least 5°C below the threshold for 2 seconds, or after the reading has been invalid for 2 seconds. A CAN temperature sample older than 3 seconds is treated as invalid.

![Temperature settings](images/temp_view_limits.png)

### 5.6. `POWER GRAPH` – power graph scale

The `SCALE` option sets the graph's upper limit between **250 and 3000 W**, in **25 W** steps. The default is **600 W**. Choose `SAVE` after changing it.

For example, with `SCALE = 500 W`, a 250 W reading fills about half the graph; with `SCALE = 1000 W`, it fills about one quarter. **This is a display-only setting.**

### 5.7. `THEMES` – dashboard themes

The list contains `STANDARD`, `YELLOW`, `ARC`, `WEDGE`, `SAVE` and `BACK`.

1. Select a theme with `POWER`. It becomes the pending choice (`SELECTED`).
2. Navigate to `SAVE`, press `POWER` and check the write result.
3. Only a confirmed theme is shown as `ACTIVE` in saved settings.
4. `BACK` without saving restores the previously confirmed theme. If saving fails, it can be retried.

The 30-second menu inactivity timeout also discards a pending theme choice unless a write is already in progress.

### 5.8. `SERVICE TIPS` – maintenance reminders

`ON` allows the `SERVICE` reminder when the trip/usage counter monitored by HMI reaches its configured service threshold; `OFF` hides the reminder. This is a scheduled reminder, not a diagnosis of wear in a particular component.

**Default:** `OFF`. This page applies the change without a separate `SAVE` option.

### 5.9. `AUTO OFF` – inactivity timeout

Select `OFF` or a timeout of **1 to 10 minutes** (default **5 minutes**). The preference is stored by the HMI.

**Hardware limitation:** The presence of this option does not mean that the application can physically disconnect power. On this firmware version, power-off behavior is controlled by the bicycle/display electronics; the actual effect of `AUTO OFF` must be checked with the specific hardware setup. Do not rely on it as a guaranteed power switch.

## 6. BIKE – bicycle and assistance settings

Changes in this section may be written **to the motor controller over CAN**, rather than only to display memory. Record your current working settings before making changes, and use a safe test environment. Do not edit motor settings while riding.

### 6.1. `ASSIST LEVELS` – assistance levels

The submenu reads the controller's assistance table and exposes fields for successive levels:

- `L1 POWER%`, `L2 POWER%`, etc. — a percentage-based current/power limit in the controller's table (the exact interpretation depends on motor firmware).
- `L1 SPEED%`, `L2 SPEED%`, etc. — the second table column in the `BAFANG` profile.
- `L1 ASSIST%`, `L2 ASSIST%`, etc. — the same second table column in the `FAKE TAXI` profile, with different semantics.

The menu displays up to five levels at once. Mapping to the controller's physical table entries accounts for the configured number of levels (3, 5 or 9). Column values can be edited from **0 to 100%**. After editing, select `SAVE` to send the full settings block to the controller and verify the returned data.

In the `BAFANG` profile, the firmware enforces non-decreasing values across consecutive levels: higher levels cannot arbitrarily be configured below lower ones. The `FAKE TAXI` profile does not enforce this ordering. `BACK` discards unsaved edits.

**Note:** `POWER%` does not necessarily mean a direct limit in watts. `ASSIST%` in the custom profile is not a universal torque multiplier either. The actual meaning of both columns is defined by the motor controller's firmware.

![Assistance level editor](images/advanced_assist.png)

### 6.2. `ACCELERATION` – acceleration response

When the menu opens, the HMI attempts to identify the format supported by the controller:

- `PER LEVEL` — a separate value for each assistance level, such as `L1`, `L2`, ..., `L5`.
- `GLOBAL` — one value shared by all levels.

The editable range is **1–8**. Select an entry, change the value, finish editing with `POWER` and choose `SAVE`. The HMI attempts to write and verify the complete settings block. If it cannot read the configuration or the controller does not support writing it, an error may appear; in some builds saving may be disabled.

**Do not guess which direction the 1–8 scale works in** (whether a higher value means gentler or faster acceleration). Confirm this using the documentation and firmware of the specific motor controller. Different controller implementations may use different formats.

### 6.3. `ASSIST PROFILE` – assistance table format

Set `PROFILE` to `BAFANG` or `FAKE TAXI` (default `BAFANG`). This selects **how the HMI interprets and validates the assistance-level table**. It does not install, replace or flash the controller's firmware.

- `BAFANG`: The second column is labeled `SPEED%`, and level values must be non-decreasing.
- `FAKE TAXI`: The second column is labeled `ASSIST%`, and its values can be arranged in any order.

Choose the profile matching the actual motor firmware and select `SAVE`. Do not switch profiles experimentally without understanding the controller's data format.

### 6.4. `WHEEL` – wheel, circumference and speed

| Option | Meaning | Range |
|---|---|---|
| `MAX SPEED` | Speed limit stored in the controller | Locked to the configured LEGAL limit in `LEGAL`; adjustable from the LEGAL limit up to 70 km/h in `OFFROAD`, step 0.5 km/h |
| `CIRC MM` | Wheel circumference | 500–3000 mm, step 10 mm |
| `DIAMETER` | Wheel size category | `KEEP`, `20"`, `24"`, `26"`, `27.5"`, `28"`, `29"` |
| `SAVE` | Save the complete set to the controller | — |
| `BACK` | Cancel | — |

`KEEP` preserves the existing diameter code read from the controller. If you choose a particular size, the firmware checks whether the entered circumference falls within its accepted range. `SAVE` sends speed, diameter and circumference as one set and then attempts to verify the result by reading it back.

**How to set circumference:** Measure the distance traveled during one full wheel revolution under the usual load. Enter this distance in millimeters, then compare the speed display against an independent GPS reference. An incorrect circumference affects speed and distance readings.

**Important:** Available speed settings and their enforcement depend on the controller and applicable law. The fact that a limit can be selected in the menu does not make it legal for use on public roads.

### 6.5. `RIDE MODE` – LEGAL and OFFROAD

| Option | Values | Function |
|---|---|---|
| `DEFAULT` | `LEGAL`, `OFFROAD` | Stored baseline riding mode |
| `AFTER START` | `LAST`, `LEGAL`, `OFFROAD` | Mode selected after restarting |
| `LEGAL SPEED` | 15.0–32.0 km/h, step 0.5 km/h | Configurable speed limit for `LEGAL` |
| `OFFRD SPD` | Configured `LEGAL SPEED` up to 70.0 km/h, step 0.5 km/h | Speed limit for `OFFROAD` |
| `SAVE` / `BACK` | — | Save / cancel |

**Defaults:** `DEFAULT = LEGAL`, `AFTER START = LEGAL`, `LEGAL SPEED` = 25.0 km/h and `OFFRD SPD` = 40.0 km/h. `LAST` restores the previously selected mode recorded in the configuration.

When applying a mode, the HMI attempts to write the corresponding limit to the controller and verify the readback. It may show a success message or `RIDE APPLY FAILED`.

**Quickly toggle LEGAL ↔ OFFROAD from the riding dashboard:**

1. Set assistance level to `0`.
2. Press `UP`, `DOWN`, `UP` in sequence within **1.5 seconds**. During the first two presses, the level temporarily changes `0 → 1 → 0`.
3. When `OFFROAD UNLOCK?` or `BACK TO LEGAL?` appears, briefly press `POWER` within **3 seconds** to confirm.
4. No confirmation or an unrelated button press cancels the operation. After confirmation, check for `MODE APPLIED` and the mode indicator. If it fails, consult diagnostics.

This shortcut changes the mode **for the current session**; it does not automatically modify the stored `AFTER START` preference. An active Walk request blocks the sequence.

> **Safety and legal notice:** `LEGAL` is a mode name, not certification of road legality. In Poland, an electrically assisted bicycle used as an ordinary bicycle must generally stop providing assistance at 25 km/h and meet other statutory conditions. A configurable `LEGAL` speed range of 15–32 km/h does not establish compliance. Elsewhere, check local requirements. Use `OFFROAD` only where permitted.

### 6.6. `QUICK WALK` – quick push assistance

| Option | Meaning | Values |
|---|---|---|
| `ENABLE` | Enables the Walk shortcut on assistance levels 1 and above | `ON` / `OFF` |
| `HOLD` | Time `DOWN` must be held before activation | 0.5–5.0 s, step 0.5 s |
| `SAVE` / `BACK` | Save / cancel | — |

**Defaults:** `ENABLE = OFF`, `HOLD = 2.0 s`.

Once activated, `DOWN` must remain held: releasing it stops the Walk request. Actual movement depends on the controller and physical button signal. See [section 2.1](#21-walk-assist--pushing-the-bicycle).

## 7. INFORMATION – data and diagnostics

Most screens in this section are read-only. Use `UP/DOWN` to change pages where more than one is available; press `POWER` to return to the `INFORMATION` menu.

### 7.1. `HMI` – display information

Shows the model (`DP C245.C 3.0`), EVistDrive application version, Flash size detected by the MCU, application validity marker, settings-data status (`SPI LOADED` or `DEFAULTS`), active storage location (`SLOT`) and whether writing is enabled (`WRITE ON` / `WRITE LOCK`).

### 7.2. `BATTERY` – battery information, five pages

1. **Electrical status:** Battery temperature, voltage, current and remaining capacity reported by the battery.
2. **Capacity and charge:** Full capacity, relative (`REL CHARGE`) and absolute (`ABS CHARGE`) charge percentage, if reported by the controller/battery.
3. **Raw service data blocks:** `CHARGE`, `CHAR` and their data lengths. These are diagnostic values; not all bytes have a documented meaning.
4. **Cells 1–8:** Voltages of the first eight cells/groups, if reported by the source; unavailable readings appear as `--`. This is not a complete cell view for larger packs such as 11S.
5. **Battery identification:** Hardware version, software version, model, serial number and manufacturer.

### 7.3. `CONTROLLER` – controller information, two pages

1. Speed, current, voltage, controller temperature, raw `STAT BIT0` state bit and raw diagnostic value `3205 RAW`, if available.
2. Controller identification fields: hardware, software, model, serial number and manufacturer.

Raw fields are not assigned a user-level interpretation in the project. Do not conclude that a particular raw value is a fault without the protocol documentation.

### 7.4. `SENSOR` – torque and cadence sensors, two pages

1. `TORQUE RAW`, `TORQUE MV`, `CADENCE`, `ALS` (raw ambient-light sensor value) and `BUTTON SIG` (analog button signal).
2. Sensor identification: hardware, software, model, serial number and `CUSTOMER`.

`TORQUE MV` represents electrical sensor voltage in millivolts; **it is not torque in Nm**. `TORQUE RAW` is an unprocessed reading and requires separate interpretation.

### 7.5. `ERRORS` – error codes

Shows an `E-CODE` history (up to ten entries in storage, with up to six most recent entries shown on screen), the latest source codes from `CTRL`, `SENS` and `BATT`, and a `CAN RX` reception counter. Not all codes enter the persistent history: certain minor/selected codes and immediate duplicates are filtered.

An empty error display does not prove that the motor controller is fault-free. Error visibility depends on the CAN frames received and the HMI's own history rules.

### 7.6. `DIAGNOSTICS` – seven pages

This is an advanced read-only view. `UP/DOWN` switches between `P1/7` and `P7/7`; `POWER` returns.

| Page | Content |
|---|---|
| 1 | `SOC RAW`, voltage, battery temperature, raw capacity, acceleration mode and start-angle reading (if available) |
| 2 | Raw torque sensor reading, torque sensor voltage, cadence, power, averaged pedal power (`PEDAL W`), controller and motor temperatures |
| 3 | Quick Walk configuration, LEGAL speed limit, assistance level, RIDE mode, session mode, last speed limit written to the controller and application result |
| 4 | Button-event counter, maximum loop time and firmware stage timings (CAN, BLE, ADC, application, SPI, LCD) |
| 5 | Most recent controller write: command, ACK and result; rejected BLE writes and their reasons |
| 6 – `TEMP WARN` | Warning count since power-up, most recent activation, temperatures and validity, CAN bytes, maximum readings and limits |
| 7 – `TEMP NOW` | Current warning decisions: application and router temperature, threshold, validity, sample freshness, state transitions, pending state and agreement with the last dashboard frame |

In temperature diagnostics, `M`, `C` and `MC` mean motor, controller and both sources. `APP/RT/LIM` show application data, router data and the configured limit. `VAL/FRESH` refer to measurement validity/freshness; `TRANS/PEND` refer to state transitions and pending warnings. Warning counts and maximum temperatures apply only to the current display power cycle.

`--` means a valid reading is not available; it does not necessarily mean zero temperature or zero voltage. Some raw fields require technical interpretation.

### 7.7. `TRIP RESET` – resetting trip data

The confirmation screen offers:

- `POWER CONFIRM` — reset current trip distance, ride time, maximum and average speed, and selected energy/calorie counters.
- `DOWN CANCEL` — leave without resetting.

This action **does not reset the total odometer (`ODO`)**. It is a deliberate user action with no second confirmation prompt.

## 8. Saving settings

EVistDrive uses two different storage mechanisms:

| Storage | Examples | How it works |
|---|---|---|
| **HMI memory** | Themes, displayed information pairs, battery view, lighting, warning configuration, RIDE preferences and other settings | Written to the display's external SPI memory with data verification |
| **Motor controller settings** | `WHEEL`, `ASSIST LEVELS`, `ACCELERATION`, applying a RIDE speed limit | Read and edited via CAN, written back to the controller and checked using readback where supported |

Seeing `SAVE`, `SAVING` or `WRITING` does not always mean the write is complete. Check the final result. Do not remove power during a write operation or firmware update.

`BACK` usually discards uncommitted editor changes, but a previously started write may still be running. Simple settings pages, including `UNIT`, request storage immediately after a change. Some builds can report `WRITE LOCK` or `SAVE DISABLED`, in which case persistent storage is not guaranteed.

**After firmware updates:** The stored settings format includes a firmware version number. If a saved record belongs to a different version, the HMI may restore user preferences to their defaults while retaining trip counters and part of the history. After an update, check the theme, information fields, speed limits and assistance settings.

## 9. Messages and warnings

| Message | Meaning and suggested action |
|---|---|
| `MOTOR HOT` | The motor temperature has reached the configured alert threshold. Reduce load and check the reading. |
| `CTRL HOT` | The controller temperature has exceeded its alert threshold. Reduce load. |
| `MOTOR+CTRL HOT` | Both temperature sources are in the warning state. |
| Red speed digits | Visual indication of an active temperature warning; not an automatic motor power limiter. |
| `OFFROAD UNLOCK?` / `BACK TO LEGAL?` | Awaiting confirmation with a short `POWER` press (3-second window). |
| `MODE APPLIED` | The HMI received a successful result for the mode/limit change. |
| `RIDE APPLY FAILED` | The limit could not be applied or verified. Check `DIAGNOSTICS`. |
| `READING`, `DETECTING`, `WRITING`, `SAVING` | A communication or write operation is in progress. |
| `READ FIRST` | No valid configuration readback is available. Do not edit values blindly. |
| `NO 3203 READBACK` | The controller's wheel-configuration readback could not be confirmed. |
| `LEVELS MUST NOT DECREASE` | Invalid ordering of assistance levels in the BAFANG profile. |
| `CIRC OUT OF RANGE FOR DIAMETER` | The entered circumference is not valid for the selected wheel size category. |
| `LEGAL MODE - MAX LOCKED` | Speed editing in `WHEEL` is locked while in LEGAL mode. |
| `AT LEAST ONE PANEL` | All lower information pairs cannot be disabled at the same time. |
| `SAVE DISABLED` / `WRITE LOCK` | The current build or configuration does not allow persistent writing. |
| `SERVICE` | The configured maintenance counter threshold has been reached and reminders are enabled. |
| `--` | Missing, invalid or unsupported data for a field. |

Temperature warnings are **informational**. In this version, the HMI warning mechanism itself does not issue a command to reduce motor power.

## 10. Troubleshooting

**I cannot open the settings menu.** Return to the riding dashboard and hold `UP` and `DOWN` together for approximately 0.5 seconds. Holding `POWER` is not the menu shortcut.

**Voltage, temperature or range is missing.** Check communication with the motor controller and battery. `--` usually means that reliable data is unavailable. The HMI cannot display a parameter that the controller does not transmit.

**`ASSIST LEVELS` or `ACCELERATION` does not save changes.** First verify that the configuration can be read. Then check the `SAVE` result and `DIAGNOSTICS` page 5. Do not assume success merely because no error message appeared.

**`DOWN` activates Walk instead of reducing assistance.** At level `0`, Standard Walk behavior applies. At levels `1+`, `QUICK WALK` may be enabled and require a configured hold duration.

**The `TEMP` lower panel never appears.** Open `DISPLAY → LOWER FIELDS`, enable `TEMP` and select `SAVE`. The tenth pair is disabled by default.

**I changed `POWER GRAPH`, but the motor behaves the same.** That is expected: this setting changes only the graph scale, not motor power.

**I cannot raise `MAX SPEED` in `WHEEL`.** The value is locked in `LEGAL` mode. Do not attempt to bypass road-use restrictions.

**Some settings returned to their defaults after a firmware update.** This can result from version validation of the records stored in SPI memory. See section 8.

**The display does not switch off after the `AUTO OFF` timeout.** The menu stores a preference, but physical power disconnection depends on the device and controller electronics. Verify behavior separately on your bicycle.

**The screen is black after an update.** Do not perform random repeat updates or erase large memory areas. Follow the project's recovery procedure and keep the last installation file that worked for you.

## 11. Safety and firmware updates

- Change drive-related settings (`ASSIST LEVELS`, `ACCELERATION`, `WHEEL`, `RIDE MODE`) only while stopped. After a change, test the bicycle's behavior in a controlled environment first.
- Do not assume `OFFROAD` mode is lawful on public roads. Check local regulations and the entire motor–controller–bicycle setup.
- Walk Assist requires holding the relevant button as described for the selected mode. It is not a riding cruise-control feature.
- Temperature alert thresholds do not replace the motor controller's thermal protection.
- The EVistDrive installation file is built with `make update` (see `docs/BUILDING.md`). Installation uses BESST and does not overwrite the resident bootloader area. If you may want to return to the manufacturer's software, keep its update file.
- Do not mass-erase Flash or install an image intended for another display hardware revision.
- Technical build and update instructions are available in `docs/BUILDING.md` and `docs/BUILD_AND_RELEASE.md`.

## 12. Glossary

| Abbreviation | Meaning |
|---|---|
| `HMI` | Human–Machine Interface: the display and control panel |
| `SOC` | State of Charge: battery charge percentage reported by the battery/controller |
| `TRIP` | Current trip distance |
| `ODO` | Total odometer reading |
| `MAX` / `AVG` | Maximum / average |
| `CADENCE`, `CAD` | Pedaling cadence, revolutions per minute |
| `CAL` | Estimated energy expenditure in kilocalories |
| `TORQUE RAW` | Raw torque-sensor reading |
| `TORQUE MV`, `TRQ` | Electrical torque-sensor signal in mV on the relevant pages |
| `WATT` / `MOTOR` | Motor power when used in a power field |
| `PEDAL`, `PDL` | Estimated rider power |
| `RANGE`, `RNG` | Estimated remaining range |
| `VOLT` | Voltage |
| `TEMP`, `TMP` | Temperature |
| `CTRL` | Motor controller |
| `ALS` | Ambient Light Sensor |
| `BL` | Backlight driver position |
| `ACK`, `READBACK` | Communication acknowledgment / verification by reading back |
| `CAN`, `BLE` | Motor communication bus / Bluetooth Low Energy connectivity |

## 13. Documentation sources and verification

This manual was developed primarily from the actual UI labels, button handling and validation implemented in:

- `src/dpc245_app.c`, `include/dpc245_app.h` – menu structure, shortcuts and navigation;
- `src/advanced_settings.c`, `include/advanced_settings.h` – editing pages, ranges and saving;
- `src/ui_dashboard.c` – exact labels, lower fields, themes and messages;
- `src/dpc245_keys.c`, `docs/INPUT_AND_POWER.md` – buttons and Walk Assist;
- `src/evist_preferences.c` – default user preferences;
- `src/ride_mode.c`, `include/ride_mode.h` – LEGAL/OFFROAD;
- `src/light_controller.c` – brightness modes and light sensor;
- `src/dpc245_runtime.c` – riding statistics, estimated rider power and trip reset;
- `src/dpc245_settings.c` – local settings and defaults;
- `src/main.c` – integration of memory, communication and configuration;
- `docs/UI.md`, `docs/PERSISTENCE.md`, `docs/FEATURE_COVERAGE.md` – technical project documentation;
- `README.md` – release and version information.

**Version note:** the current version (**10.24**) is given by `include/app_config.h`, `Makefile` and `CHANGELOG.md`; they are the source of truth.

**Before marking this manual as verified:** The project maintainer should compare this manual with the current `HEAD`, resolve inconsistencies and generate/verify simulator screenshots. In particular, the following require physical hardware verification: `AUTO OFF` behavior, the direction of the `ACCELERATION` scale, how `BACKLIGHT → LEVEL` affects actual brightness, details of Standard Walk operation, and successful application of RIDE settings with a specific controller. Do not present these as hardware-verified facts until they have been tested.

---

*EVistDrive is an independent project, not a Bafang product and not sponsored by Bafang. This manual describes the firmware's functions and does not guarantee compatibility with every controller version.*
