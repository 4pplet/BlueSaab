# v6 code audit — correctness & robustness findings (6.1.1 baseline → 6.1.7)

First audit 2026-07-22, full read of the firmware source; later rounds are
appended below (newest: 2026-09-29 deep audit → 6.1.7). Scope at the time:
bugs and robustness only (v6 firmware work later resumed as 6.1.2+). Each
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
  in `RN52::initialize`) — the earlier TODO menu item claiming otherwise was wrong
  and has been removed.
- v6 **already shows scrolling track metadata** (artist – title) on the SID:
  on track change it queries the RN52 (`AD`) and feeds the scroller; the
  static "BlueSaab v6" text is only the fallback. USAGE_v6.md updated.

## Deployed-unit findings (to fill in after bench session)

The project's own unit reportedly always shows the static "BlueSaab v6"
text. Candidate causes and the diagnosis recipe are in USAGE_v6.md
troubleshooting (firmware age vs RN52 < 1.16 vs dead event chain). Also
confirmed during this investigation: **no STM32-side power saving exists in
v6.1.1** — `main()` idles, `bt_pwren_pin` stays 1, transceiver sleep pins
unused. The RN52 itself is configured to power off after 10 min unconnected
(`S^,600`). Parked draw is unmeasured (estimate 15–30 mA).

- [ ] Boot banner version on the unit: ___ (also:
      `strings backup_v6_unit.bin | grep -i version` on the flash backup)
- [ ] RN52 module firmware version (`RN52 version:` boot line after flashing
      6.1.2+ — `d` only prints the Bluetooth address): ___
- [ ] Metadata behavior on track change: ___

## Deep audit (2026-09-29, four reviewers → v6.1.7)

Whole repo re-audited: firmware (fresh eyes, disassembly checked against the
vendored RTX sources), docs vs code, CI/release, and external facts against
datasheets. The firmware findings below are fixed in 6.1.7 unless marked
otherwise.

Permanent-halt paths fixed (no watchdog exists, so each was a unit that
stays dead until its power is cut):

- **Regression from 6.1.6:** `can_frequency()` gave INAK 2 ms and then called
  `error()`. Since 6.1.6 constructs the CAN object at 47.619 kbit/s, the
  controller is bus-synchronized when `SaabCan::initialize()` re-applies the
  bitrate, so entering init mode waits out the frame in flight (up to
  ~2.8 ms) — ~1-2 % of boots with an active bus halted. Now 20 ms, no
  `error()`, the INRQ request is withdrawn on timeout, and HAL init runs in
  silent mode so the placeholder 1.2 Mbit/s timing can't put error flags on
  the bus.
- ID-0 CAN frame → the empty callback slots (id 0) → `Callback::call` →
  `MBED_ASSERT(_ops)` → `mbed_die`. Standard, extended, data or remote.
  Now: only standard data frames are dispatched, and only to set slots.
- RN52 command `Queue` overflow inside RTX: ISR-posted messages are only
  counted at PendSV, so two posts from one ISR (CDC-on posts two) into a
  queue at 19/20 both pass → `OS_ERR_MBX_OVF` → halt. Needs a stalled RN52
  thread to fill the queue. Now an own occupancy count, updated under a
  critical section at post time, refuses at 18.
- `can_init_freq()` still called `error()` on HAL-init / bitrate failure
  (stuck-dominant bus at boot). Now logged and retried in
  `SaabCan::initialize()`.

Other fixes: TX frames retried ~20 ms when all 3 mailboxes are busy
(previously dropped at once — a lost 0x6A2 breaks the 9-5 handshake);
invalid UTF-8 / Latin-1 metadata no longer swallows following letters
(host-tested, fuzzed); `showTemporary()` fully atomic (PRIMASK save/restore,
safe from ISR and thread); SID event flag volatile and captured atomically;
four 256-byte stacks → 320 (estimated 80-86 % full); banner buffer size
checked at compile time; debug help text corrected.

RAM budget (6.1.6 figures from the ELF): static 10.0 KB, heap used ~5.1 KB
(thread stacks are heap-allocated in `Thread::start`), ~1.3 KB free in the
heap's top chunk before these stack increases (~1.0 KB after). Nothing
allocates after boot, so it is not a live risk, but it is a hard ceiling
for 6.2.0 — a new thread or buffer of more than ~1 KB halts at boot.
Reclaimable if needed: `OS_TIMERS=0` (−800 B, RTX timers unused), logThread
1536→1024 (−512 B), log mailbox 64→32 (−~390 B), idle stack 512→256.

Open — design input for 6.2.0, not fixed here:

- **The node transmits forever, even into a parked, silent bus.** 0x3C8
  (every 950 ms) and 0x357 (every 1 s) are sent unconditionally. With
  nothing ACKing, the controller goes error-passive (never bus-off) and
  retransmits back-to-back indefinitely: extra transceiver current, maybe
  keeping other nodes awake, and it would make an RX-counter "is the parked
  bus silent?" measurement meaningless. Fix: gate periodic TX on bus
  liveness (a frame received from another node in the last ~2 s) and abort
  pending mailboxes (`TSR.ABRQx`) when the bus goes quiet.
