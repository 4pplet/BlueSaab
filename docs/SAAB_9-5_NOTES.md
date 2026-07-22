# SAAB 9-5 — bus architecture, CDC support, and emulator quirks

Research notes, 2026-07-22. Sources are linked inline; note that **Tomi
Liljemark's `pikkupossu.1g.fi` and the old BlueSaab Nabble forum are dead** —
use the Wayback captures linked below.

## Bus architecture (OG 9-5, 1998–2009/10)

- **I-Bus**: 47.619 kbit/s, CAN 2.0A 11-bit IDs, 8-byte frames, 2-wire
  ISO 11898-2. Same protocol as the OG 9-3. Nodes: MIU, SID, TWICE, ACC,
  DICE, IHU/audio, **CDC**, SPA, PSM, airbag.
  [I-Bus protocol (Wayback)](http://web.archive.org/web/20250122150340/http://pikkupossu.1g.fi/tomi/projects/i-bus/i-bus.html)
- **P-Bus**: 500 kbit/s powertrain bus (Trionic ECM, TCM, ABS/ESP).
  [P-Bus (Wayback)](http://web.archive.org/web/20250122150436/http://pikkupossu.1g.fi/tomi/projects/p-bus/p-bus.html)
- **Gateway**: the MIU bridges P-bus ↔ I-bus; they are electrically isolated.
  [WIS 2002](https://saabwisonline.com/9-5-9600/2002/3-electrical-system/bus-and-diagnostics-communication/technical-description/p-bus-and-i-bus)
- **2006 facelift**: the dual-lead I-bus (with IHU and CDC on it!) remains,
  but a new **single-lead I-bus** (GMLAN-style SWCAN) is added for the GM base
  radio (EHU), satellite radio, and OnStar; the **SID bridges the two buses**.
  [WIS 2007](https://saabwisonline.com/9-5-9600/2007/3-electrical-system/bus-and-diagnostics-communication/technical-description/p-bus-and-i-bus)
- The OG 9-5 kept this architecture to the end: last sedan July 2009, last
  wagon Feb 2010. The NG 9-5 (2010+) is Epsilon-2/GMLAN — incompatible.
- Confirmed: the 2003+ 9-3 Sport Sedan is GMLAN (33 k single-wire + optical
  O-bus for audio) — BlueSaab-style emulators cannot work there. The 2003
  9-3 **Convertible** stayed on the old platform.

## CDC support by year/head unit

- **1998–2005 (AS1/AS2/AS3)**: factory CDC supported everywhere. The real
  changer is VIN-married via Tech2; emulators bypass this with SECURITY byte
  `0xD0` ("married, VIN matches") in frame 0x3C8.
- **Wiring difference vs 9-3**: the 9-3 has a single ready CDC plug in the
  trunk. Many 9-5s have **two chassis connectors** (6-pole + 2-pole I-bus)
  behind the left rear trim and need Saab adapter harness **400129243**;
  cars from ~mid-2000 on were often **not pre-wired at all**.
  [BlueSaab blog](http://bluesaab.blogspot.com/2014/01/9-5-adapter-harness.html)
  (The PDF in `HARDWARE/` about "cars without CDC provisioning" covers this.)
- **Navigation cars**:
  - Kenwood nav ('02–'04): conflicting reports — one firsthand "incompatible"
    (own DIN-plug changer protocol), one secondhand success. Unresolved.
  - Denso nav ('04+ incl. 2006 facelift): **confirmed working** — the nav
    screen shows disc/track itself; the SID is not used (SID text features
    silently do nothing).
- **2006+**: CDC protocol still lives on the dual-lead I-bus per WIS, and a
  BlueSaab co-admin daily-drove a **2006 9-5 wagon (Denso nav)** on BlueSaab.
  Unknown: base-radio (GM EHU, mostly US) cars — no confirmed install either
  way. Not pre-wired; wiring must be added.

## 9-5-specific emulator quirks (all field-verified by others)

1. **Node-status handshake is mandatory on the 9-5.** The 9-3 accepts
   periodic 0x3C8 alone; the 9-5 IHU also requires a 0x6A2 reply sent in
   response to each received 0x6A1. (v6 `CDCStatus.cpp` implements this —
   the "here be dragons" section — and must be ported faithfully.)
2. **A malformed 0x6A2 can light airbag + MIL warnings** with a chime
   (wrong first byte, e.g. 0x64 instead of 0x32, on a 2004 9-5). The
   0x32/0x42/0x52/0x62 reply sequence in v6 encodes the fix.
3. **Buttons can go dead until source is switched away and back** — 9-5
   only; suspected 0x6A1/0x6A2 timing. The real CDC also sends a different
   status when the IHU is not in CDC mode (never implemented in v6).
4. **SID text flicker** on some cars (both 9-3 and 9-5 reported) — the
   0x348/0x368 display-resource handshake is timing-sensitive; get the
   grant before writing, re-request each ~1 s.
5. **Frame pacing**: same-ID frames ≥10 ms apart; flooding the I-bus can
   reset the MIU and SID and is suspected of causing spurious warning
   lights via diagnostics starvation.
6. **Beeps (0x430) aggravate the 9-5** — debugging advice from the field
   was "remove the beeps". v6 already dropped beep feedback.
7. **Audio variants (AS1/AS2/AS3 — internal amp / Pioneer / H-K)**: no
   difference for emulators; CDC input is always balanced line-level into
   the head unit. Drive balanced outputs (v6's THS4522 stage does).
8. **Night panel**: no documented CDC quirk; the SID blanks text in night
   panel, which users can mistake for a device fault.
9. **Steering-wheel frames are identical 9-3 vs 9-5** (0x290, 0x3C0/0x3C8);
   only physical button layouts differ. The one protocol delta is the
   0x6A1/0x6A2 strictness above.

## Implications for the ESP32 successor

- The pre-2006 scope is **conservative and safe**; 2006+ Denso-nav cars are
  a proven bonus, base-EHU 2006+ cars are unverified. Revisit after launch.
- Port `CDCStatus`'s node-status logic exactly — it embeds hard-won 9-5
  fixes (handshake, reply sequence, 140 ms interval).
- Enforce ≥10 ms same-ID spacing and never busy-flood the bus.
- SID text must remain compile-time/config-optional (nav cars, flicker).
- Installation docs must cover the 9-5 two-connector harness (400129243)
  and non-pre-wired cars.

## Primary sources

- [I-Bus protocol — Tomi Liljemark (Wayback)](http://web.archive.org/web/20250122150340/http://pikkupossu.1g.fi/tomi/projects/i-bus/i-bus.html)
- [9-5 handshake findings — SaabCentral (Wayback)](http://web.archive.org/web/20230323083251/https://www.saabcentral.com/threads/canbus-version-8bit-29bit-2004-9-5.286321/)
- [BlueSaab forum threads (Wayback)](http://web.archive.org/web/20180622201619/http://bluesaab-forum.2349123.n4.nabble.com:80/Functions-currently-supported-by-the-software-td123.html)
- [Saab WIS online — 9-5 bus communication](https://saabwisonline.com/9-5-9600/2007/3-electrical-system/bus-and-diagnostics-communication/technical-description/)
- [9-5 Audio FAQ](https://z90.pl/saab/Angry%20Kitchen%20Appliances_%20Saab%209-5%20Audio%20FAQ.html)
- [iSaab — another open CDC emulator](https://github.com/mcaldwelva/iSaab)
