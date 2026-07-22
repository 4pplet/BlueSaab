# BlueSaab revival — TODO

Goal: revive BlueSaab as a properly maintained open-source project, keep v6
hardware supported, and design a v7 based on a modern radio (ESP32) with
built-in CAN.

## Phase 0 — Repo hygiene (make it a real OSS project)

- [ ] Commit the `HARDWARE/` directory (currently untracked)
- [x] Add a root `README.md` (photos still to add)
- [x] Add a top-level `LICENSE` file (GPL-3.0 full text)
- [x] Add `CLAUDE.md` / contributor docs; `docs/BUILD_v6.md` (build steps
      unverified — see next item); `docs/IBUS_PROTOCOL.md` protocol capture
- [ ] Verify the firmware still builds with a current `arm-none-eabi-gcc`; document
      the known-good toolchain version in the README
- [ ] Add CI (GitHub Actions) that builds the firmware on every push
- [ ] Decide branch strategy (`master` = releases, `new` currently identical — rename or delete)

## Phase 1 — Keep v6 alive

### v6.2 firmware — QOL release (ON HOLD, 2026-07-22)

**Deferred unless a concrete need arises** — v6 works well enough, and effort
goes to the successor, whose main purpose is production continuity (the RN52
is EOL and unobtainable; v6 cannot be manufactured anymore). The tiers below
are kept as a menu should a v6 firmware release ever be warranted; all are
additive and gated on build verification.

Tier 1 — restore what users miss (driver support already exists):

- [ ] Volume up/down on presets 4/5 (`AV+`/`AV-` already in RN52 driver)
- [ ] Extra-long middle SEEK → discoverable (wheel-only pairing, pre-v6 muscle memory)
- [ ] Wire IHU pause events 0xB1/0xB0 → AVRCP pause/play (currently ignored)
- [ ] Set RN52 gain to max at boot (`SS,0F`) — fixes common "too quiet" complaint

Tier 2 — polish:

- [ ] Auto-discoverable when paired-device list is empty (first-install UX)
- [ ] SID state feedback: "PAIRING" / "CONNECTED" / "NO PHONE" (SID builds only)
- [ ] Sleep on bus silence: STM32 stop + RN52_PWREN off + CAN transceiver
      sleep, wake on CAN RX edge (~25 mA → ~6 mA; LM1117 Iq is the floor).
      Doubles as the successor's sleep-state-machine prototype
- [ ] Voice assistant (Siri etc.) on long middle SEEK (`vassistant()` exists)

Tier 3 — fixes:

- [ ] 9-5 "buttons dead until source switch" bug — implement the missing
      "IHU not in CDC mode" status variant (see docs/SAAB_9-5_NOTES.md #3)
- [ ] Make the CDC-entry beep (0x430) compile-time optional (9-5 advice)

Out of scope for v6.2: track metadata on SID, config system, shuffle,
multi-device (RN52 can't).

- [x] Document pairing properly — see `docs/USAGE_v6.md` (preset 1 = discoverable;
      long-press SEEK was v5 behavior and does nothing in 6.1.1)
- [x] iPhone pairing issue resolved: preset 1 confirmed working in-car (2026-07-22);
      old docs incorrectly listed long-press SEEK / preset 1 as volume up
- [ ] Consider mapping `SEEK_MIDDLE_EXTRA_LONG` to `bluetooth.discoverable()` so
      steering-wheel-only cars can pair without the head unit preset buttons
- [ ] Recover v6 CAD sources if they exist (only v5 Eagle files are in the repo)
- [ ] Note: mbed OS 2 and mbed-rtos are dead (Arm shut Mbed down in 2024). Libraries
      are vendored so builds still work, but no fixes will ever come from upstream.

## Phase 2 — Successor hardware (ESP32)

Spiritual successor to BlueSaab v6 — new hardware and firmware, but a
**familiar in-car interface**: the v6 button map (`docs/USAGE_v6.md`) works
unchanged; improvements only on currently-unused buttons. Hardware analysis:
`docs/SUCCESSOR_HARDWARE_OPTIONS.md`.

**Decided 2026-07-22: single ESP32** (Option A) — one chip for BT audio, CAN,
and app logic. **Target cars: pre-2006** (9-3 gen1 1998–2002/03, 9-5 gen1
1998–2005); 2006+ facelift and nav-equipped cars out of scope for now.

- [ ] Pick a project name
- [ ] Design multi-device swap UX (several paired phones, switch from the
      driver's seat — e.g. cycle on preset 3 with device name on SID)
- [ ] Order prototype parts: ESP32-A1S audio dev board (or WROOM devkit +
      PCM5102A board) + TJA1051/SN65HVD230 transceiver breakout
- [ ] Prototype A2DP sink + AVRCP on ESP32 with ESP-IDF (original ESP32 required —
      S3/C3/C6 have no Bluetooth Classic)
- [ ] Prototype I-Bus on ESP32 TWAI @ 47.619 kbps with an SN65HVD230/TJA1051 transceiver
- [ ] Port the protocol layer (SaabCan / CDCStatus / Buttons / SidResource) — the
      I-Bus frame logic is platform-independent and can largely be reused
- [ ] Decide audio output path: ESP32 internal DAC is poor — external I2S DAC
      (e.g. PCM5102) for line-level output
- [ ] Schematic + PCB in an open tool (KiCad) so the design files are truly OSS
- [ ] Power supply: 12 V automotive input (load-dump tolerant) → 3.3 V via
      low-quiescent buck (AP63203-class)
- [ ] Sleep architecture: ESP32 deep sleep on I-Bus silence, wake on CAN RXD
      edge via EXT0/EXT1 GPIO; target < 100 µA total draw when parked
      (CDC 12 V is battery-fed, not ignition-switched — v6 idles at ~15-30 mA
      forever, the successor must not)

## Phase 3 — Community

- [ ] CONTRIBUTING.md, issue templates
- [ ] Publish assembly/flashing guide
- [ ] Changelog + tagged releases
