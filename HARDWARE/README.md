# BlueSaab Hardware

Board design files for the BlueSaab CD changer emulator — a device that plugs into
the CD changer connector of older SAAB cars (9-3/9-5 with I-Bus) and streams
Bluetooth audio via a Microchip RN52 module.

## Contents

| File | Description |
| --- | --- |
| `BlueSaab_v6.PDF` | Schematic for hardware v6 (the version the current firmware, 6.1.1, targets) |
| `BOM_PartType-BlueSaab_v6_1_1.csv` | Bill of materials for v6.1.1 |
| `BlueSaab v5.0/` | Full v5.0 design: Eagle `.sch`/`.brd` sources plus PDF exports ("RN52 v5.0 + 10pin") |
| `BlueSaab v5.0.zip` | Zipped copy of the v5.0 folder |
| `Hardware - Installing BlueSAAB module in cars without CDC provisioning cable.pdf` | Install guide for cars lacking the CDC pre-wiring |
| `Screen_Shot_2016-10-26_at_09.png` | Reference screenshot (2016) |

## v6 architecture (current)

- **MCU:** STM32F103RB (Cortex-M3), programmed via SWD (Nucleo-F103RB used as dev board/programmer)
- **Bluetooth:** Microchip RN52 (Bluetooth 3.0 Classic, A2DP/AVRCP)
- **CAN:** SAAB I-Bus at 47.619 kbps
- **Enclosure:** BUD Industries CU-3242

## Known issues / status

- The **RN52 is EOL** (Microchip, not recommended for new designs) and hard to source.
  It still works fine with modern phones (A2DP/SBC over Bluetooth Classic), but a new
  hardware revision needs a different radio.
- Editable sources (Eagle) exist only for v5.0; v6 is PDF + BOM only. If the v6 CAD
  files can be recovered, add them here.

## Interim mod: inline power switch

Until the sleep firmware exists (see TODO.md, 6.1.7), the unit draws ~25 mA
continuously — enough to trouble a battery in weeks of parking, hence the
unplug-when-unused ritual. A cleaner interim fix: splice a small automotive
toggle switch into the **12 V wire (connector pin 6)** near the plug.

- Switching only 12 V is electrically clean: an unpowered CAN transceiver is
  high-impedance on the bus, the silent audio stage is harmless, and the car
  simply sees "no changer" — identical to unplugging.
- Any small switch works (~250 mA peak load). No enclosure or board changes.
- Caveat: manual = forgettable in both directions (drain anyway / "why is
  Bluetooth dead?"). The 6.1.7 sleep firmware (~6 mA parked) and the
  optional LDO mod (~0.2 mA) make this switch obsolete by design.

## Planned successor direction

Replace both the STM32 and the RN52 with a single **ESP32** (the original ESP32 —
not S3/C3, which lack Bluetooth Classic):

- A2DP sink + AVRCP supported in the free ESP-IDF
- Built-in CAN (TWAI) controller — only an external transceiver needed
  (e.g. SN65HVD230/TJA1051), and TWAI supports the I-Bus 47.619 kbps rate
- Collapses the board to one module + transceiver + power + connector

See the repo root [TODO.md](../TODO.md) for the roadmap.
