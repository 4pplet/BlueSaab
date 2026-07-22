# BlueSaab successor — Hardware options

The new device is a **spiritual successor to BlueSaab v6**: a fresh, fully
open design that does the same job in the same cars, not an incremental v7 of
the STM32+RN52 board. Working name TBD.

> **DECISION (2026-07-22): Option A — single ESP32.** One chip for Bluetooth
> audio, CAN, and application logic. Options B/C kept below for the record.

What the successor must do (same job as v6, minus the dead parts):

- **Familiar in-car interface, v6 baseline.** Existing users must not have to
  relearn anything: the v6.1.1 button map ([USAGE_v6.md](USAGE_v6.md)) works
  unchanged — preset 1 = pair, preset 3 = reconnect, preset 6 = disconnect,
  NXT = play/pause, track ± = next/prev, same SID text behavior. Improvements
  are allowed but must be *additive* (new behavior on currently-unused
  buttons), never a change to what an existing button does.
- **Modern multi-device handling** — the headline improvement over v6. The
  RN52 could only blindly "reconnect last"; the successor should hold several
  paired phones and swap between them from the driver's seat. Sketch:
  preset 3 = reconnect last (as v6), *repeated* presses cycle through the
  paired-device list, with the SID showing the device name being connected
  ("BT: STEFAN'S IPHONE"). Presets 2/4/5 are free if dedicated device slots
  turn out nicer. Exact UX is an open decision below.
- **Bluetooth Classic A2DP sink + AVRCP** — this is what phones stream music
  over. Still universally supported by iPhone/Android; LE Audio is not a safe
  bet yet (see rejected options).
- **CAN @ 47.619 kbps** for the SAAB I-Bus (nonstandard rate — must verify each
  controller can hit it exactly).
- **Stereo line-level analog out** into the head unit (the RN52 had this built
  in; most replacements need an external DAC).
- **12 V automotive power**, ignition-switched.
- **Open toolchain** — the whole point of the revival. NDA'd SDKs disqualify.

## Option A — ESP32 (CHOSEN)

One original-series ESP32 module replaces **both** the STM32 and the RN52.

| Aspect | Detail |
| --- | --- |
| Bluetooth | BT 4.2 dual-mode; A2DP sink + AVRCP controller in free ESP-IDF (SBC codec; AAC possible via ESP-ADF) |
| CAN | Built-in TWAI controller. 47.619 kbps verified feasible: 80 MHz / (BRP 112 × 15 TQ) = 47 619 bps exactly. Needs only a transceiver (TJA1051 or SN65HVD230) |
| Audio out | External I2S DAC — PCM5102A (~$2) gives clean line-level stereo; skip the awful internal 8-bit DAC |
| Extras | Wi-Fi: OTA firmware updates, web-based config/debug console instead of UART |
| Toolchain | ESP-IDF, fully open, huge community |
| Cost | ~$5 module + ~$4 support parts |
| Module | ESP32-WROOM-32E (or WROVER-E if PSRAM wanted for audio buffering) |

**Must be the original ESP32** — S3/C3/C6/P4 all dropped Bluetooth Classic.

Risks: the ESP32 is a 2016 design and Espressif's Classic-BT stack is
effectively in maintenance mode; the chip is still in volume production with
no EOL announced, and its longevity-commitment runs to 2031+. Single-source,
like any SoC choice. A2DP sink quality in ESP-IDF is proven by many
open-source car/speaker projects.

Prototype shortcut: the **ESP32-A1S audio dev board** (ESP32 + ES8388 codec)
gets Bluetooth→line-out running in a day; add a CAN transceiver on jumper
wires and the whole successor concept is testable before any PCB exists.

## Option B — Microchip BM83 + small CAN MCU

The RN52's official successor (IS2083 based): BT 5.0 audio module with SBC+AAC
and built-in analog line out — audio-wise the nicest drop-in.

- Needs a host MCU for the I-Bus anyway (e.g. STM32G0B1 with FDCAN, ~$2), so
  it's a two-chip board like v6.
- **Config tooling is closed** (Windows GUI, firmware blobs) — contributors
  can't fully reproduce the build. Repeats the RN52 story: module discontinued
  → project stranded.
- Choose only if A2DP-AAC audio quality is judged worth the closed tooling.

## Option C — Raspberry Pi Zero 2 W (Linux route)

BlueZ/PipeWire A2DP sink (SBC/AAC/aptX possible), CAN via SPI MCP2515/MCP2518FD,
infinitely hackable, all open.

- **Boot time kills it for daily driving**: ~15 s stock (maybe ~5 s with a
  stripped Buildroot image) vs. effectively instant for an MCU. The car UX is
  "turn key, press play".
- Also needs safe-shutdown supervision (SD corruption on ignition-off) and has
  higher idle draw.
- Verdict: great for a bench/dev mule, wrong for the product.

## Rejected

- **Qualcomm QCC30xx/51xx** — best codecs (aptX HD), but SDK is NDA-only.
  Incompatible with an OSS project.
- **LE Audio (nRF5340 Audio etc.)** — phones' LE Audio *source* support for
  general A2DP-replacement streaming is still patchy (especially iPhone) as of
  2026. Revisit for a future generation.
- **STM32-only** — no STM32 has Bluetooth Classic (WB series is BLE-only), so
  a radio module is needed regardless; that module choice is the real decision.
- **Jieli/KCX cheap BT-audio chips** — no docs, no toolchain, no.

## Recommended successor architecture (Option A)

