# Changelog

Versions 6.1.2–6.1.7 are unreleased; they ship together as **6.1.7** once it
passes hardware validation (checklist in TODO.md). 6.1.2–6.1.6 will not be
released separately — they are the development history folded into 6.1.7.

## 6.2.0 (planned) — "the reliability release"

Not started; scope in TODO.md. Planned: hardware watchdog (IWDG — the unit
is always-powered with no reachable reset, so any hang currently persists
until the harness is unplugged), sleep on bus silence (battery protection
for all units; the LDO+TVS hardware mod remains optional on top), and the
9-5 dead-buttons fix if a test car is available. Plus anything the 6.1.7
validation session turns up. (Was called "6.1.7" in earlier notes.)

## 6.1.7 (2026-09-30) — unreleased, release candidate

Deep audit of the whole repo (firmware, docs, CI, external facts); findings
and dispositions in `docs/V6_CODE_AUDIT.md`. Supersedes the frozen 6.1.6
candidate, which was never released or flashed. Re-pinned 2026-09-30 after
an independent review of the 6.1.7 changes themselves found a regression
(see the CAN-init item) — the first 6.1.7 pin was never flashed either; the
current pin is in `tools/rc.env`.

- Fixed (regression from 6.1.6): CAN init gave the controller 2 ms to
  enter/leave init mode, but since 6.1.6 it is bus-synchronized and must
  wait out the frame in flight (up to ~2.8 ms) — a ~1-2 % chance per boot
  of a permanent halt (`error()`, no watchdog). Now 20 ms, and a timeout
  returns failure instead of halting. CAN init also starts in silent mode, so
  it can't disturb the bus during setup.
- Fixed: a CAN frame with ID 0x000 (or any ID-0 extended/remote frame)
  matched the empty callback slots → `MBED_ASSERT` → permanent halt. The node
  now ignores all non-standard and remote frames too.
- Fixed: two command posts from one interrupt (e.g. CDC-on) into a nearly
  full RN52 command queue could overflow it inside the RTOS kernel →
  permanent halt. Queue occupancy is now tracked atomically at post time.
- Fixed: CAN init failures (e.g. bus stuck dominant at boot) no longer halt
  the unit; they are logged and the bitrate setup is retried once. Normal
  mode is only entered once the bitrate is set, so a failed setup leaves the
  node silent rather than on the bus at the wrong bitrate. (CAN then stays
  off until a power cycle — the 6.2.0 watchdog will cover that.)
- Fixed: a TX frame was dropped at once when all 3 mailboxes were busy; it is
  now retried for up to 20 ms (a lost 0x6A2 breaks the 9-5 handshake), and
  same-ID frames keep ≥ 10 ms spacing even when a retry delays one.
- `E`'s RX-overrun count starts after boot (the 3-frame FIFO can overrun
  before the RX handler is attached).
- Fixed: invalid UTF-8 in track metadata (e.g. Latin-1 "Håkan Hellström")
  swallowed the following letters; invalid bytes now consume one byte and
  Latin-1 letters are transliterated. A UTF-8 lead byte cut off at the end of
  a (byte-truncated) title is dropped.
- Fixed: rare SID temporary-text corruption when the version/PAIRING banner
  and "CONNECTED" collided; the SID event flag is now updated atomically.
- More stack headroom for four 256-byte threads (now 320).
- Debug help text: `d` shows the RN52 Bluetooth address, `E` lists all CAN
  health counters, `u` warns that it forgets all phones.
- Protocol logic (button decode + action map, CDC commands, 0x6A2 reply
  selection and tables, 0x3C8 builder, SID text framing, RN52 V/Q parsing)
  extracted into pure modules (`IbusProtocol`, `RN52Parse`) and unit-tested
  byte-exact; behavior unchanged (proven equivalent to the previous code on
  2.46 M inputs). The 0x6A2 tables moved from RAM to flash (−96 B RAM).
- Build: `make EXTRA_FLAGS=...` hook; CI builds four variants with
  `-Werror`, runs host tests with sanitizers, names artifacts
  `BlueSaab-<version>-<sha7>` with SHA-256 sums; tag-triggered draft
  releases.

