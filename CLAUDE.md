# BlueSaab

CD changer emulator for older SAAB cars (9-3/9-5 with I-Bus): the car's head unit
thinks it's talking to a CD changer over CAN, while audio actually streams from a
phone over Bluetooth (Microchip RN52 module). Firmware v6.1.1, hardware v6.1.

Lineage: this repo is the mbed/STM32 rewrite of the Arduino-era "SAAB-CDC"
codebase (github.com/kveilands/SAAB-CDC, forks incl. si1/SAAB-CDC) — same
authors, same RN52 concept, ATmega + MCP2515 hardware. That generation had a
different button map (long-press SEEK = pairing, presets 1/2/4 = volume);
old user docs describing those buttons refer to it, not to 6.1.1. Its commit
history is a useful I-Bus protocol reference.

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

## Documentation map

- `README.md` — project front door; `LICENSE` — GPL-3
- `docs/USAGE_v6.md` — v6 user manual (button map is the successor's interface contract)
- `docs/BUILD_v6.md` — building/flashing v6
- `docs/IBUS_PROTOCOL.md` — the I-Bus CDC protocol spec (canonical upstream source is dead; this is the capture)
- `docs/SUCCESSOR_HARDWARE_OPTIONS.md` — successor design + decisions
- `docs/SUCCESSOR_PARTSLIST.md` — block-by-block parts list, keyed to v6 designators
- `docs/SAAB_9-5_NOTES.md` — 9-5 research, model quirks
- `docs/RELATED_PROJECTS.md` — lineage, competitors, 2006+ landscape
- `HARDWARE/README.md` — board files
- `TODO.md` — roadmap

## Project direction

See `TODO.md`. Short version: v6 (STM32+RN52) stays as-is — no firmware work
unless a concrete need arises. All effort goes to the ESP32 spiritual
successor (A2DP sink + built-in TWAI CAN, `docs/SUCCESSOR_HARDWARE_OPTIONS.md`),
whose primary purpose is production continuity: the RN52 is EOL, so v6 can no
longer be manufactured. Ship a buildable v6-equivalent first; features second. Hard requirement:
the successor keeps the v6.1.1 in-car interface as a baseline (`docs/USAGE_v6.md`);
improvements (e.g. multi-device swapping) must be additive on unused buttons,
never repurpose an existing one. `HARDWARE/` holds board files — see `HARDWARE/README.md`.
