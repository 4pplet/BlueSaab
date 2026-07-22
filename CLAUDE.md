# BlueSaab

CD changer emulator for older SAAB cars (9-3/9-5 with I-Bus): the car's head unit
thinks it's talking to a CD changer over CAN, while audio actually streams from a
phone over Bluetooth (Microchip RN52 module). Firmware v6.1.1, hardware v6.1.

## Build

- Target: STM32F103RB (Cortex-M3), mbed OS 2 "classic" + mbed-rtos (both vendored
  in-repo — mbed is dead upstream since 2024, never try to update these libs online)
- Toolchain: `arm-none-eabi-gcc`, plain `make` (see `Makefile`)
- Output goes to `BUILD/` (gitignored)

## Architecture

- `main.cpp` — boots RTOS threads, initializes subsystems
- `SaabCan.*` — I-Bus CAN node emulation @ 47.619 kbps; frame IDs defined in `SaabCan.h`
- `CDCStatus.*` — emulated CD changer state machine; handles CDC mode on/off
  (on: RN52 goes connectable + reconnects last phone; off: disconnects)
- `Buttons.*` — decodes head unit / steering wheel buttons from frame `0x3C0`.
  Preset 1 = discoverable (pairing), preset 3 = reconnect, preset 6 = disconnect.
  Long SEEK presses are decoded but intentionally unhandled since v6.
- `SidResource.*` / `Scroller.*` / `utf_convert.*` — scrolling text on the SID
  dashboard display; compiled out unless `SID_TEXT_CONTROL_ENABLED`
- `common/` — RN52 driver: `Bluetooth` (high-level API), `RN52` (serial command
  protocol), `SerialLog`/`SerialRX` (debug console), mbed glue (`can_api.c`)

## Debug serial console

Single-char commands (see `Bluetooth::handleDebugChar`): `V` discoverable,
`I` connectable, `C` reconnect, `D` disconnect, `P`/`N`/`R` playback, `B` reboot
RN52, `H` help.

## Project direction

See `TODO.md`. Short version: keep v6 (STM32+RN52) alive as-is; v7 will be an
ESP32 redesign (A2DP sink + built-in TWAI CAN) since the RN52 is EOL.
`HARDWARE/` holds board files — see `HARDWARE/README.md`.
