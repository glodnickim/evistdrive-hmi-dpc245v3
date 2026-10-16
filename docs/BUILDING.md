# Build and install

This guide builds EVistDrive HMI from a fresh checkout; the version is set by `VERSION` in the `Makefile`. Use Ubuntu 22.04 or 24.04, directly or through WSL2 on Windows.

## 1. Prepare Ubuntu

On Windows, open PowerShell and run `wsl --install`, then open the installed Ubuntu shell. On Linux, use your normal terminal. Install the programs invoked by the Makefile and CI:

```sh
sudo apt-get update
sudo apt-get install -y clang lld llvm make python3 git binutils
```

The build uses:

- `clang` for the ARM firmware and the host tests, with `lld` as the linker;
- `llvm-objcopy` (package `llvm`) for the application image;
- `size` (package `binutils`), which `tools/check_firmware.py` uses to check the image sections;
- GNU Make, Python 3, Git for the source identifier, and standard shell utilities.

Confirm that `clang --version`, `llvm-objcopy --version` and `size --version` work.

## 2. Get the source

On Windows, clone inside your WSL home directory (for example `~/evistdrive-hmi-dpc245v3`), not in a Windows drive folder under `/mnt`: builds in the Linux file system are much faster, and files keep their LF line endings.

```sh
cd ~
git clone https://github.com/glodnickim/evistdrive-hmi-dpc245v3.git
cd evistdrive-hmi-dpc245v3
```

If you downloaded an archive, extract it inside the WSL home directory and open its top-level directory instead.

## 3. Run the gate

```sh
make check
```

The expected last line is `EVistDrive HMI: all checks passed`. This runs repository and requirement checks, host tests, screen layout checks, and product, debug, recovery-fault and probe firmware builds. It writes output under `build/`.

`make check` starts with `make clean`, which deletes the whole `build/` directory, including an update image made earlier with `make update`. Copy an update image you want to keep to another place first.

## 4. View the screens

```sh
make sim
```

Open `sim/out/index.html` in a browser to see every rendered screen. The page includes pixel coordinates for layout inspection. From the WSL shell, the Windows browser can open it with:

```sh
explorer.exe "$(wslpath -w sim/out/index.html)"
```

Alternatively, open the file in Windows Explorer through the `\\wsl$` network location, for example `\\wsl$\Ubuntu\home\<your user>\evistdrive-hmi-dpc245v3\sim\out\index.html`.

The README screenshots in `docs/images/` are produced by a separate command, without the grid:

```sh
make screenshots
```

## 5. Create and inspect the installation file

```sh
make update
python3 tools/build_update_image.py --inspect build/EVistDrive_HMI_<version>_Update.bin
sha256sum build/EVistDrive_HMI_<version>_Update.bin
```

`make update` needs no input besides this repository. The output `build/EVistDrive_HMI_<version>_Update.bin` is always 237,600 bytes (`make update-full` builds the 456,572-byte full layout). Inspection must end with `installation file: PASS` (format identifier, header fields, length, CRC, application marker and, in the full layout, blank fill); the command exits with status 1 otherwise. Do not install a file that fails inspection or has another size. Record its SHA-256 before installation, and copy the file out of `build/` before the next `make check` or `make clean`.

The same sources and toolchain give a byte-identical file, so a downloaded release can be checked by building its tagged commit and comparing SHA-256.

## 6. Install

Use the BESST firmware update workflow with the generated file, on a stable power supply, and do not interrupt the transfer. The display's own updater stays on the device and is never written. Recovery after an interrupted or rejected transfer is not specified by this project; keep the last installation file that worked for you. The application also supports update entry by key chord, BLE and CAN. [BUILD_AND_RELEASE.md](BUILD_AND_RELEASE.md) describes the file layout and checks. Check controller settings and display operation on a stand before riding.

## Troubleshooting

- **Missing command:** install the Ubuntu packages above and verify the command is on `PATH`.
- **Shell errors after a Windows checkout:** clone inside WSL and ensure source files have LF line endings; `.gitattributes` requests LF.
- **`UPDATE_TEMPLATE is not supported`:** run `make update` without arguments.
- **`installation file: FAIL`:** the listed check is false. Rebuild with `make update`; never edit an installation file by hand.
- **`repository-check: FAIL`:** the check lists each problem with its file and line: a build artifact or private file in the tree, a local path, a link that leaves the repository, an address outside the application map, trailing whitespace or a missing SPDX header. Fix the named lines, then run `python3 tools/check_repository.py` again.
