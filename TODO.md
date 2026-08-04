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
- [x] Verify the firmware builds — clean build 2026-07-22 with Homebrew
      ARM GCC 8.5.0 (103 KB flash / 10 KB RAM); toolchain documented in
      `docs/BUILD_v6.md`. GCC 12+ untested — stick with 8 for releases
- [x] Add CI (GitHub Actions) builds every push (.github/workflows/build.yml, pinned GCC 8)
- [ ] Decide branch strategy (`master` = releases, `new` currently identical — rename or delete)

## Phase 1 — Keep v6 alive

### v6.x firmware — QOL improvements (RESUMED 2026-07-22)

Firmware improvement is active again: v6.1.2 (version display) implemented,
CI builds every push. Pick items from the tiers below as wanted — all
additive, bench-test before each release (process in `docs/BUILD_v6.md`).
The successor remains the priority; v6 work must not delay it.

Tier 0 — support/diagnostics (the one candidate worth bending the freeze for):

- [x] Show BlueSaab fw + RN52 fw versions on the SID when entering CDC mode
      (`6.1.2 R1.16`, ~4 s) — implemented in v6.1.2 (2026-07-22); bench
      test pending before release

Tier 1 — restore what users miss:

- [x] Volume down/up on presets 4/5 — **v6.1.3** (trims RN52 gain, which
      boots at max)
- [x] Extra-long middle SEEK → discoverable — **v6.1.3**
- [ ] Wire IHU pause events 0xB1/0xB0 → AVRCP — DEFERRED: RN52 only has a
      play/pause *toggle* (`AP`), so this risks state desync; revisit after
      bench observation of when the IHU actually sends these events
- [x] ~~Set RN52 gain to max at boot~~ — audit found v6 already does this

Tier 2 — polish:

- [ ] Auto-discoverable when paired-device list is empty — needs a design
      decision (RN52 can't report PDL state; would be a heuristic, and makes
      the unit pairable to anyone in range)
- [x] SID state feedback "PAIRING" / "CONNECTED" — **v6.1.3**
- [ ] Sleep milestone (bundle, optional enthusiast upgrade): firmware sleep
      on bus silence (STM32 stop + RN52_PWREN off + transceiver sleep, wake
      on CAN RX edge) **plus** LDO swap — sleep alone gets ~25→6 mA (LM1117
      Iq floor), sleep + low-Iq LDO gets ~0.2 mA. LDO: same-pinout SOT-223
      swap; pick MCP1792 (45 V tolerant, ~70 µA Iq, 200 mA) over MCP1703A
      (2 µA but only 16 V max — unsafe without TVS); add SMBJ TVS on 12 V
      input while soldering. Prerequisite: measure real current profile
      (idle/streaming/pairing peak) on the bench to confirm 200 mA headroom.
      Doubles as the successor's sleep-state-machine prototype
- ~~Voice assistant on long middle SEEK~~ — decided against (2026-07-22)

### v6.1.7 candidates — "the reliability release" (post-6.1.6-validation)

- [ ] **Hardware watchdog (IWDG)** — currently absent. The unit is
      always-powered, buried behind trunk trim, with no reachable reset:
      any firmware hang today persists until the harness is unplugged,
      with Bluetooth dead and ~25 mA draining the battery. IWDG + a
      thread-liveness kick turns every unknown future hang into a blip.
      ~20 lines; the single highest-value robustness addition left.
- [ ] **Sleep on bus silence (firmware half of the sleep milestone)** —
      promoted from "optional bundle": even at the LM1117's ~6 mA floor
      it protects every unmodded unit's battery; the LDO+TVS hardware mod
      remains the optional enthusiast upgrade on top.

**Design constraint — watchdog and sleep are COUPLED, implement together:**
the F103 IWDG cannot be paused once started (runs in stop mode, max ~26 s
timeout), so a standalone watchdog would reset the chip mid-sleep unless
the sleep cycle wakes ~every 20 s to kick it. Co-design: RTC/periodic-wake
kick integrated into the sleep loop, budgeting the wake bursts. Both are
bench-iterative (stop mode drops PLL + RTOS tick; wake-on-CAN EXTI shares
the CAN RX pin) — do NOT desk-develop; build on a validated 6.1.6 baseline
with a current meter attached.

