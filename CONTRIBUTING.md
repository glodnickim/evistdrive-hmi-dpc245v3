# Contributing

Keep behavior in `src/`, hardware access in `platform/dpc245/`, and protocol state outside rendering code. Explain a change, its affected modules, and how you tested it in a pull request. Run `make check` and `make sim` before submitting.

## Engineering rules

- The application starts at `0x08006000`. Never mass erase or write below that address. Preserve the resident update area `0x08000000..0x08005FFF` and keep marker `0x0803FFFE` under explicit application ownership.
- Keep unowned GPIOs high impedance and leave SWD available during hardware development. The external radio firmware and board power circuit are separate components.
- Keep update entry serviceable before LCD/SPI/UI initialization. A BLE request is `11/A5/30/00`; the CAN request is destination 3, type 1, command `0x40`, subcommand `0x00`, DLC 4, bytes `08 42 03`.
- Send display changes through `ui_strip_render()`. An unchanged frame sends no tiles; a local change sends local 16 × 10 tiles. Invalidate only when panel content is unknown. Use real-size 1-bit fonts and add new screens to simulator layout checks.
- Accept configuration writes only with a specified payload, range validation, explicit user action, timeout, ACK where applicable, and read-back verification. Treat uncertain fields as read-only.
- Keep Walk tied to the physical key. Document and test persistence format changes and protocol state machines.
