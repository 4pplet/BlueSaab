# BlueSaab revival — TODO

Goal: revive BlueSaab as a properly maintained open-source project, keep the
v6 hardware supported, and design an ESP32-based spiritual successor (the
RN52 is end-of-life, so v6 can no longer be built).

## Now — validate and release 6.1.7

6.1.7 is the release candidate: the 6.1.2–6.1.6 work plus the 2026-09-29 deep
audit fixes (see CHANGELOG.md). **Nothing since 6.1.1 has touched hardware
yet.** The candidate (commit, CI run, `.bin` SHA-256) is pinned in
[tools/rc.env](tools/rc.env).

- [ ] Bench + in-car validation — follow **[docs/BENCH_SESSION.md](docs/BENCH_SESSION.md)**
      (printable checklist; `tools/bench.sh` fetches and verifies the RC,
      backs up the unit, flashes, and runs a logging console)
- [ ] Record the findings in docs/V6_CODE_AUDIT.md (`tools/bench.sh summary`)
- [ ] Release per docs/BUILD_v6.md: fast-forward master, tag `v6.1.7`, check
      the draft release's `.bin` hash against `tools/rc.env`, publish

## Phase 0 — repo hygiene

- [x] HARDWARE/ committed; README, LICENSE, CLAUDE.md, CHANGELOG, docs/
- [x] Build verified (Homebrew GCC 8.5 + CI GCC 8-2019-q3, both
      byte-reproducible); CI builds 4 variants with `-Werror`, runs host
      tests with sanitizers, pinned actions + Dependabot; tag-triggered
      draft releases
- [x] Branch strategy (2026-09-29): `master` fast-forwarded to `revival`
      and is the default/release branch; `revival` stays the dev branch and
      is fast-forwarded into `master` at each release (never rebase it).
      Stale local `new` deleted.
- [x] Duplicate `HARDWARE/BlueSaab v5.0.zip` removed (byte-identical to the
      folder; the zip *inside* the folder holds the Gerbers and stays)
- [x] Fork settings: issues enabled + bug template, topics, README CI badge
- [ ] Recover v6 CAD sources if they exist (only v5 Eagle files are here)

## Phase 1 — Keep v6 alive

Shipped in 6.1.2–6.1.7 (see CHANGELOG): version banner, presets 4/5
(AVRCP volume), >2 s middle-SEEK pairing, PAIRING/CONNECTED notices,
optional beep, CAN health counters, and many robustness fixes.

Open v6 items:

- [ ] Wire IHU pause events 0xB1/0xB0 → AVRCP — deferred: the RN52 only has
      a play/pause *toggle* (`AP`), so this risks state desync. Seeing when
      the IHU sends them needs a debug build that logs decoded buttons.
- [ ] Auto-discoverable when nothing is paired — needs a design pass
      (short-window variant). Note the RN52 is *already* discoverable on
      every boot (`S%,1084` bit 2), and v6 reboots it on every STM32 boot.
- [ ] 9-5 "buttons dead until source switch" — reportedly the real CDC
      varies its status when the IHU isn't in CDC mode; needs a bus capture
      from a real changer and a 9-5 to test.
- ~~Voice assistant on long middle SEEK~~ — decided against (2026-07-22)

