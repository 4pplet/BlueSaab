# Successor board — parts list (schematic prep)

Working parts list for the ESP32 successor, organized by functional block.
The **v6 reference** column points at the proven design to copy from:
designators refer to `HARDWARE/BlueSaab_v6.PDF` sheets and
`HARDWARE/BOM_PartType-BlueSaab_v6_1_1.csv`. Blocks marked **carry over**
should be copied from the v6 schematic as-is (same topology and values).

## 1. Main controller (new)

| Part | Package | Function | v6 reference |
| --- | --- | --- | --- |
| ESP32-WROOM-32E (4 MB) | SMD module | MCU + BT Classic + TWAI CAN | new — replaces U1 (STM32F103RBT7) *and* U2 (RN52) |
| 10 µF + 0.1 µF on 3V3 | 0805 | module decoupling | same values as v6 C4-C7/C14-C17 pattern |
| 10 k pull-up + 1 µF to GND on EN | 0805 | power-on reset RC | new (standard ESP32 app circuit) |
| 2× tactile switch (EN / IO0) | SMD gull wing | reset / boot strap | reuse part: KMR231GLFS ("BOOT0, RESET" in v6 BOM) |

Notes: PCB-antenna module — keep antenna keep-out at board edge, no copper
under it. WROVER (PSRAM) only if audio buffering demands it (open decision).

## 2. CAN front end (partly carried over — v6 "CANBUS" sheet)

| Part | Package | Function | v6 reference |
| --- | --- | --- | --- |
| TCAN3414 (or similar ISO 11898-2:2016 part with remote wake) | SOIC-8 | 3.3 V CAN transceiver, ~10 µA standby with bus wake-up | **new — replaces U6** (SN65HVD234: listening standby 200–600 µA, sleep mode deaf) |
| SZNUP2105LT3G | SOT-23 | CAN bus ESD/transient protector | **D4** |
| 68 k across CANH/CANL | 0805 | bus bias/keeper (I-Bus has no 120 Ω termination) | **R36** |
| standby-pin drive | — | STB to an ESP32 GPIO (v6's R25/RS arrangement doesn't apply to the new part) | replaces **R25** |
| 10 µF + 0.1 µF | 0805 | transceiver decoupling | C9 + C19 |

Notes: the transceiver's standby pin goes to an ESP32 GPIO. RXD additionally
routes to an RTC-capable GPIO for deep-sleep wake (the transceiver pulls RXD
low on a bus wake-up pattern). The ESD protector and 68 k bias carry over from
v6 unchanged; check the new transceiver's pinout — it is not an HVD234
drop-in.

## 3. Audio DAC (new)

| Part | Package | Function | v6 reference |
| --- | --- | --- | --- |
| PCM5102APWR | TSSOP-20 | I2S DAC, 2.1 Vrms line out, integrated charge pump | new — replaces the RN52's internal DAC |
| 2× 2.2 µF (CPVDD flying/neg rail) | 0805 | charge-pump caps | new (datasheet app circuit) |
| 10 µF + 0.1 µF per supply pin | 0805 | decoupling | v6 pattern |
| strap resistors (FMT, XSMT, FLT, DEMP) | 0805 | mode straps per datasheet | new |

Notes: output is single-ended, **ground-centered** (swings below 0 V thanks
to the charge pump) — AC-couple into the line driver (see block 4).

## 4. Differential line driver (carry over — v6 "Amplifier" sheet)

| Part | Package | Function | v6 reference |
| --- | --- | --- | --- |
| THS4522IPWR | TSSOP-16 | dual fully-differential amp driving the head unit's balanced CDC input | **U4 — copy topology** |
| 4× 1 k input resistors | 0805 | gain-set (Rg) | R13-R16 |
| 4× 2 k feedback resistors | 0805 | gain-set (Rf), gain ≈ 2 | R21-R24 |
| 4× 0.22 µF | 0805 | filter caps (as in v6) | C27-C30 |
| 4× 100 Ω series output | 0805 | output isolation to connector | R6-R9 |
| 10 µF + 0.1 µF ×2 | 0805 | amp decoupling | C10/C20, C11/C21 |

