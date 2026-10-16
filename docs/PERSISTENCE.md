# HMI settings storage

The application stores HMI preferences in a versioned record in external SPI memory. Controller-owned settings remain on the controller and are read or changed through CAN.

## Ring and record

The 221 pages at `0x1F0100..0x1FDDFF` hold a 192-byte record at the start of each 256-byte page. The remaining bytes stay erased. The ring lies within the allowed settings area `0x1F0000..0x1FFEFF`; the final page at `0x1FFF00` is outside it.

| Offset | Size | Content |
|---:|---:|---|
| 0 | 4 | `EVN1` magic |
| 4 | 2 | Format version 1 |
| 6 | 2 | Record length 192 |
| 8 | 4 | Wrap-aware sequence number |
| 12 | 17 | Preferences and display configuration |
| 29 | 73 | HMI settings record and ride statistics |
| 102 | 7 | RIDE MODE choices and speed values |
| 109 | 2 | Motor and controller temperature warning limits; 0 disables, 40..150 °C enables |
| 111 | 32 | Firmware version string, including a NUL terminator; unused bytes written `FF` |
| 143 | 45 | Reserved, written `FF` |
| 188 | 4 | CRC-32/ISO-HDLC over bytes 0..187 |

The decoder checks magic, version, length, CRC and field ranges. It loads the highest valid sequence and keeps two recent valid records for fault tolerance. A read failure is an error, never a blank-page indication.

On a valid record written by a different firmware version, including records with `FF` in the version field, startup restores user options to firmware defaults and queues one normal ring save. This includes assist preferences and RIDE MODE configuration. In the 73-byte HMI record, only option bytes `05`, `06`, `07`, `09`, `0A`, `0D`, `0E` and `2E` (BLE mode) are reset; the checksum at `48` is recalculated. All other bytes, including accumulated distance, time, energy, service progress, error history and controller-mirrored data, are retained. A failed save ends normally; the next startup tries again from the prior valid record. Starting the same version does not reset options.

When no valid ring record exists at first start, the application reads the settings-log layout at `0x1F0000` when its header is `EVP1`, along with HMI settings and RIDE MODE records in formats 1 and 2. It validates the decoded values and uses defaults where records are absent. If settings-log values are present, it resets their options before the first ring save while retaining counters and history. Truly blank memory follows the defaults path. Once a valid ring record exists, that settings-log area is read only.

Each explicit change takes a full target. Foreground service performs one bounded read, program, verification, erase or status step per loop, allowing CAN, BLE, keys and display service between steps. A destination page is erased and verified before programming; the new record is read back before an older page is retired. Interrupted writes retain a previously valid image where possible.

Page erase uses opcode `0x81` only on aligned starts in `0x1F0000..0x1FFE00` and only after JEDEC ID `85 60 15`. The authorization point is `dpc245_spi_mem_erase_allowed()`. Each erase wait, including retirement of an older page, ends with an erase error if memory remains busy for 5 s. This generous limit bounds the save session while physical chip erase timing remains unmeasured. Physical timing and endurance on each assembled display require measurement.

`tests/test_native_store.c` models programming, erasure, interruptions, CRC damage and I/O faults. `tests/check_persistence_service.py` checks foreground service ordering. These host checks do not establish physical power-loss behavior.