```text
        12V (CDC connector)
             │
     [automotive buck 12V→3.3V
      + TVS load-dump protection]
             │
  ┌──────────┴───────────┐
  │      ESP32-WROOM     │
  │  BT Classic A2DP/AVRCP│──── antenna (module PCB antenna)
  │  TWAI ──────────────┐│
  │  I2S  ───────┐      ││
  └──────────────┼──────┼┘
                 │      │
          [PCM5102A]  [TJA1051]
            DAC        CAN xcvr
                 │      │
           line out    I-Bus (47.619 kbps)
           to head unit
```

Firmware plan: port `SaabCan`/`CDCStatus`/`Buttons`/`SidResource` protocol
logic (platform-independent) onto ESP-IDF + TWAI; replace the whole
`common/RN52*` layer with ESP-IDF's A2DP sink + AVRCP APIs. The in-car
behavior must be indistinguishable from v6.1.1 (see requirement above) —
[USAGE_v6.md](USAGE_v6.md) doubles as the test checklist. PCB in KiCad so the
hardware is as open as the code.

## What carries over from the v6 schematic

Sheet-by-sheet review of `HARDWARE/BlueSaab_v6.PDF` (8 schematic sheets):

| v6 sheet | What it is | Successor |
| --- | --- | --- |
| Amplifier | THS4522 differential line driver, gain ≈2 (2k/1k), 0.22 µF filtering, 100 Ω series outputs | **Reuse as-is.** The head unit's CDC audio input is *differential* (L±/R± on the connector) — the PCM5102A is single-ended, so this stage is still required. Proven part, still in production |
| CANBUS | SN65HVD234 transceiver + NUP2105L bus ESD protector + 68 k bias resistor | **Reuse as-is** (drives from ESP32 TWAI instead of STM32 bxCAN) |
| Connectors | TE 827229-1 24-pin CDC connector — 12 V (pin 6), GND (12), CAN H/L (11/5), R± (7/2), L± (8/3); reverse-polarity diode; FTDI + USB power OR-ing | **Keep connector + pinout** — this is what makes the successor a drop-in install. Debug connectors can modernize |
| Mic | Header + bias + slide switch feeding the RN52's mic inputs (hands-free option) | **Open decision.** ESP32 can do HFP but needs an I2S mic/ADC path; defer to post-v1 unless demanded |
| Power | LM1117 3.3 V **linear** regulator straight from car 12 V | **Must be redesigned.** Fine for ~100 mA of STM32+RN52; the ESP32's ~500 mA radio bursts would dissipate >4 W linearly. Use an automotive buck (TPS54202/AP63203-class) + load-dump TVS (which v6 never had — only a series diode) |
| Microprocessor | STM32F103, 8 MHz resonator, JTAG, BOOT0/RESET buttons | Replaced by the ESP32 module |
| RN52 | Module wiring, status LEDs (RGB driven by RN52 + heartbeat) | Replaced by ESP32; keep equivalent status LEDs |

Net effect: the successor board is the v6 **audio output stage and CAN front
end unchanged**, wrapped around one ESP32 + I2S DAC + buck converter instead
of two chips and a linear regulator.

## Power management (parked-car current draw)

The CDC connector's 12 V is **battery-fed, not ignition-switched** — the
device is always powered. v6 idles the LM1117 + STM32 + RN52 continuously
(order of 15–30 mA, ~0.5 Ah/day), acceptable for a daily driver but hard on a
car that sits for weeks. An ESP32 doing nothing would be worse (40–80 mA), so
sleep handling is a **hard requirement**, not an optimization:

- **ESP32 deep sleep** (~10 µA) whenever the I-Bus has been silent for a few
  minutes. The SAAB buses go quiet when the car is locked, so bus silence ==
  car asleep.
- **Wake on bus activity**: TWAI doesn't run in deep sleep, but the CAN
  transceiver's RXD idles high (recessive) and any frame produces a falling
  edge — route RXD to an EXT0/EXT1 wakeup GPIO. First frames after unlock wake
  the chip; full boot + BT stack is ~1–2 s, well inside the time it takes the
  driver to reach the CD button. (The first wake-up frame is missed —
  irrelevant, the IHU polls node status continuously.)
- **CAN transceiver sleep**: the SN65HVD234's RS/EN pins (already wired to the
  MCU in v6) give it a sub-µA listen/sleep mode that still passes RXD edges.
- **Low-quiescent buck**: pick for Iq, e.g. AP63203 (~22 µA) — a lesser buck's
  quiescent current would dominate the whole sleep budget.

Sleep-state target: **< 100 µA total** from 12 V — years of parking, ~500×
better than v6. Firmware obligation: the main loop must track bus silence and
enter deep sleep; there is no ignition signal to lean on.

Open decisions:

- [ ] Project name (it's a spiritual successor, not "BlueSaab v7" — or is it?)
- [ ] Multi-device swap UX: cycle on preset 3 vs. device slots on presets 2/4/5
      vs. both; SID name display; whether an idle unit auto-accepts any known
      phone (true multipoint is likely out — ESP32 handles one A2DP stream)
- [ ] Hands-free/mic support (v6 had an optional mic header; needs ESP32 HFP
      plus an I2S mic path — defer to post-v1?)
- [ ] SBC-only (simple, universal) vs. chasing AAC via ESP-ADF
- [ ] Reuse the v6 enclosure/connector footprint or shrink the board
- [ ] Wi-Fi config/OTA in scope for the first release or later
- [ ] PSRAM (WROVER) needed for audio buffering, or WROOM enough
