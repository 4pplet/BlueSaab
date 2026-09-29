# BlueSaab

[![build](https://github.com/4pplet/BlueSaab/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/4pplet/BlueSaab/actions/workflows/build.yml)

A CD-changer emulator that brings Bluetooth audio to classic SAAB cars.
BlueSaab plugs into the CD-changer connector, speaks the SAAB I-Bus CAN
protocol so the factory head unit believes a changer is present, and streams
music from your phone through the car's original audio path — steering-wheel
controls included.

**Supported cars:** SAAB 9-3 (1998–2002, Convertible –2003) and SAAB 9-5
(1998–2005; a 2006 Denso-nav 9-5 is confirmed working, other 2006+ cars are
unverified — see the 9-5 notes). The 2003+ 9-3 Sport Sedan uses a different
(GMLAN) architecture and is not supported.

## Current hardware: v6.1 (STM32 + RN52)

- STM32F103RB MCU running mbed OS 2 (vendored), Microchip RN52 Bluetooth 3.0
  audio module, CAN at 47.619 kbit/s
- Firmware: **v6.1.1** is the latest release
  ([download](https://github.com/4pplet/BlueSaab/releases)); this repo's root
  is the firmware source, currently **v6.1.7** — unreleased, pending in-car
  validation (see [CHANGELOG.md](CHANGELOG.md))
- Board design files in [HARDWARE/](HARDWARE/README.md)
- The RN52 went end-of-life in 2024 and Arm retired Mbed in 2026, so v6
  hardware is frozen: existing units keep working (modern iPhones/Androids
  stream to it fine over A2DP) and still get firmware improvements, while
  the main effort is the successor below.

## Using it

See **[docs/USAGE_v6.md](docs/USAGE_v6.md)** — pairing procedure, full
button map, debug serial console, troubleshooting. Short version: select the
CD-changer source, press preset **1**, pair with "BlueSaab v6" on your phone.

## The successor (in design)

A spiritual successor is being designed around a single **ESP32**: same
in-car interface, same connector, plus multi-device pairing/swap and a
sub-100 µA parked power target. Design docs:

- [docs/SUCCESSOR_HARDWARE_OPTIONS.md](docs/SUCCESSOR_HARDWARE_OPTIONS.md) —
  hardware evaluation and decisions
- [docs/IBUS_PROTOCOL.md](docs/IBUS_PROTOCOL.md) — the I-Bus CDC protocol
  (the spec any new firmware must honor)
- [docs/SAAB_9-5_NOTES.md](docs/SAAB_9-5_NOTES.md) — 9-5 bus research and
  model-specific quirks
- [docs/RELATED_PROJECTS.md](docs/RELATED_PROJECTS.md) — lineage,
  competitors, and how 2006+ cars are handled
- [TODO.md](TODO.md) — the roadmap

## Building v6 firmware

See [docs/BUILD_v6.md](docs/BUILD_v6.md) (toolchain, CI, release process) and
[docs/FLASHING_v6_HOWTO.md](docs/FLASHING_v6_HOWTO.md).

## Credits & history

- Hardware design: Seth Evans (bluesaab.blogspot.com)
- Initial code: Seth Evans and Emil Fors
- v6 firmware: Girts Linde and Karlis Veilands
- I-Bus protocol research: Tomi Liljemark
  ([archived](http://web.archive.org/web/20250122150340/http://pikkupossu.1g.fi/tomi/projects/i-bus/i-bus.html))
- RN52 handling based on RN52lib by Tim Otto
- Predecessor codebase: [SAAB-CDC](https://github.com/kveilands/SAAB-CDC)
  (Arduino generation)

## License

GPL-3.0-or-later — see [LICENSE](LICENSE).
