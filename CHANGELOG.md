# Changelog

Versions 6.1.2–6.1.6 are unreleased pending hardware validation (see the
checklist in TODO.md); they will ship together as one release.

## 6.1.6 (2026-07-22) — unreleased

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

## 6.1.5 (2026-07-22) — unreleased

Fixes from an adversarial review of the 6.1.2–6.1.4 diff:

- Fixed: `scroller.clear()` still ran in the CAN ISR (via CDC mode
  changes), inflating the lock semaphore each time — completing the 6.1.4
  ISR fix
- Fixed: 0x6A2 sequences could violate the ≥10 ms same-ID rule at sequence
  seams; coalesced polls could go unanswered
- Fixed: display grants arriving during request-spacing were delayed;
  temporary-text ISR races closed
- PAIRING display extended to the full 10 s RN52 pairing window

## 6.1.4 (2026-07-22) — unreleased

Robustness release (own audit findings):

- SID text formatting moved off the CAN interrupt onto a thread (locking
  now effective — historic flicker root-cause candidate)
- All 0x6A2 node-status replies consolidated into one sender (no
  interleaved sequences; 9-5 handshake hardening)
- CAN TX error/drop counters; debug command `E`
- Stack monitoring available behind `STACK_MONITOR_ENABLED`

## 6.1.3 (2026-07-22) — unreleased

Quality-of-life features (all additive) + first audit fixes:

- Presets 4/5: RN52 gain down/up (gain boots at max; 4 trims it)
- Extra-long middle SEEK (>2 s): enter pairing mode from the wheel
- SID feedback: "PAIRING" and "CONNECTED" notices
- CDC-entry beep compile-time optional (`CDC_ENTRY_BEEP_ENABLED`)
- Fixed: scroll-seam stutter; deleted dead+broken CAN send overloads;
  NULL checks on queue allocation; lowercase hex decode

## 6.1.2 (2026-07-22) — unreleased

- Version banner on the SID when entering CDC mode (e.g. `6.1.6 R1.16`) —
  every unit self-identifies without a serial console
- RN52 module firmware version queried at boot and logged to serial

## 6.1.1 (2019) — released

Last firmware of the original development era. Reproducible-build binaries
published 2026-07-22:
[github.com/4pplet/BlueSaab/releases/tag/v6.1.1](https://github.com/4pplet/BlueSaab/releases/tag/v6.1.1)
