# Building & flashing v6 firmware

Status: **build verified 2026-07-22** — clean build from this repo with ARM
GCC 8.5.0 on macOS (Apple Silicon). Newer GCC major versions are untested;
the code is `gnu++98`-flagged mbed OS 2, so prefer the known-good GCC 8.

## Toolchain

- **Known good:** Homebrew `arm-none-eabi-gcc@8` (8.5.0) + `arm-none-eabi-binutils`.
  Both are keg-only, so put them on PATH explicitly:

  ```sh
  export PATH="/opt/homebrew/opt/arm-none-eabi-gcc@8/bin:/opt/homebrew/opt/arm-none-eabi-binutils/bin:$PATH"
  ```

- `make` — the root [Makefile](../Makefile) is a self-contained mbed-exported
  GCC ARM makefile (builds into `BUILD/`, target NUCLEO_F103RB /
  STM32F103RB, Cortex-M3).
- No mbed CLI, no internet: `mbed/` and `mbed-rtos/` libraries are vendored
  in-repo. The `mbed.bld` / `mbed-rtos.lib` files point at mbed.org URLs
  that **no longer exist** (Arm killed Mbed in 2024) — never try to update
  these libraries; they are frozen.

## Build

```sh
make
```

Output: `BUILD/` (gitignored) — `BlueSaab.bin` / `.hex` / `.elf`. Verified
footprint at v6.1.1: **103 KB flash** (of 128 KB), **~10 KB static RAM**
(of 20 KB) — text 100524 + data 2720 + bss 7160.

## Flash

Full step-by-step (hookup, backup, bootloader dance, verification):
**[FLASHING_v6_HOWTO.md](FLASHING_v6_HOWTO.md)**. Summary — two options,
both on the board's headers:

- **SWD/JTAG**: 10-pin 1.27 mm Cortex debug header; an ST-Link (or the
  ST-Link half of a Nucleo-F103RB board). With OpenOCD:

  ```sh
  # back up the unit's current firmware first (128 KB flash):
  openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
          -c "init; reset halt; flash read_bank 0 backup_v6_unit.bin; exit"

  # flash:
  openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
          -c "program BUILD/BlueSaab.elf verify reset exit"
  ```

  After flashing, bench-verify on the UART2 console (boot banner, `d`, `V`)
  before reinstalling in the car.
- **Serial bootloader** (the primary, historically used method): BOOT0
  button + FTDI header (USART1) with `stm32flash`. (The pyOCD launch config
  referenced in `.gitignore` was the original developers' debug setup, not
  the flashing workflow.)

## Debug console

UART2 header (PA2/PA3), **115200 8N1** — boot banner plus single-character
commands (see [USAGE_v6.md](USAGE_v6.md#debug-serial-console)).

## CI and release process

Every push builds in GitHub Actions (`.github/workflows/build.yml`, pinned
GCC 8-2019-q3) and uploads `.bin`/`.hex`/`.elf` artifacts. CLI driving:

```sh
gh workflow run build --ref <branch>   # manual trigger (pushes auto-build)
gh run watch                           # follow progress
gh run download -n BlueSaab-firmware   # fetch built binaries
```

Releasing a firmware version:

1. Bump `FIRMWARE_VERSION` in `SaabCan.h` (single source of truth — feeds
   the boot banner and the SID version display).
2. Push; wait for CI green; **bench-test the CI artifact on real hardware**
   (serial console + phone, per FLASHING_v6_HOWTO.md verification).
3. Tag (`git tag v6.x.y && git push --tags`), then
   `gh release create v6.x.y <files> --title ... --notes ...` attaching the
   **CI-built** binaries with their SHA-256 sums — CI is the canonical
   toolchain for releases (local Homebrew GCC 8.5 produces different, also
   valid, binaries; don't mix them in one release).

## Compile-time options

- `SID_TEXT_CONTROL_ENABLED` in [SidResource.h](../SidResource.h) — set to
  `0` to build without SID text support (e.g. for nav-equipped cars).
