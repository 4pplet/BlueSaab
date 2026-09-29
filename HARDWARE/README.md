# BlueSaab Hardware

Board design files for the BlueSaab CD changer emulator — a device that plugs into
the CD changer connector of older SAAB cars (9-3/9-5 with I-Bus) and streams
Bluetooth audio via a Microchip RN52 module.

## Contents

| File | Description |
| --- | --- |
| `BlueSaab_v6.PDF` | Schematic for hardware v6 (targeted by all 6.1.x firmware) |
| `BOM_PartType-BlueSaab_v6_1_1.csv` | Bill of materials for v6.1.1 |
| `BlueSaab v5.0/` | Full v5.0 design: Eagle `.sch`/`.brd` sources plus PDF exports ("RN52 v5.0 + 10pin"); the `.zip` inside holds the v5.0 **Gerbers** |
| `Hardware - Installing BlueSAAB module in cars without CDC provisioning cable.pdf` | Install guide for cars lacking the CDC pre-wiring |
| `Screen_Shot_2016-10-26_at_09.png` | Reference screenshot (2016) |

## v6 architecture (current)

- **MCU:** STM32F103RB (Cortex-M3), programmed via the ROM serial bootloader
  on the FTDI header (primary) or SWD — see
  [docs/FLASHING_v6_HOWTO.md](../docs/FLASHING_v6_HOWTO.md)
- **Bluetooth:** Microchip RN52 (Bluetooth 3.0 Classic, A2DP/AVRCP)
- **CAN:** SAAB I-Bus at 47.619 kbps, SN65HVD234 transceiver
- **Power:** LM1117-3.3 linear regulator from the CDC connector's 12 V
  (battery-fed, not ignition-switched)
- **Enclosure:** BUD Industries CU-3242

## Headers and connectors (from the schematic)

| Header | Pins | Use |
| --- | --- | --- |
| CDC connector P1 (TE 827229-1) | 6 = 12 V, 12 = GND, 11/5 = CAN H/L, 7/2 = R±, 8/3 = L± | The car |
| FTDI (6-pin) | 1 = GND (black), 3 = 5 V in (red), 4 = USART1_RX (orange), 5 = USART1_TX (yellow) | Flashing (ROM bootloader) |
| UART2 (2-pin) | 1 = TX (PA2), 2 = RX (PA3) — **no GND** | Debug console, 115200 8N1, 3.3 V only |
| UART3 (2-pin) | 1 → RN52 RX, 2 ← RN52 TX — **no GND** | Direct RN52 UART (DFU) |
| EXT VCC (2-pin) | 1 = 3.3 V, 2 = GND | Handy ground for the UART headers |
| SWD (10-pin 1.27 mm) | 1 = VCC sense, 2 = SWDIO, 4 = SWCLK, 3/5/9 = GND, 10 = RESET | ST-Link |
| MicroUSB J1 | 5 V in via diode D2; D+/D- to PA11/PA12 (unused by firmware) | Bench power only |

## Known issues / status

- The **RN52 is end-of-life**: Microchip PCN MFOL-01KSIK286 (April 2024),
  last shipment June 2024, recommended replacement BM83. Only broker stock
  remains. It still works fine with modern phones (A2DP/SBC over Bluetooth
  Classic), but no new v6 boards can be built — hence the ESP32 successor.
- Editable sources (Eagle) exist only for v5.0; v6 is PDF + BOM only. If the v6 CAD
  files can be recovered, add them here.
- Parked current draw has never been measured (estimated 15–30 mA). The
  LM1117 alone draws ~5–10 mA quiescent.

## Interim mod: inline power switch

Until the sleep firmware exists (planned for 6.2.0 — see TODO.md), the unit
draws power continuously — enough to trouble a battery over weeks of
parking, hence the unplug-when-unused ritual. A cleaner interim fix: splice a
small automotive toggle switch into the **12 V wire (connector pin 6)** near
the plug.

- Switching only 12 V is electrically clean: an unpowered CAN transceiver is
  high-impedance on the bus, the silent audio stage is harmless, and the car
  simply sees "no changer" — identical to unplugging.
- Any small switch rated for ≥ 1 A at 12 V works (the unit's peak draw is
  unmeasured but far below that). No enclosure or board changes.
- Flip it on with the ignition on and the unit boots onto a live bus. Use
  firmware 6.1.7 or later: 6.1.6 could occasionally hang at such a boot, and
  6.1.1 is fine.
- Caveat: manual = forgettable in both directions (drain anyway / "why is
  Bluetooth dead?"). The planned sleep firmware makes it largely obsolete.

## Low-Iq regulator mod — read before soldering

A lower-quiescent regulator only pays off together with sleep firmware, and
it is **not a drop-in swap**. See the "Optional hardware mod" section in
[TODO.md](../TODO.md). In short:

- Low-Iq automotive LDOs like the MCP1792 use a VIN–GND–VOUT SOT-223 pinout
  with the tab on GND, while the LM1117 is GND–VOUT–VIN with the tab on VOUT.
  Soldered onto the LM1117 footprint, the MCP1792 gets 12 V on its output
  and is destroyed.
- The MCP1792 is also a 100 mA part — measure the peak current first.

## Planned successor direction

Replace both the STM32 and the RN52 with a single **ESP32** (the original ESP32 —
not S3/C3, which lack Bluetooth Classic):

- A2DP sink + AVRCP supported in the free ESP-IDF
- Built-in CAN (TWAI) controller — only an external transceiver needed, and
  TWAI hits the I-Bus 47.619 kbps rate exactly
- Collapses the board to one module + transceiver + power + connector

See [docs/SUCCESSOR_HARDWARE_OPTIONS.md](../docs/SUCCESSOR_HARDWARE_OPTIONS.md)
and the repo root [TODO.md](../TODO.md).
