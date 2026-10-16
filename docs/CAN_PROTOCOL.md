# CAN Protocol

## Identifier layout

The application uses 29-bit identifiers with fields:

```text
(source << 24) | (target << 19) | (operation << 16) | (command << 8) | sub
```

Primary node IDs:

- 1 torque sensor
- 2 drive controller
- 3 HMI
- 4 battery
- 5 service tool
- 31 broadcast

## Periodic HMI frames

The periodic profile is defined in `dpc245_v3_profile.c` and built by `dpc245_tx.c`. Heartbeat assist codes support 3, 5 and 9 assist-level layouts. Walk assist uses assist code `0x06` only while the physical DOWN-button dead-man state is active.

## Telemetry

`bafang_can.c` decodes controller, torque-sensor and battery telemetry. Unknown extended frames remain available in the router log instead of being discarded.

## Service transfers

`bafang_service.c` handles normal ACK/error responses and multi-frame service replies. The router generates required service ACK frames and applies identity/battery information to dedicated state objects.

## Wheel configuration

Command `0x3203` carries:

```text
speed limit, 0.01 km/h   uint16 LE
wheel diameter code      uint16 LE (0x0140 20", 0x0180 24", 0x01A0 26", 0x01B5 27.5", 0x01C0 28", 0x01D0 29")
circumference, mm        uint16 LE
```

All configuration traffic uses the transaction engine (`controller_write.c`), sent as the display node 3:

1. Opening the WHEEL page sends one READ `0x3203` (220 ms), then the `0xF203` form (280 ms). Only a reply addressed to node 3/5 that arrives after this READ fills the page; the `0x02F83203` broadcast never does.
   Every receive window is half-open: ACK or data counts only while `now < deadline` (wrap-safe), independent of whether the main loop ran RX or the timeout check first.
2. SAVE accepts speed 10.00..70.00 km/h (0.50 steps), circumference 500..3000 mm (10 mm steps) and, when a diameter is chosen, the circumference range of that diameter; otherwise the code read from the controller is kept.
3. One 6-byte WRITE; ACK (DLC 0) within 600 ms; 400 ms settle; READ `0x3203`; the reply received within 600 ms after that READ is compared byte for byte.
4. Outcome on the page: VERIFY OK, NO CONTROLLER ACK, NO READBACK or READBACK MISMATCH. Unsaved edits stay until SAVE or BACK.

## ASSIST profile

Command `0x6011` is a 64-byte profile. EVistDrive modifies only:

- `0x28..0x30`: POWER (current limit) for slots 1..9, range 0..100.
- `0x31..0x39`: SPEED (BAFANG profile) or ASSIST % (FAKE TAXI profile) for slots 1..9, range 0..100.
- `0x3F`: checksum, sum of bytes `0..62` modulo 256.

The page shows at most five levels. Visible level -> slot: 3 levels 2,4,8; 5 levels 1,3,5,7,8; 9 levels 0..4 (`assist_profile.c`). For the BAFANG profile a level cannot be edited past its neighbours, hidden slots are reconciled after each change, and SAVE requires both columns non-decreasing over all nine slots; the transaction refuses any other block. FAKE TAXI keeps free order.

All other bytes are preserved from the read result. Write sequence: declaration `WRITE 60 11 [64]`, then START, six continuation frames and END, all from node 3, 20 ms apart, without waiting between frames; ACK `0x6011` within 600 ms after the last frame (one full resend otherwise); 500 ms settle; up to three READs of the block (450 ms each, 150 ms apart); checksum and byte-exact comparison of the block received after the READ. A reassembled block counts only if its START frame arrived after this READ (a fresh multi-frame state for each READ). Limit: without a request id a late reply to an earlier READ whose START comes after the new READ, or frames already queued in the receive FIFO when the READ leaves, cannot be told apart.

## ACCEL

The ADV page first queries `0xF203`. A long reply selects PER LEVEL:
visible acceleration values are P0 (`0x6010`) bytes `1..9`, range `1..8`.
Without that reply it offers GLOBAL using P2 (`0x6012`) byte `54`, range `1..8`. The meaning of this byte across controller variants is unconfirmed; verify it on a stand before use.
SAVE is gated by `EVIST_ENABLE_CONFIG_WRITES`. It starts from the cached full
64-byte block, changes the selected byte, and uses the source-3 long write
with ACK after each fragment, re-read and exact comparison. P0 byte 63 is a
read flag: the write uses the sum of bytes `0..62`, but comparison expects the
read flag. P2 byte 63 is the checksum, sum of bytes `0..62` modulo 256.
The write path has host coverage. Its behavior on a connected controller
requires a stand test before use.

## Raw controller status channels

Command `0x3202` is retained as one raw byte. This product consumes only byte0 bit0, exposed as `controller_status_bit0`; no physical label is assigned by the firmware. Command `0x3205` retains its first 16 bits as `controller_fast5_raw` for diagnostics.

## Information queries

Controller, torque-sensor and battery identity pages request `0x60/00..05`. Battery information additionally requests charging information `0x64/01`, cell groups `0x64/02..03` and characteristics `0x64/15`. Unlabelled service bytes remain visible in raw form.

## Identity strings

For interoperability with the controller and service tools, the HMI answers identity queries `0x60` (READ, addressed to node 3) with the standard DPC245 V3 identification. Replies use the long-transfer service handshake.

| Sub | Field | Reply |
|---|---|---|
| `00` | Hardware | `DP C245.C 3.0` |
| `01` | Software | `DPC245CF80301.2` |
| `02` | Model | `DP C245.CAN` |
| `03` | Serial number | length-prefixed record at external SPI address `0x000` |
| `04` | Customer number | length-prefixed record at external SPI address `0x100` |
| `05` | Manufacturer | `BAFANG` |
| `07` | Error history | two uppercase hex characters per stored code |
| `08` | Bootloader | eight raw bytes of the resident bootloader metadata |

The BLE information block (`A5`) carries the same hardware, software, model and manufacturer strings, and at start-up the HMI sets the radio module name to `DP C245-30.CAN`. The controller, the radio and service tools recognise the display by these values, so the firmware keeps them unchanged. They are protocol values only: they do not imply origin, affiliation or endorsement. The firmware's own name and version (`EVISTDRIVE` followed by the version number) appear on the HMI INFO page.
