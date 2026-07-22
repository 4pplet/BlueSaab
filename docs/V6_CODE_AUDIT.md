# v6.1.1 code audit — correctness & robustness findings

Audit date 2026-07-22, full read of the firmware source. Scope: bugs and
robustness only — **no feature work** (v6 is frozen; see TODO.md). Each
finding notes whether it matters for a possible v6 patch and/or as a
"don't port this" flag for the ESP32 successor.

## A. Bugs

### A1. Scroller locking is ineffective in ISR context — likely the SID flicker

`Scroller.h` admits the design: text is *set* from the Bluetooth thread and
*read* from the CAN RX **interrupt** (`SidResource::grantReceived` →
`scroller.get()`), guarded by a `Semaphore` "because a mutex cannot be used
in ISRs". But in CMSIS-RTOS1, `osSemaphoreWait` is **also not callable from
an ISR** — it fails immediately, the return value is unchecked, and `get()`
proceeds without any lock. A track change during a display grant can tear
the text mid-copy. This is the best in-code candidate for the community's
"SID text flickers/garbles" reports.

Proper fix (v6 or successor): never touch the scroller from the ISR — have
the grant handler just signal a thread, format + send from there. Then an
ordinary mutex works. (`SidResource` already owns a thread.)

### A2. Scroll seam glitch: separator/length mismatch

`Scroller::set_info` appends a **1-char** `" "` separator but advances
`text_len += 3` — leftover from the earlier `" - "` separator, which the
commented-out unit tests in `Scroller.cpp` still expect. Result: two
phantom scroll positions at wrap-around, a visible stutter/duplicate at the
seam. Fix: restore `strcat(text, " - ")` (matches the tests) — one line.

### A3. Dead and wrong: `SaabCan::sendCanMessage` overloads

Both overloads are **never called**. The `(format, id, len, data)` one is
also broken: it passes `format` where the `CANMessage` constructor expects
the CAN **ID** (arguments misordered), so it would transmit on ID 0/1.
Delete both; do not port.

### A4. Unchecked `Mail::alloc()` — hardfault under queue exhaustion

- `SaabCan::sendCanFrame/sendCanMessage`: placement-new on the alloc result
  without a NULL check; a full 16-deep TX queue → write through NULL.
- `SerialLog::log`: same pattern (`e->time = ...` on NULL).

Both are only reachable under flood (TX backlog, log storm), but the
failure is a hardfault — in a car. Add NULL checks that drop the frame/log
line instead. `SerialRX::onSerialRX` does this correctly; copy its pattern.

### A5. `iBus.write()` result ignored

TX failures drop frames silently (debug code that read the error counters
is commented out). Mitigated: the custom `common/can_api.c` sets
`ABOM = ENABLE`, so bus-off auto-recovers — good, and worth keeping in the
successor. Improvement: count TX errors and expose via the debug console.

### A6. Lowercase hex not decoded (`getVal`)

`parseQResponse` validates with `isxdigit()` (accepts `a–f`) but `getVal()`
only decodes digits and `A–F` — lowercase would silently mis-decode. The
RN52 emits uppercase in practice; latent only. One-line fix.

## B. Robustness / fragility

- **B1. Three `MessageSender` threads share frame ID 0x6A2** (power-on /
  active / power-down replies). Each spaces its own 4 frames by 140 ms, but
  nothing prevents two *senders* from interleaving when the IHU switches
  state quickly — violating the ≥10 ms same-ID rule and scrambling the
  reply sequence the 9-5 is strict about. Plausible contributor to the 9-5
  "buttons dead until source switch" issue. Successor: one sender, one
  queue per frame ID.
- **B2. `SaabCan::attach` fails silently** when its 10-slot table is full
  (5 used today) and reserves ID 0 as an empty-marker. Fine now; add an
  assert/log if ever touched.
- **B3. `RN52::initialize` logs "configuration completed!"** after a blind
  5 s wait — the queued commands may or may not have been processed. Log
  truthfulness only.
- **B4. 256-byte thread stacks** (MessageSender, SaabCan TX, SidResource)
  with the stack monitor (`registerThread`/`printThreads`) commented out.
  It held in the field, but any code change should re-enable the monitor
  and re-measure before shipping.
- **B5. `title[]` global doubles as a scratch buffer** for the debug `d`
  command's `BTA=` line. Works (scroller keeps its own copy) but is a trap.
- **B6. Heavy work in the CAN RX interrupt**: all frame callbacks run in
  ISR context — including SID text formatting (see A1). The RTX calls used
  (`Mail::alloc/put`, `signal_set`, `Queue::put`) are ISR-safe, so this
  works, but the successor should dispatch frames to a task/thread and keep
  the ISR to a copy + queue push.

## Cleared during audit (looked suspicious, is fine)

- 47.619 kbps timing: custom `can_api.c` computes BTR correctly from the
  requested rate; bus-off auto-recovery (ABOM) enabled.
- `SerialRX` buffer termination: bounds are safe (`RX_BUF_SIZE-1` NUL +
  match-char NUL at ≤ index 79).
- `utf_convert`: table-driven Latin transliteration with bounded copies.
- `sendCanFrame`'s reliance on default `CANMessage.len == 8` — implicit but
  correct (mbed initializes len to 8).

## Facts discovered that correct earlier docs

- v6 **already sets RN52 gain to max at boot** (`RN52_SET_MAXVOL` = `SS,0F`
  in `RN52::initialize`) — the v6.2 menu item claiming otherwise is wrong
  and has been removed.
- v6 **already shows scrolling track metadata** (artist – title) on the SID:
  on track change it queries the RN52 (`AD`) and feeds the scroller; the
  static "BlueSaab v6" text is only the fallback. USAGE_v6.md updated.

## Deployed-unit findings (to fill in after bench session)

The project's own unit reportedly always shows the static "BlueSaab v6"
text. Candidate causes and the diagnosis recipe are in USAGE_v6.md
troubleshooting (firmware age vs RN52 < 1.16 vs dead event chain). Also
confirmed during this investigation: **no power-saving code exists in
v6.1.1** — `main()` idles, `bt_pwren_pin` stays 1, transceiver sleep pins
unused; ~25 mA constant draw is inherent to this firmware.

- [ ] Boot banner version on the unit: ___
- [ ] RN52 module firmware version (`d`): ___
- [ ] Metadata behavior on track change: ___

## Fix status

- **Fixed in v6.1.3:** A2 (scroll seam), A3 (dead overloads deleted),
  A4 (Mail::alloc NULL checks), A6 (lowercase hex).
- **Fixed in v6.1.4:** A1 (SID text formatting moved from CAN ISR to the
  SidResource thread; locking now effective), B1 (single sender thread for
  all 0x6A2 sequences), A5 (TX error/drop counters, debug `E`), B3
  (truthful init log). SidResource stack bumped 256→384 (B4 partial).
- **Open:** B2 (attach() silent overflow), B4 (stack monitoring in other
  threads), B5 (title[] scratch reuse), B6 (remaining ISR-context frame
  callbacks — Buttons/CDCStatus handlers still run in the RX interrupt;
  they only queue/signal, which is ISR-safe, but the successor should
  dispatch to a task regardless).
- Bench validation of all of the above: pending (v6.1.2–6.1.4 have not yet
  touched hardware).
