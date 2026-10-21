# Changelog

## 10.24

- PEDAL power is now averaged over about 10 seconds and shown in 5 W steps; it shows `--` when no data is available and drops to 0 when pedalling stops.
- The display also sends the averaged pedal power to connected apps (status bytes 26–27).
- DIAGNOSTICS page 2 shows `PEDAL W`, the unrounded averaged value.
- As with every new version, display options return to their defaults at the first start; the odometer, ride times and error history are kept.

## 10.23

- RIDE MODE: the default OFFROAD speed is 40.0 km/h (LEGAL stays 25.0 km/h).
- POWER GRAPH: the default scale is 600 W.
- Build tooling: `make update` builds the short installation file (header and application, 237,600 bytes); `make update-full` builds the full layout.
- As with every new version, display options return to their defaults at the first start, so the new defaults apply right after the update; the odometer, ride times and error history are kept.

## 10.22

- Build tooling: updated installation file layout; `make update` builds it and `tools/build_update_image.py --inspect` checks it (see the build and release guide).
- Display functions are the same as in 10.21.
- As with every new version, display options return to their defaults at the first start; the odometer, ride times and error history are kept.

## 10.21

- The RIDE MODE key combination (UP, DOWN, UP at level 0, then POWER) works again with the physical keys.
- After installing a new version, RIDE MODE settings (OFFROAD speed, default and start mode) also return to their defaults, like the other options; set them again after an update.

## 10.20

- A motor without a temperature sensor no longer shows 215 C or a false overheating warning.
- Unavailable controller temperatures also show `--` instead of a temperature.

## 10.19

- After installing a new version, display options return to their defaults; the odometer, ride times and error history are kept.
- DIAGNOSTICS has a new TEMP WARN page that shows what triggered the last overheating warning.
- The page also shows the highest temperatures seen since power-up and the limits in use beside the TEMP VIEW limits.
- A TEMP NOW page shows the live temperatures, limits and timing that the overheating warning uses.

## 10.18

- The menu no longer returns to the main screen by itself while you are using it.
- The RIDE MODE key combination keeps its full time window during screen updates.

## 10.17

- Menu keys remain responsive while a settings save or screen update is in progress; each key press acts on the
  row shown on the screen.
- Releasing DOWN after an active Quick Walk no longer lowers the assist level.
- Walk stops on every physical DOWN release, even a short one between two screen updates.
- A settings save that gets no answer from the memory ends after 5 s with an error instead of staying busy.
- Menu rows, values and hints use larger text with spacing adjusted for the display.
- TEMP VIEW shows temperature limits without a leading zero (40 C instead of 040 C).

## 10.16 — first public release

- Four dashboard themes, ten selectable lower information panels, temperature views and overheat warnings.
- Assist, Walk, RIDE MODE, lighting, ride statistics and configurable preferences.
- CAN and BLE application transports with update entry and controller setting read-back.
- CRC-protected settings ring in external SPI memory, host tests and a screen simulator.
- Installation images can be made with the user's own DPC245 V3 update container.

New behavior should be checked on a stand before riding. OFFROAD mode and the “Fake Taxi” assist profile are for use off public roads only where local law permits.
