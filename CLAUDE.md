# BlueSaab

CD changer emulator for older SAAB cars (9-3/9-5 with I-Bus): the car's head unit
thinks it's talking to a CD changer over CAN, while audio actually streams from a
phone over Bluetooth (Microchip RN52 module). Hardware v6.1. Firmware: the last
release is v6.1.1 (2019); the tree is v6.1.7, unreleased pending in-car
validation (`FIRMWARE_VERSION` in `SaabCan.h`, history in `CHANGELOG.md`).

Lineage: this repo is the mbed/STM32 rewrite of the Arduino-era "SAAB-CDC"
codebase (github.com/kveilands/SAAB-CDC, forks incl. si1/SAAB-CDC) — same
authors, same RN52 concept, ATmega + MCP2515 hardware. That generation had a
different button map (long-press SEEK = pairing, presets 1/2/4 = volume);
old user docs describing those buttons refer to it, not to 6.1.x. Its commit
history is a useful I-Bus protocol reference.

## Build

- Target: STM32F103RB (Cortex-M3), mbed OS 2 "classic" + mbed-rtos (both vendored
  in-repo; the HAL is prebuilt `libmbed.a`). Arm retired Mbed in July 2026 and
  its URLs are dead — never try to update these libs.
- Toolchain: `arm-none-eabi-gcc` 8, plain `make` (see `Makefile`); variants via
  `make EXTRA_FLAGS="-Werror -D…"`. Output goes to `BUILD/` (gitignored).
- CI (`.github/workflows/build.yml`) is the canonical, byte-reproducible release
  toolchain; host unit tests live in `test/`. Always pass `--repo 4pplet/BlueSaab`
  to `gh` — this checkout also has the upstream repo as a remote.

## Architecture

- `main.cpp` — boots RTOS threads; initializes CAN-side subsystems first, then
  Bluetooth (which blocks ~5 s while the RN52 reboots)
- `SaabCan.*` — I-Bus CAN node @ 47.619 kbps: RX dispatch (ISR), TX thread,
  health counters; CDC frame IDs in `SaabCan.h`
- `CDCStatus.*` — emulated CD changer: CDC mode on/off, 0x3C8 status, and
  `NodeStatusSender` (the single sender of all 0x6A2 node-status replies)
- `Buttons.*` — decodes head unit / steering wheel buttons from frame `0x3C0`.
  Preset 1 = discoverable, 3 = reconnect, 4/5 = volume down/up (AVRCP to the
  phone, 6.1.3+), 6 = disconnect; extra-long middle SEEK (0x88, >2 s) =
  discoverable (6.1.3+). Long SEEK± (0x45/0x46), long middle SEEK (0x84),
  RANDOM, pause on/off and preset 2 are decoded but unhandled.
- `SidResource.*` / `Scroller.*` / `utf_convert.*` / `MessageSender.*` — SID
  text: request/grant handshake (0x357/0x368), 3-frame writes (0x337), banner,
  PAIRING/CONNECTED notices, scrolling metadata. On by default
  (`SID_TEXT_CONTROL_ENABLED 1` in `SidResource.h`); 0 disables the SID calls.
- `common/` — RN52 driver: `Bluetooth` (high-level API + debug console),
  `RN52` (serial command protocol), `SerialLog`/`SerialRX`, and `can_api.c`
  (our override of the mbed bxCAN driver)
- `test/` — host unit tests (Scroller, utf_convert), run by CI
- `tools/` — `rc.env` pins the release candidate under validation;
  `bench.sh` (fetch+verify, backup, flash, logging console, summary) and
  `bench_console.py`; they write to the gitignored `bench/`

No watchdog exists yet (planned for 6.2.0), so any halt path (`error()`,
`MBED_ASSERT`, RTX `os_error`) is permanent in the car — avoid them.

## Debug serial console

2-pin UART2 header, 115200 8N1, 3.3 V, no GND pin. Single-char commands (see
`Bluetooth::handleDebugChar`): `V` discoverable, `I` connectable, `C` reconnect,
`D` disconnect, `P`/`N`/`R` playback, `A` voice assistant, `B` reboot RN52,
`d` RN52 Bluetooth address (`BTA=`), `u` wipe all pairings, `E` CAN health
counters, `H` help. 6.1.2+ logs `RN52 version: x.xx` at boot.

## Documentation map

- `README.md` — project front door; `LICENSE` — GPL-3
- `CHANGELOG.md` — firmware version history (6.1.2–6.1.7 unreleased, ship as 6.1.7)
- `docs/USAGE_v6.md` — v6 user manual (button map is the successor's interface contract)
- `docs/V6_CODE_AUDIT.md` — every audit round's findings + halt-path inventory; don't-port-this list
- `docs/BUILD_v6.md` — toolchain, CI, release process
- `docs/FLASHING_v6_HOWTO.md` — step-by-step flash guide (serial bootloader + SWD)
- `docs/IBUS_PROTOCOL.md` — the I-Bus CDC protocol spec (canonical upstream source is dead; this is the capture)
- `docs/SUCCESSOR_HARDWARE_OPTIONS.md` — successor design + decisions
- `docs/SUCCESSOR_PARTSLIST.md` — block-by-block parts list, keyed to v6 designators
- `docs/SAAB_9-5_NOTES.md` — 9-5 research, model quirks
- `docs/RELATED_PROJECTS.md` — lineage, competitors, 2006+ landscape
- `HARDWARE/README.md` — board files, headers, interim power-switch mod
- `docs/BENCH_SESSION.md` — printable bench + in-car validation checklist
- `TODO.md` — roadmap

## Project direction

See `TODO.md`. v6 **hardware** is frozen: the RN52 went end-of-life in 2024
(Microchip PCN, last shipment June 2024), so v6 can no longer be manufactured.
RN52 module firmware updates are avoided (brick risk, metadata-only benefit).
v6 **STM32 firmware** work continues: 6.1.7 is being validated, 6.2.0
(watchdog + sleep) is planned. The main track is the ESP32 spiritual
successor (A2DP sink + built-in TWAI CAN, `docs/SUCCESSOR_HARDWARE_OPTIONS.md`),
whose primary purpose is production continuity — ship a buildable
v6-equivalent first; features second; v6 work must not delay it. Hard
requirement: the successor keeps the released v6 in-car interface
(`docs/USAGE_v6.md`, incl. the 6.1.3 additions); improvements (e.g.
multi-device swapping) must be additive on unused buttons, never repurpose an
existing one. Hardware claims must be checked against datasheets — an earlier
"drop-in" LDO recommendation turned out to have an incompatible pinout.