- The RN52 thread retries the command-mode handshake forever (1 Hz) if the
  module never answers — Bluetooth dead while the thread looks alive.
  Needs escalation (power-cycle via PWREN after N failures) and a
  progress-based watchdog check. Related open question: what `S^,600`
  auto power-off actually requires to wake the module.
- Idle thread busy-spins at 72 MHz (empty idle hook); a `__WFI()` hook is a
  cheap current saving — measure it.
- Init could be split so RN52 start-up (5 s) never delays the CAN side
  (matters if 6.2.0 treats wake as a full reboot).
- NITs: DLC ignored (handlers read `data[]` regardless of `len`), async
  serial re-arm results unchecked, `txDropped++` not atomic.

Halt-path inventory after 6.1.7 (what the 6.2.0 watchdog must cover): RTX
stack overflow (detected only at context switch), heap exhaustion at boot
(latent), HardFault (default `b .`), crystal failure loop at boot, RTX ISR
post-queue overflow (`OS_FIFOSZ` 16 — ~15 posts if one CAN ISR pass drains
three CDC-on frames; needs ≥ 2 frame times of ISR latency, practically
unreachable), and the "alive but useless" RN52 retry loop. The IWDG covers all halts — `mbed_die`
disables interrupts but the IWDG is independent — but not the RN52 loop,
which needs a progress check. Kick it from a thread that checks heartbeats
(CDCStatus 950 ms, SidResource 1 s, logThread 1 s), never from an ISR.

## Review of the 6.1.7 changes (2026-09-30 → re-pinned 6.1.7)

An independent adversarial review of the 6.1.6 → 6.1.7 firmware diff itself
(the deep-audit fixes had been written after that audit's reviewers
finished). It positively verified the RN52 queue guard (no counter drift on
any path), the UTF-8 rewrite (3 M fuzzed strings, output identical to the old
code on all valid UTF-8), the CAN register semantics, and the RAM budget
(~1.0 KB heap free). Found and fixed:

- **MAJOR — regression:** 6.1.7 made `mode(CAN::Normal)` unconditional in
  `SaabCan::initialize()` (6.1.1 gated it on the bitrate setup succeeding).
  If both bitrate attempts timed out, `can_mode()` could clear SILM on the
  HAL's placeholder 1.2 Mbit/s timing — an error-active node at the wrong
  bitrate, repeatedly corrupting I-Bus traffic (bus-off/ABOM recovery cycles)
  until the battery is disconnected. Needed a stuck-then-freed bus at boot.
  Fixed: normal mode only after a successful bitrate setup, and `can_mode()`
  never touches BTR unless it really entered init mode.
- MINOR: the TX retry could put same-ID frames back-to-back (violating the
  ≥ 10 ms rule) → per-ID spacing enforced at write time. A UTF-8 lead byte
  truncated at end of string became a spurious Latin-1 letter → dropped.
  `make EXTRA_FLAGS=…` doesn't rebuild → documented `make clean`.
- NIT: RX-overrun counter reset after the RX handler is attached (overruns
  before that are expected); `onRx` also requires a non-zero slot id.
- Noted, not fixed: CAN init has only one retry — a double failure leaves CAN
  off until power-cycle (6.2.0 watchdog / periodic self-check). 0x6A2 frames
  are spaced at exactly the 140 ms maximum, so a retry delay can stretch a gap
  past the +10 % tolerance (rare; kept the field-proven interval).

## Bug hunt (2026-07-22, three adversarial reviewers → v6.1.6)

Full-codebase hunt by three independent reviewers (RN52/serial stack,
display/text stack, CAN layer). All fixes in v6.1.6; all bugs below were
**pre-existing since ≤v6.1.1** unless noted.

RN52/serial stack:

- MAJOR: `queueCommand` silently dropped commands on a full queue → now
  logged. MAJOR: details handler passed a mutable global as a *deferred
  printf format string* (`%` in a track title → garbled log or hardfault) →
  own buffer via `%s`. MAJOR: a phone connecting during the 5 s boot wait
  was never detected (GPIO2 is a pulse; no initial state query) leaving all
  AVRCP buttons dead → initial `Q` query added.
- MINOR: `logFrame` heap leak on full mailbox; unbounded response-drain
  loops (now capped at 40 lines); stale `a2dpConnected` after debug reboot.

Display/text stack (this reviewer host-compiled and ran the dormant unit
tests — all 60 Scroller asserts pass; core scroll math is clean):

- MAJOR: `utf_convert` passed 3/4-byte UTF-8 (curly quotes, dashes, emoji)
  through as raw bytes → garbage glyphs on the SID; now consumed+dropped
  like unknown 2-byte sequences. MAJOR: `writeTextOnDisplayUpdateNeeded`
  was never cleared — every SID write since first activation carried the
  "event" mark (0x82) instead of static (0x02); now one-shot. Another
  strong flicker candidate.
