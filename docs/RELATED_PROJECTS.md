# Related projects & products

## Lineage (our own family)

- **SAAB-CDC** — github.com/kveilands/SAAB-CDC (forks incl. si1/SAAB-CDC).
  The Arduino generation of BlueSaab: ATmega + MCP2515 + RN52, "Bluetooth aux
  input" via the CDC connector, 1998–2005 cars. Different button map
  (long-press SEEK = pairing, presets 1/2/4 = volume). Its 270-commit history
  is a useful I-Bus protocol reference.
- **BlueSaab v6** — this repo. STM32F103 + mbed + RN52.

## Commercial competitors

### CDConnect (saabaux.se), 1 595 kr

Bluetooth CDC-connector adapter, plug-and-play, CE/RoHS. Feature-parity with
BlueSaab v6: activate with double CD press, track control via wheel/IHU,
auto-reconnect last device, built-in amplifier stage, no hands-free.

Intelligence worth noting from their compatibility claims:

- Supports 9-3 gen1 1998–2002 (Cab –2003), 9-5 gen1 **1998–2005 only** —
  suggests the 2006 9-5 facelift changed the head unit/bus side even though
  the I-Bus platform ran to 2009/10. Verify before claiming successor support
  for 2006+ 9-5s.
- **Excludes navigation-equipped 9-5s** — nav owns the SID display; likely a
  display-resource conflict. Our `SidResource` negotiation may hit the same
  wall; test on nav cars or gate SID text off.
- **"Not compatible with Night Panel"** — SID/display edge case to test.
- Markets its amplifier as "higher volume than standard versions" — audio
  gain matters to users; consider a configurable gain option in the successor.

Differentiators the successor has that CDConnect lacks: SID text, multi-device
pairing/swap, open source hardware+firmware, sub-100 µA parked draw (unknown
for CDConnect, but worth beating).

## How 2006+ cars are handled (out of our scope, for the record)

The 2006 9-5 / 2007 9-3 facelift head units (black Fujitsu) **dropped the
external CD-changer input** — that's why every CDC-based device stops at 2005.
Aftermarket solutions for those cars impersonate a different factory
accessory:

- **BT Changer** (bt-changer.com, Hungary): plugs into the **XM satellite
  tuner harness** behind the head unit and emulates the factory XM tuner — an
  "XM" source appears next to AM/FM. Requires the black Fujitsu unit;
  steering-wheel support is partial on 9-3; known Fujitsu firmware bug mutes
  audio ~14 s occasionally.
- **9-3 SS 2003–2006 (GMLAN)**: AUX-connector modules on the back of the
  radio (e.g. SP-BT02), Dension Gateway 500, or retrofitting the factory OEM
  Bluetooth module (2007–2008); ~2006+ cars have a factory 3.5 mm AUX.

Supporting 2006+ would therefore be a separate product (XM emulation, other
connector and protocol), not an extension of the CDC successor.
