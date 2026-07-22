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
