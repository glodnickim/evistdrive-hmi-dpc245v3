# Input, Button Signal and Power Contract

## Digital keys

The local key inputs are active-low GPIOs with pull-ups:

- POWER/ENTER: PA2
- UP/+: PA3
- DOWN/-: PA4

The input model separates the physical safety state from application gestures. The 1 ms clock interrupt samples the GPIO and advances a separate 10-count physical debouncer every millisecond. The gesture state machine samples at 100 Hz and accepts a changed key mask only after it remains unchanged for at least 35 ms (normally five samples including the first). Gesture events are queued with their sample times and handled in the main loop, so screen drawing and settings writes cannot skip a short press. Each event keeps its sample time unless it was sampled after the current foreground pass began; in that case the application uses the pass time so its timers cannot see an event from the future. Repeat events are discarded when the queue approaches capacity. If the queue fills, later navigation actions are discarded until the queued events drain and all keys are released; a DOWN release remains available for Walk safety. Short actions require at least 60 ms between accepted press and release. Held actions use a 4.01 s threshold from the accepted press. UP+DOWN held for 500 ms is an EVistDrive settings shortcut and is intentionally separate from the standard held-action timing. A single 1 ms interruption during a hold produces neither an accepted gesture release nor a safety release.

Application key codes:

| Physical/event | Code |
|---|---:|
| DOWN short | `0x13` |
| UP short | `0x14` |
| POWER short | `0x0A` |
| UP held | `0x11` |
| DOWN held while Walk is authorized | `0x12` |
| UP+DOWN chord | `0x02` |
| POWER+DOWN held combination | `0x08` |
| very-long class | `0x1B` |

POWER held does not request a software shutdown. Power removal is owned by the board/controller power path.

### Value editing repeat

In value edit mode, a held UP or DOWN produces its first repeat 400 ms after
the debounced press, then repeats every 120 ms. At 2 s held, the interval is
40 ms; scales with more than 50 steps apply five value steps per fast repeat.
The accepted release stops repeats, and the edit path suppresses the extra
short action after a repeated edit. Repeat events do not move dashboard or
menu selection and do not replace Walk, lighting, or key-combination gestures.
Repeat steps clamp at limits; a wrap is available only from a new press.

## Walk Assist

Two explicit policies exist:

**Standard Walk** is available at assist level 0. A DOWN short action opens a 5 s CAN authorization window (`walk` wire code `0x06`). A later DOWN press keeps the request on continuously while held, including past the window deadline. The 4.01 s DOWN-held event still refreshes the deadline; after that held action, the request clears on the accepted DOWN release. A release before the held action retains any unexpired short-click window. Actual motor motion remains physically gated by the separate Button Signal line. Walk does not change assist level 0.

**Quick Walk** is an EVistDrive extension for assist levels 1 and above. The configured hold time is 0.5–5.0 s. The CAN Walk request is asserted only after the hold deadline and stays on during a continuous DOWN hold. After Quick Walk has activated, release clears Walk and consumes the trailing DOWN short action, leaving the selected assist level unchanged. Releasing before activation retains the ordinary DOWN short action and lowers the assist level by one.

The main loop also clears an active held Walk request when the physical DOWN debouncer reports release, even while queued gesture events await delivery. The 1 ms debouncer latches the release edge across foreground passes, including when DOWN is pressed again before the next pass. It reports release after approximately 10 consecutive released samples from a held state. Thus a contact interruption of about 10 ms can clear the CAN Walk request without producing a gesture release: the gesture state machine still requires 35 ms of stable input. After such an interruption, a later DOWN press normally needs a new Quick Walk hold. If the original DOWN press is still queued during a screen update, re-pressing can let the Quick Walk hold deadline be reached from that original press time; the Quick Walk CAN request is asserted only while DOWN is debounced pressed. An unexpired standard short-action authorization window remains active after a short release. Controller behavior on re-press is outside firmware scope.

This separation prevents the custom dead-man policy from altering the standard level-0 authorization behavior.

## PA1 Button Signal

PA1 is sampled by ADC and remains high impedance. The software filter uses a small integrator: readings above the low threshold increment toward an asserted state and low readings decay it. The line is not driven by the MCU.

The Button Signal path and the P+/wake path include board-level passive/electrical behavior outside the APP GPIO contract. Firmware therefore never attempts to synthesize a voltage on PA1 and never assigns an arbitrary GPIO as a P+ output.

## P+ / wake / power hold

The application has no software-controlled power-latch GPIO. Startup and physical power removal are owned by the board power path and controller. The APP responsibilities are limited to:

1. initialize safely after reset;
2. preserve PA1 as an input;
3. maintain CAN state while running;
4. enter the resident update environment when explicitly requested;
5. never drive an unassigned pin in an attempt to keep the HMI powered.

Exact connector voltage levels and analog timing are electrical properties of the board and are intentionally outside the MCU firmware API.
