# Build and release

The supported toolchain is Ubuntu 22.04/24.04 with Clang, LLD, LLVM tools, GNU Make and Python 3. Follow [BUILDING.md](BUILDING.md) for installation and first build.

`make check` validates repository content and requirements, runs host tests and layout checks, builds the optimized product image plus debug, recovery-fault and probe variants, checks the binary boundaries, and builds and inspects the installation file. `make sim` renders all host screens to `sim/out/`.

The linker starts the application at `0x08006000` and keeps the valid marker at `0x0803FFFE`. The resident update area below the application is preserved. Do not mass erase. Firmware and temporary outputs go under `build/`; `make clean` removes them. The current version is set by `VERSION` in the `Makefile`.

## Installation file

```sh
make update
python3 tools/build_update_image.py --inspect build/EVistDrive_HMI_<version>_Update.bin
sha256sum build/EVistDrive_HMI_<version>_Update.bin
```

`tools/build_update_image.py` builds the file from the application image alone; it takes no other input. The result is deterministic: the same commit and toolchain give the same SHA-256.

Full layout (`make update-full`, `build/EVistDrive_HMI_<version>_Update_full.bin`, 456,572 bytes):

| File offset | Size | Content | Origin |
|---:|---:|---|---|
| 0 | 12 | format identifier `08 42 83 00` and fixed fields `40 00 00 00 00 00 00 00` | interface constants required by the display's update path |
| 12 | 4 | payload length, big-endian, always `0x0006F75C` (456,540) | computed |
| 16 | 2 | CRC16/XMODEM of the payload, big-endian | computed |
| 18 | 14 | zero | interface constant |
| 32 | 237,568 | application image for `0x08006000..0x0803FFFF`, ending with the `55 AA` marker | this project (GPL-3.0-or-later) |
| 237,600 | 218,972 | blank fill `0xFF` for `0x08040000..0x0807575B` | computed |

The full layout keeps the payload length fixed for every release, so each installation transfers the same amount of data. The application never reads the blank area. `--inspect` checks every row above and exits with status 1 when a check fails; a file with any non-blank byte after the application fails inspection.

Short layout (default; `make update`, `build/EVistDrive_HMI_<version>_Update.bin`, 237,600 bytes): the same header with payload length `0x0003A000` (237,568) and the CRC of the application alone, followed by the application; there is no fill, so about 52 % of the bytes of the full layout are transferred. The short layout carries data for the application area only. What the flash after the application holds after installation depends on the display's updater and is not specified here; the application never reads that area. The short layout has been installed and run on a display. The full layout has not been installed on a display; it is kept for comparison and is not published.

What is not in the file: the display's resident updater (`0x08000000..0x08005FFF`) stays on the device and is never written. Interface values needed to work with the e-bike system are limited to the header constants above and the CAN identifiers, frames and identification strings described in [CAN_PROTOCOL.md](CAN_PROTOCOL.md#identity-strings); everything else in the file is this project's code and data.

Install through BESST with a stable power supply and verify operation on a stand. Do not interrupt a transfer. This project does not specify how the resident updater behaves after an interrupted or rejected transfer; the paths covered here are a complete installation, the application's own update entry (key chord, BLE, CAN) and installing another complete file over a working one. Keep the last installation file that worked for you.

## Publishing a release

1. Bump `VERSION` in the `Makefile` and `EVISTDRIVE_HMI_VERSION` in `include/app_config.h`, add a `CHANGELOG.md` entry.
2. From a clean checkout of the tagged commit: `make check && make update`, then `--inspect` must print `installation file: PASS`.
3. Attach `EVistDrive_HMI_<version>_Update.bin` and its SHA-256 to the release only after it has been installed and tested on a display; the attached file must be byte-identical (same SHA-256) to the tested one.

## Fonts

`include/ui_font1.h` describes the 1-bit glyph format. Generated data in `include/ui_font1_*_data.h` is produced with `tools/gen_font1.ps1` from the bundled Inter font. The generator needs Windows PowerShell with .NET `System.Drawing` (GDI+) and does not run on Linux; the generated headers are committed, so a normal build never needs it. New UI text uses its actual glyph size, and simulator layout checks detect overlap or clipping. The Inter license is in `tools/fonts/Inter-OFL-LICENSE.txt`.