## 6.1.6 (2026-07-22) — unreleased, folded into 6.1.7

Full-codebase bug hunt by three independent adversarial reviewers; all
findings fixed (details: `docs/V6_CODE_AUDIT.md`). All bugs pre-existing
since ≤6.1.1 unless noted.

- Fixed: CAN peripheral joined the live I-Bus at 100 kbit/s error-active
  during boot, disturbing the car bus every ignition-on
- Fixed: ~5 s deaf-on-bus window at boot (init order); CAN handlers now
  live within milliseconds
- Fixed: TX mailbox reordering could scramble same-ID frame groups under
  bus load (TXFP now FIFO order)
- Fixed: every SID write carried the "event" mark since first activation
  (never-cleared flag) — flicker candidate
- Fixed: 3/4-byte UTF-8 (curly quotes, dashes, emoji) leaked raw bytes to
  the SID; now dropped like other unmapped characters
- Fixed: phone connecting during the boot wait left AVRCP buttons dead
  until a later chance event (initial state query added)
- Fixed: `%` in a track title could crash the log thread via the debug `d`
  command (deferred format-string misuse)
- Fixed: silent RN52-command drops and CAN RX FIFO overruns — now counted;
  debug `E` shows TX failures/drops, RX overruns, plus the live bxCAN
  REC/TEC error counters and ESR flags (bus-off/error-passive) — one
  keypress measures bus health during validation
- Fixed: CDC status byte-0 encoding wrong for 2 of 4 event/remote cases
  (latent); SID text group seam/tear pacing; atomic breakthrough flag;
  breakthrough only on recognized buttons; bounded RN52 response loops;
  bounded CAN init waits; log-thread stack headroom
- Verified: CAN bit timing exact (0 ppm); Scroller logic passes all 60
  dormant unit-test asserts (host-executed)

## 6.1.5 (2026-07-22) — unreleased, folded into 6.1.7

Fixes from an adversarial review of the 6.1.2–6.1.4 diff:

- Fixed: `scroller.clear()` still ran in the CAN ISR (via CDC mode
  changes), inflating the lock semaphore each time — completing the 6.1.4
  ISR fix
- Fixed: 0x6A2 sequences could violate the ≥10 ms same-ID rule at sequence
  seams; coalesced polls could go unanswered
- Fixed: display grants arriving during request-spacing were delayed;
  temporary-text ISR races closed
- PAIRING display extended to the full 10 s RN52 pairing window

## 6.1.4 (2026-07-22) — unreleased, folded into 6.1.7

Robustness release (own audit findings):

- SID text formatting moved off the CAN interrupt onto a thread (locking
  now effective — historic flicker root-cause candidate)
- All 0x6A2 node-status replies consolidated into one sender (no
  interleaved sequences; 9-5 handshake hardening)
- CAN TX error/drop counters; debug command `E`
- Stack monitoring available behind `STACK_MONITOR_ENABLED`

## 6.1.3 (2026-07-22) — unreleased, folded into 6.1.7

Quality-of-life features (all additive) + first audit fixes:

- Presets 4/5: volume down/up — sends the RN52's `AV-`/`AV+`, which
  Microchip documents as AVRCP volume commands to the phone (the effect
  depends on the phone; to be verified in the car)
- Extra-long middle SEEK (>2 s): enter pairing mode from the wheel
- SID feedback: "PAIRING" and "CONNECTED" notices
- CDC-entry beep compile-time optional (`CDC_ENTRY_BEEP_ENABLED`)
- Fixed: scroll-seam stutter; deleted dead+broken CAN send overloads;
  NULL checks on queue allocation; lowercase hex decode

## 6.1.2 (2026-07-22) — unreleased, folded into 6.1.7

- Version banner on the SID when entering CDC mode (e.g. `6.1.7 R1.16`) —
  every unit self-identifies without a serial console
- RN52 module firmware version queried at boot and logged to serial

## 6.1.1 (2019) — released

Last firmware of the original development era. Reproducible-build binaries
published 2026-07-22:
[github.com/4pplet/BlueSaab/releases/tag/v6.1.1](https://github.com/4pplet/BlueSaab/releases/tag/v6.1.1)
