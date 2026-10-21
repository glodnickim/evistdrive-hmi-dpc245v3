# BLE Protocol

## Physical transport

The DPC245 application communicates with the radio module through USART1:

- TX: PA9
- RX: PA10
- 230400 baud
- one byte stream in each direction

The radio owns RF/GATT behavior. The MCU owns the UART packet protocol described here.

## Packet format

Application packets use the following envelope:

```text
55 AA command class operation selector length payload... checksum
```

The checksum is the 8-bit additive checksum used by `dpc245_ble_proto.c`. The parser tolerates repeated synchronization bytes and emits a frame only after the complete packet passes validation.

## Application dispatcher

`dpc245_ble_app.c` is the single owner of application commands. It handles command families:

- `0x10` — HMI settings/status selectors;
- `0xA1` — compatibility data block;
- `0xA2` — compatibility data block;
- `0xA3` — controller information and live data;
- `0xA4` — compatibility data block;
- `0xA5` — HMI information, state selectors and update request;
- `0xA7` — compatibility data block;
- `0xA9` — personalized/profile data.

Fields with a stable product contract are represented by typed application state. Bytes without a stable product label are retained byte-for-byte in fixed-size compatibility blocks; the firmware does not assign speculative meanings to them.

## Periodic status

Every 500 ms the application can publish a `10/11/06/01` packet with a 39-byte status payload. It contains current assist/light state and available ride telemetry/statistics. The scheduler is latest-only and does not create a backlog.

Payload bytes `26..27` carry the averaged rider power (`rider_power_avg`) as a 16-bit little-endian value. Bit 15 is a validity flag; bits 0..14 are power in watts, scale 1 W. The value is the mean of the last five controller samples, about ten seconds at the controller frame period. `0x0000` means no data (the consumer must not record a sample); `0x8000` means a valid 0 W (the rider is not pedalling and the data is fresh). The value is limited to 9999 W. Earlier firmware versions send `0x0000` in these bytes.

## Update entry

The A5 update selector enters the same update coordinator used by CAN service entry. The reply is transmitted first, then the application sends the controller coordination frame, waits for CAN TX drain, invalidates the application-valid marker and resets into the platform update environment.

## Radio boundary

The external radio module is a separate processor. Its internal firmware, GATT database and simultaneous-connection capacity are not part of the GD32 application image. The UART protocol remains functional regardless of which compatible radio firmware is installed.
