# User Interface

## Dashboard

The dashboard presents speed, assist state, battery information and a power graph. The lower area contains two information cells. POWER cycles only through enabled lower panels.

ARC and WEDGE show the same active pair in their right-hand tiles. Short POWER changes that pair in every dashboard theme. ARC has a curved left power gauge and two filled tiles beside the pointed speed frame. WEDGE has a tapered bottom power bar, a filled right panel, and a coloured mode badge.

The HMI can show a temperature warning on STANDARD, YELLOW, ARC and WEDGE. A valid motor or controller temperature at or above its enabled limit for 2 seconds turns the speed digits, decimal point and tenth red (`0xF800`). The message `MOTOR HOT`, `CTRL HOT`, or `MOTOR+CTRL HOT` includes the current temperature and appears for 3 seconds after activation or after another source joins. It then disappears while the red speed remains. A RIDE message has priority. The warning uses the lower message band for those 3 seconds; the usual lower pair (STANDARD/YELLOW), centre battery band (ARC), or bottom power band (WEDGE) returns when the message ends. No controller command, assist change or error-list entry is made. A source clears after 2 seconds at or below its limit minus 5 C, or after 2 seconds without a valid temperature. A `0x3201` sample older than 3 seconds is invalid.

At assist level 0, tap UP, DOWN, UP within 1.5 seconds, then tap POWER while the RIDE MODE question is shown to switch between LEGAL and OFFROAD for this session. The first UP and DOWN change assist to 1 and back to 0. The question expires after 3 seconds; another key cancels it and keeps its usual action.

Temperature bytes 6 (controller) and 7 (motor) in `0x3201` use a -40 C offset. Values `0x00` and `0xFF` mean the temperature is unavailable. The dashboard, TEMP views, controller information and DIAGNOSTICS show `--` for that source, and it cannot trigger or update a temperature warning. Diagnostic raw bytes remain visible. An active warning for that source clears after 2 seconds without a valid temperature.

Available lower panels:

1. TRIP + ODO
2. MAX + AVG
3. CADENCE + CAL
4. CADENCE + TORQUE
5. TIME + TRIP
6. WATT + TEMP
7. PEDAL + MOTOR
8. RANGE + VOLT
9. VOLT + TEMP
10. TEMP (controller and motor temperature)

## Main settings

The settings chord opens the root menu. DISPLAY → THEMES lists STANDARD, YELLOW, ARC, WEDGE, SAVE and BACK. UP/DOWN moves the cursor; POWER on a theme marks a pending choice. ACTIVE identifies the theme confirmed by memory and SELECTED identifies a different pending choice. POWER on SAVE applies the choice and reports the memory result on the same page; after an error, SAVE retries it. In a build without memory writes, ACTIVE follows the applied choice. BACK discards a pending choice; after a failed save, BACK or the 30-second idle exit also restores the confirmed theme in runtime without another write. A write still in progress continues after exit. The dashboard returns through the root EXIT row or after 30 seconds without input.

DISPLAY SETTING controls unit, service tips, auto-off, trip reset, coarse brightness and ambient-light sensitivity.

## Advanced settings

- WHEEL: speed limit, wheel size, circumference, APPLY/read-back.
- ASSIST: selected level, SPEED %, POWER %, APPLY/read-back.
- ACCEL: opening the page queries the controller (`0xF203`). PER LEVEL edits one acceleration value 1..8 for each visible assist level; GLOBAL edits one value 1..8 for all levels. SAVE writes the full block and verifies it by read-back; it is available only in builds with configuration writes enabled (the default).
- POWER GRAPH: 250..3000 W, 25 W step, default 600 W.
- FIRMWARE: controller profile selector.
- BATTERY: icon/SOC/voltage visibility.
- LIGHT: AUTO/MANUAL, gain, dwell, threshold, level.
- PANELS: independent enable for all ten lower panels; at least one must remain enabled.
- TEMP VIEW (DISPLAY): `VIEW`, `MOTOR LIMIT`, `CTRL LIMIT`, `SAVE`, `BACK`. VIEW selects both, alternating, TEMP 1 or TEMP 2. Each limit is `OFF` or 40..150 C in 1 C steps; `OFF` is the default. UP from OFF selects 40 C; DOWN from 40 C selects OFF. Held UP/DOWN stops at either end. SAVE stores both limits with the view choice; BACK discards unsaved edits. The ordinary ADV save status appears below the rows.
- QUICK WALK: enable and 0.5..5.0 s hold time.
- DIAGNOSTICS: seven read-only pages. TEMP WARN shows the last overheating warning activation, its sampled temperatures and CAN bytes, the highest valid fresh temperatures since power-up, and separate limits used by the warning and shown in the TEMP VIEW menu. The record and counter reset at power-up. `ON S SRC L` gives activation uptime in seconds, source, and the two limits used then. TEMP NOW shows the latest warning decision for motor and controller: app and router (`RT`) temperatures, the active limit, effective validity (`VAL`), freshness, sample age, transition condition (`TRANS`), pending state (`PEND`), and pending duration. `APP/SCR AGE` shows the current app warning source beside the source used by the last completed dashboard frame and its age in seconds. Source letters are `M` for motor, `C` for controller, `MC` for both, and `-` for none; `--` means no record. Both TEMP page footers show this legend plus `U/D` for page selection and `PWR BK` for POWER to return. `OK` and `NO` report validity and freshness; `ON` and `OFF` report transition and pending state. Temperatures and limits are always motor/controller pairs, and raw bytes are 6 then 7. Ages above 9999 ms or 99999 s display with `>` rather than wrapping off the screen.

## Input safety

Walk assist is never latched. Releasing DOWN immediately removes the walk request regardless of the configured hold time.

## Information pages

Battery information uses five pages: live electrical state, capacity/charge state, raw service blocks, cells 1-8 and battery identity. Controller and sensor identity pages use the complete identity field range.