- [ ] 9-5 dead-buttons fix — joins this release if a 9-5 is available for
      testing after the 6.1.6 validation session.

**Sleep/wake design sketch:** sleep = STM32 stop mode + RN52 PWREN off +
transceiver in RS-standby (NOT EN-sleep: standby keeps the receiver
mirroring the bus onto RXD; EN-sleep forces RXD recessive = unwakeable).
Wake = bus goes dominant → RXD (idles high) falls → EXTI on PB8 wakes the
core (the CAN peripheral is unclocked in stop; its AWUM can't help).
Resume: restore PLL/72 MHz → RTOS tick → RS low → CAN reinit 47619 →
PWREN on (BT functional ~2 s — fine, driver hasn't reached the CD button).
First wake frame is lost by design; the IHU re-polls. Watchdog cadence:
RTC alarm (LSI) wakes every ~20 s for a ~ms IWDG kick, ~0.02 % duty.
Bench-verify: HVD234 standby RXD wake behavior per datasheet; stop-exit
clock + mbed RTX tick restore.

**Sleep risk register (beyond "doesn't wake"):** (1) wakes-wrong — botched
clock/RTOS/CAN restore skews protocol timing → the warning-lamp failure
class; treat wake as full reboot, check `E` after wake cycles. (2)
doesn't-sleep — symptomless 25 mA drain; only a current meter proves sleep.
(3) wake-storm — if the locked car's I-Bus is NOT truly silent (alarm/TWICE
chirps?), the unit oscillates awake and may drain worse than no sleep;
**prerequisite: overnight parked-bus activity measurement** (add an RX
frame counter to `E` in a 6.1.7 dev build, read it after 8 h locked). (4)
watchdog-kick slip → nightly reset cycles; visible via the reset counter.
(5) wrong-moment sleep — threshold tuning. (6) sleeping unit looks dead on
the bench — document "wake first / BOOT0 still works".

6.1.7 validation plan (watchdog/sleep are "anti-features" — must NEVER act
normally, ALWAYS act when needed — so observability gets built in):

- [ ] Reset-cause telemetry in 6.1.7 itself: boot banner prints the reset
      reason (RCC_CSR flags: IWDG vs POR vs pin) + a reset counter kept in
      RTC backup registers (survive resets) — makes spurious watchdog
      resets visible evidence instead of invisible blips
- [ ] Hang-injection debug command (debug builds only): wedge a thread on
      purpose → verify reset within timeout → verify full recovery
      (BT reconnect + IHU handshake)
- [ ] Sleep: bench PSU with current readout; scripted silence→sleep→
      frame→wake cycles (50×) via the CAN test rig; false-sleep check
      (active bus must always hold it awake); measure IWDG-kick wake
      bursts on the meter
- [ ] In-car soak: multi-day parked test — battery voltage before/after,
      unlock + CD mode must just work; a week of driving with zero
      unexplained resets on the counter

**Validation method (decided 2026-07-22): v6 firmware is validated in the
car**, per the two-phase checklist above — bench serial checks first, then
the in-car pass; the `E` command's error counters/ESR provide the
quantified bus-health verdict. For the flicker verdict specifically: soak
30+ min with scrolling metadata. (An ESP32-based CAN logger/IHU-simulator
rig remains an idea for *successor development* — v7 firmware tested
against the v6 board as reference — but is not part of v6 validation.)

Tier 3 — fixes:

- [ ] 9-5 "buttons dead until source switch" bug — implement the missing
      "IHU not in CDC mode" status variant (see docs/SAAB_9-5_NOTES.md #3);
      needs a 9-5 for testing
- [x] CDC-entry beep compile-time optional (`CDC_ENTRY_BEEP_ENABLED`) — **v6.1.3**

Audit fixes shipped in **v6.1.3**: scroll-seam separator (A2), dead
sendCanMessage overloads removed (A3), Mail::alloc NULL checks (A4),
lowercase-hex decode (A6).

Shipped in **v6.1.4**: SID text moved out of the CAN ISR onto the
SidResource thread — the locking now actually works (A1, likely the
long-reported flicker); all 0x6A2 node-status replies consolidated into one
sender thread so sequences can't interleave (B1, 9-5 handshake hardening);
CAN TX error/drop counters on debug `E` (A5); truthful RN52 init log (B3).

### Validation checklist — v6.1.2→6.1.6 (nothing has touched hardware yet)

Bench phase (USB-serial on UART2 @115200 for console; FTDI header powers
the board for flashing — see docs/FLASHING_v6_HOWTO.md):

- [ ] BEFORE flashing: record the unit's current state — boot banner
      (firmware version) and `d` output → fill in docs/V6_CODE_AUDIT.md
      "deployed unit findings" (answers the static-SID-text question)
- [ ] Back up current flash: `stm32flash -r backup_v6_unit.bin <port>`
- [ ] Flash the CI-built v6.1.6 artifact; boot banner shows 6.1.6
- [ ] Console shows "RN52 version: X.XX" after ~6 s (validates the V-command
      parse — new in 6.1.2; `R?` on SID would mean parse failed)
- [ ] Debug commands still work: `V` (phone sees "BlueSaab"), pair, stream
      music; `P`/`N`/`R` control playback; `d`, `u`, `H`
- [ ] `E` counter exists (note: off-car it WILL count TX errors — no CAN bus
      is connected; that's expected, not a failure)
- [ ] Optional: flash a `STACK_MONITOR_ENABLED 1` build first and check no
      thread's max stack usage approaches its size (SidResource and the new
      NodeStatusSender changed in 6.1.4); then flash the release build
- [ ] Measure current draw from 12 V: idle, streaming, pairing peak —
      informs the sleep-milestone LDO choice and the successor's buck sizing

In-car phase (any 9-3/9-5; extra valuable on a 9-5 — the 0x6A2 path is new):

- [ ] **No warning lights** (airbag/MIL) after entering/leaving CD mode a
      few times — critical check, the 0x6A2 sender was restructured and a
      malformed sequence lit warnings on a 2004 9-5 historically
- [ ] CD mode activates normally, audio plays (CDC handshake via new sender)
- [ ] Version banner "6.1.6 R1.16" shows ~4 s on entering CD mode
- [ ] "CONNECTED" flashes when the phone attaches
- [ ] Preset 1 → "PAIRING" on SID + phone sees BlueSaab
- [ ] Extra-long middle SEEK (>2 s) → same pairing behavior (new)
- [ ] Presets 4/5 audibly step gain down/up during playback (new)
- [ ] Presets 3/6 (reconnect/disconnect) unchanged
- [ ] NXT + track ± unchanged
- [ ] Track metadata scrolls; seam shows " - " with no stutter (A2 fix);
      no flicker/garbled text over a longer drive (A1 fix)
- [ ] Switch source away and back repeatedly → buttons never go dead
      (B1; historically a 9-5 issue)
- [ ] Entry beep still sounds (default-on config)
- [ ] `E` over serial after a drive: 0 or near-0 errors
- [ ] Observation task: note when the IHU sends pause events 0xB1/0xB0
      (unblocks the deferred pause feature)

Release v6.1.6 (tag + `gh release create` with CI artifacts) only after the
in-car phase passes.

Out of scope for v6.2: config system, shuffle, multi-device (RN52 can't).
(Track metadata on SID turned out to already exist — see docs/V6_CODE_AUDIT.md.)

Bug-fix menu (if a release ever happens): see `docs/V6_CODE_AUDIT.md` —
scroll-seam fix A2, Mail-alloc NULL checks A4, ISR-locking fix A1, dead-code
removal A3.

- [x] Document pairing properly — see `docs/USAGE_v6.md` (preset 1 = discoverable;
      long-press SEEK was v5 behavior and does nothing in 6.1.1)
- [x] iPhone pairing issue resolved: preset 1 confirmed working in-car (2026-07-22);
      old docs incorrectly listed long-press SEEK / preset 1 as volume up
- [x] ~~Consider mapping `SEEK_MIDDLE_EXTRA_LONG` to `bluetooth.discoverable()`~~ — done in v6.1.3; was:
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
- [ ] Schematic + PCB in an open tool (KiCad) so the design files are truly
      OSS — parts list ready in `docs/SUCCESSOR_PARTSLIST.md`
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