Out of scope for v6: config system, shuffle, multi-device (the RN52 can't).

### 6.2.0 — "the reliability release" (after 6.1.7 is validated)

Was called "6.1.7" in earlier notes. Build and validate on the bench with a
current meter; do not desk-develop.

- [ ] **Hardware watchdog (IWDG).** Kick it from a thread that checks
      heartbeats (CDCStatus 950 ms, SidResource 1 s, logThread 1 s), never
      from an ISR, plus a progress check for the RN52 thread (see below).
      It covers every remaining halt path (list in docs/V6_CODE_AUDIT.md).
- [ ] **Stop transmitting into a silent bus.** 0x3C8 and 0x357 are sent
      unconditionally. When the car sleeps, nothing ACKs and the controller
      retransmits back-to-back forever. Gate periodic TX on bus liveness
      (another node heard in the last ~2 s) and abort pending mailboxes
      (`TSR.ABRQx`) when it goes quiet. **Prerequisite for any sleep work
      and for measuring whether the parked bus is really silent.**
- [ ] **RN52 escalation:** after N failed command-mode handshakes, log it
      and recover. PWREN can only turn the RN52 *on*, so a true power-cycle
      needs switching its VDD (hardware); first find out what the bench
      `S^,600` test shows.
- [ ] **Sleep on bus silence:** STM32 stop mode + transceiver RS-standby,
      wake on CAN RX edge.
- [ ] Idle-thread `__WFI()` hook — cheap awake-current saving; measure it.
- [ ] Reset-cause telemetry: boot banner prints the reset reason (RCC_CSR:
      IWDG vs POR vs pin) + a reset counter in RTC backup registers.
- [ ] Hang-injection debug command (debug builds only) to prove the
      watchdog: wedge a thread → reset within the timeout → full recovery.
- [ ] Optional: split init so RN52 start-up never delays the CAN side
      (matters if wake is treated as a full reboot).
- [ ] 9-5 dead-buttons fix if a car and a capture are available.

**Design constraints** (fact-checked against datasheets 2026-09-29):

- **Watchdog and sleep are coupled.** The F103 IWDG can't be paused and
  keeps running in stop mode. Its max timeout is 26 s at nominal LSI, but
  LSI is 30–60 kHz, so the worst case is **17.5 s**. Kick it from an RTC
  alarm on the same LSI (the kick period scales with it), or keep the
  period under ~15 s.
- **Wake source:** SN65HVD234 **RS-standby** keeps the receiver active
  (RXD mirrors the bus, falling edge → EXTI on PB8) but draws **200–600 µA**.
  EN-sleep (0.05 µA) is deaf and can't wake anything.
- **The RN52 can't be switched off from firmware.** PWREN only turns it on;
  its disconnected standby is < 0.5 mA.
- **Realistic parked current, v6 with sleep:** roughly 0.5–1.2 mA plus
  regulator Iq (LM1117 ~5–10 mA). With a low-Iq regulator, about 20×
  better than today's (unmeasured, est.) 15–30 mA — not the 0.2 mA
  estimated earlier.
- Resume after stop: restore PLL/72 MHz → RTOS tick → RS low → CAN reinit.
  The first wake frame is lost by design (the IHU re-polls). `HAL_GetTick`
  waits need interrupts enabled — never run CAN re-init from the wake ISR.

**Sleep risk register:**

1. Wakes wrong: a botched clock/RTOS/CAN restore skews protocol timing —
   the warning-lamp failure class. Treat wake as a reboot, and check `E`
   after wake cycles.
2. Doesn't sleep: a symptomless drain. Only a current meter proves sleep.
3. Wake-storm: if the locked car's bus isn't truly silent, the unit
   oscillates. Measure it with our own TX gated off (otherwise we are the
   noise).
4. Watchdog-kick slip → nightly reset cycles; visible via the reset counter.
5. Wrong-moment sleep: threshold tuning.
6. A sleeping unit looks dead on the bench. Document "wake it first; the
   BOOT0 bootloader always works".

**Validation:** bench with a 12 V supply and current meter; silence→sleep→
wake cycles by starting/stopping bus traffic; the hang-injection test; then
a multi-day in-car soak (battery voltage before/after, CD mode must just
work, zero unexplained resets on the counter).

### Optional hardware mod — low-Iq regulator + input TVS

Only worthwhile together with 6.2.0 sleep; the LM1117's own ~5–10 mA Iq
dominates a sleeping board. **Not a drop-in swap.**

- **Do NOT solder an MCP1792 (or MCP1703A) onto the LM1117 footprint.**
  Their SOT-223 pinout is VIN–GND–VOUT with the tab on GND, while the
  LM1117 is GND–VOUT–VIN with the tab on VOUT. 12 V would land on the
  MCP1792's output and destroy it.
- MCP1792 facts: 4.5–55 V (70 V transient), 25 µA Iq, **100 mA** max. It
  needs an adapter board or rewiring, and the measured peak current must
  fit under 100 mA.
- TLV761 is a true LM1117 drop-in (~65 µA Iq, 1 A), but only 20 V abs max
  like the LM1117. It needs input clamping below 20 V, which an SMBJ33A
  (53 V clamp) does not provide.
- Drop MCP1703A (18 V abs max).
- An SMBJ33A (clamps ≤ 53.3 V) is coherent only in front of a ≥ 55–70 V
  part like the MCP1792.

## Phase 2 — Successor hardware (ESP32)

Spiritual successor to BlueSaab v6 — new hardware and firmware, but a
**familiar in-car interface**: the released v6 button map (`docs/USAGE_v6.md`,
including the 6.1.3 additions) works unchanged; improvements go only on
unused buttons (preset 2, long SEEK±, RANDOM). Hardware analysis:
`docs/SUCCESSOR_HARDWARE_OPTIONS.md`.

**Decided 2026-07-22: single ESP32** (Option A) — one chip for BT audio, CAN,
and app logic. **Target cars: pre-2006** (9-3 gen1 1998–2002/03, 9-5 gen1
1998–2005); 2006+ facelift and nav-equipped cars out of scope for now.

- [ ] Pick a project name
- [ ] Design multi-device swap UX (several paired phones, switch from the
      driver's seat — e.g. cycle on preset 3 with device name on SID)
- [ ] Order prototype parts: ESP32 WROOM-32E devkit + PCM5102A board + CAN
      transceiver breakout (SN65HVD230 is fine for the bench)
- [ ] Prototype A2DP sink + AVRCP with ESP-IDF (original ESP32 required —
      S3/C3/C6 have no Bluetooth Classic)
- [ ] Prototype I-Bus on TWAI @ 47.619 kbit/s: BRP 80, TSEG1 15, TSEG2 5
      (1 µs tq, 76 % sample point — identical to v6's proven timing)
- [ ] Port the protocol layer (SaabCan / CDCStatus / Buttons / SidResource),
      with the lessons in docs/V6_CODE_AUDIT.md (no work in the RX ISR, one
      sender per frame ID, gate TX on bus liveness, watchdog from day one)
- [ ] Schematic + PCB in KiCad — parts list in `docs/SUCCESSOR_PARTSLIST.md`
- [ ] Power input rated for automotive transients: a 60 V-class low-Iq buck
      (e.g. LMR36015/LMR36006) behind an SMBJ33A. AP63203 (35 V abs max)
      and TPS54202 (30 V) are too weak for a suppressed load dump. Decide
      how to handle an unsuppressed load dump (79–101 V) — surge stopper or
      a documented assumption.
- [ ] Sleep architecture: ESP32 deep sleep on I-Bus silence, wake on CAN RXD
      via EXT0/EXT1, with a **wake-capable transceiver** (e.g. TCAN3414,
      ~10 µA standby with remote wake) — the SN65HVD234's listening standby
      alone draws 200–600 µA. Target < 100 µA parked.

## Phase 3 — Community

- [x] Changelog; flashing guide; v6.1.1 released with binaries
- [ ] Tagged 6.1.7 release (after validation)
- [ ] Issue templates (see Phase 0); CONTRIBUTING only if contributors appear