- MINOR: `sendCdcStatus` byte-0 expression was wrong for 2 of 4
  event/remote combinations (latent — callers pass them in lockstep);
  corrected to match the documented bit layout with no on-bus change for
  existing call patterns. ~5 s boot window with CAN IRQ live but no
  handlers/no 0x3C8 (bluetooth init blocked first) → init reordered,
  CAN handlers now attach in ms. 0x337 text groups lacked the seam/tear
  guard 0x6A2 got in v6.1.5 → 35 ms pacing in writeGrantedText.
  Breakthrough flag capture-and-clear made atomic; breakthrough now only
  requested for recognized buttons, not every 0x80 frame. logThread stack
  1024→1536 (full-newlib vfprintf).

CAN layer (bit timing verified EXACT: PCLK1 36 MHz, prescaler 36, 21 tq,
TSEG1 15/TSEG2 5, sample point 76.19 %, SJW 2, BTR 0x014E0023 →
47 619.048 bit/s = 0 ppm vs the true 10⁶/21 I-Bus rate):

- MEDIUM: the global CAN object joined the live I-Bus at **100 kbit/s
  error-active during static init**, corrupting car-bus frames at every
  ignition-on until `initialize()` set the real rate → constructed at
  47619 directly. MEDIUM: `TXFP=0` let same-ID frame groups reorder by
  mailbox index under bus load → FIFO order enabled. MEDIUM: RX FIFO
  overruns were silent → counted, cleared, shown in debug `E`.
- LOW: INAK waits bounded (stuck-dominant bus degrades instead of hanging
  boot). Latent/noted: FIFO1 gating mismatch (unused), no bus-off
  telemetry (ABOM recovers silently), crystal-fail silent hang in vendored
  clock code (not touched).

## Fix status

- **Fixed in v6.1.3:** A2 (scroll seam), A3 (dead overloads deleted),
  A4 (Mail::alloc NULL checks), A6 (lowercase hex).
- **Fixed in v6.1.4:** A1 (SID text formatting moved from CAN ISR to the
  SidResource thread; locking now effective), B1 (single sender thread for
  all 0x6A2 sequences), A5 (TX error/drop counters, debug `E`), B3
  (truthful init log). SidResource stack bumped 256→384 (B4 partial).
- **Open:** B6 (remaining ISR-context frame callbacks — Buttons/CDCStatus
  handlers still run in the RX interrupt; they only queue/signal, which is
  ISR-safe, but the successor should dispatch to a task regardless).
  (B2/B5 fixed; B4 partially — the stack monitor is available via
  `STACK_MONITOR_ENABLED` and stacks were raised, but margins have not been
  measured on hardware yet.)
- **Design gap (found 2026-07-22, planned for 6.2.0): no watchdog.** The
  IWDG is never enabled, on an always-powered device with no reachable
  reset — any firmware hang persists until the harness is unplugged, with
  ~25 mA battery drain. The successor must also treat a watchdog (ESP32
  task WDT) as mandatory from day one.
- Bench validation of all of the above: pending (v6.1.2+ has not yet
  touched hardware).

## Independent adversarial review (2026-07-22, → v6.1.5)

A fresh-eyes adversarial review of the full v6.1.1→v6.1.4 diff (verifying
RTX semantics against the vendored sources) found and v6.1.5 fixed:

- **MAJOR: `scroller.clear()` still ran in the CAN ISR** via
  `activate()/deactivate()`. Worse than a skipped lock: RTX semaphore *wait*
  fails in ISR (unchecked) but *release* succeeds, so each CDC mode change
  inflated the semaphore by one token — permanently making it a two-owner
  lock and re-enabling the very corruption v6.1.4 claimed to fix. Now: ISR
  sets `clearPending` + signal 0x40; the SidResource thread clears.
- **MAJOR: 0x6A2 seam violation + dropped polls** in NodeStatusSender: a
  poll arriving mid-sequence started the next sequence with zero gap after
  the previous one's last frame (violating the ≥10 ms same-ID rule), and
  coalesced signals answered only the highest-priority poll. Now: ≥10 ms
  seam guard + every requested sequence is sent.
- MINOR: grants arriving during the 100 ms request-spacing sleep were
  delayed; the spacing wait now services grant/clear signals.
- MINOR: `tempGrants`/`tempText` ISR-vs-thread races; snapshot now taken
  under a brief critical section.
- NITs: protocol doc button table updated, PAIRING display now 10 s to
  match the RN52 pairing window.

The review also positively verified (against vendored RTX sources): the
signal_wait(0) usage, ISR-safety of signal_set/Mail/Queue calls, ticker
wraparound handling, all new buffer bounds, V-command retry behavior, and
preservation of the 0x357/0x3C8/140 ms/10 ms protocol cadences.