Adaptation notes (the one carry-over block that needs thought):

- v6 fed it differentially from the RN52; the PCM5102A is single-ended —
  drive Rg from the DAC output, reference the complementary input to audio
  ground, **AC-couple** both (PCM5102A output is ground-centered, amp runs
  on single 3.3 V).
- Check the VOCM pin handling on the v6 sheet and keep it mid-supply.

## 5. Power (new design — v6 "Power" sheet is NOT reusable)

| Part | Package | Function | v6 reference |
| --- | --- | --- | --- |
| LMR36015 (4.2–60 V in, 66 V transient, 1.5 A, ~24 µA Iq) or similar 60 V-class buck | — | low-Iq 3.3 V buck from 12 V | new — replaces U3 (LM1117, linear: inadequate for ESP32). **Not** AP63203 (35 V abs max) or TPS54202 (30 V): below the TVS clamp and a suppressed load dump (up to 35–42 V) |
| 4.7–10 µH shielded inductor | SMD | buck inductor | new |
| 22 µF ×2 out, 10 µF in (50 V) | 0805/1210 | buck caps | new |
| SS34 or similar 3 A Schottky | SMA | reverse-polarity series diode | upgrade of D1 (1N4148W — too small for successor) |
| SMBJ33A TVS | SMB | transient clamp on 12 V input (breakdown 36.7 V, clamp ≤ 53.3 V — only coherent with a ≥ 55–60 V regulator) | **new — v6 never had one; add it**. An SMB TVS cannot absorb an *unsuppressed* load dump (ISO 16750-2 Test A: 79–101 V for up to 400 ms); whether 1998–2005 SAABs suppress load dump centrally is unverified — decide on a surge stopper or document the assumption |
| Polyfuse ~500 mA hold | 1812 | input overcurrent | new |
| Load switch (TPS22918 or high-side P-FET) | SOT-23 | gates DAC + line driver + LEDs off in sleep | new (sleep-by-construction) |

## 6. Connectors & misc (mostly carry over — v6 "Connectors" sheet)

| Part | Package | Function | v6 reference |
| --- | --- | --- | --- |
| TE 827229-1, 24-pin | THT | CDC connector — 12 V (6), GND (12), CAN H/L (11/5), R± (7/2), L± (8/3) | **P1 — keep part and pinout exactly** (drop-in install) |
| UART debug header | 2.54 mm | ESP32 UART0 console/flash | successor of FTDI/UART2 headers |
| RGB or 2× status LED + 10 k drivers | SMD | status/heartbeat | LED1/POWER + R28-R31 pattern |
| BUD CU-3242 enclosure | — | same box as v6 | BOX1 |
| Fiducials ×3 | — | pick-and-place | FID1-3 |

Open (deliberately not listed yet): USB for flashing (bare header vs on-board
USB-UART bridge — the original ESP32 has no native USB), mic/HFP block
(deferred; v6 "Mic Circuit" sheet is the reference if revived).

## Deleted from v6 (no successor equivalent)

- U1 STM32F103, Y1 8 MHz resonator, JTAG header, BOOT0 straps → ESP32 module
- U2 RN52 and all its support circuitry → ESP32 + PCM5102A
- U3 LM1117 linear regulator → buck
- MicroUSB power-only jack J1, power OR-ing diodes D2/D3 → single 12 V path
  (debug power via UART header)

## Prototype (pre-PCB) shopping list

- ESP32 devkit (WROOM-based, e.g. ESP32-DevKitC) — or ESP32-A1S audio kit
- PCM5102A breakout (purple "GY-PCM5102" boards are fine)
- SN65HVD230 breakout (the jellybean breakout cousin of the 234 — fine for
  the bench; the final board needs a wake-capable transceiver, see block 2)
- Bench 12 V supply + the CDC connector pigtail / v6 test harness
