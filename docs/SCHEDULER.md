# Scheduler and Timing

EVistDrive HMI uses a cooperative main loop driven by a 1 ms SysTick time base. Interrupt code does not draw the UI or mutate protocol state directly.

## Periodic work

| Task | Period / condition | Owner |
|---|---:|---|
| SysTick | 1 ms | platform runtime |
| ADC sample group input | 40 ms | ADC/ALS path |
| ALS/PA1 aggregate window | 32 samples ≈ 1.28 s | ADC filter |
| HMI CAN `0x03106300` | 100 ms | TX scheduler |
| HMI CAN `0x03106304` | 200 ms | TX scheduler |
| HMI CAN `0x03106301` | 200 ms | TX scheduler |
| HMI CAN `0x03106302` | 200 ms | TX scheduler |
| HMI CAN `0x03106303` | 1000 ms | TX scheduler |
| HMI CAN `0x03986207` | listed, not transmitted | TX scheduler |
| Ride counters | 500 ms while moving | runtime |
| BLE periodic envelope | 500 ms contract | BLE service |
| UI draw | 250 ms or key-driven need | renderer |
| Info/service refresh | 500 ms while relevant screen is open | service scheduler |

The CAN periods are nominal. Due frames are checked from a 33 ms job, so a period P is emitted every ceil(P / 33) × 33 ms: 100 ms → 132 ms, 200 ms → 231 ms, 1000 ms → 1023 ms.

## Concurrency rules

- CAN RX is drained before periodic work.
- BLE UART bytes are drained into one parser; application commands are handled only after a complete checksum-valid frame.
- LCD drawing has one owner: `ui_dashboard_draw()`.
- CAN/ADC/BLE code updates state; it never paints directly.
- Configuration writes use explicit state machines and never block waiting for protocol ACKs.
- The update transition is the only deliberate terminal action: after its coordination frame is sent and TX is idle, the APP invalidates its marker and resets into the resident update environment.
