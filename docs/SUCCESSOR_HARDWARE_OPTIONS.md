# BlueSaab successor — Hardware options

The new device is a **spiritual successor to BlueSaab v6**: a fresh, fully
open design that does the same job in the same cars, not an incremental v7 of
the STM32+RN52 board. Working name TBD.

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

## Option A — ESP32 (recommended)

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

Open decisions:

- [ ] Project name (it's a spiritual successor, not "BlueSaab v7" — or is it?)
- [ ] Multi-device swap UX: cycle on preset 3 vs. device slots on presets 2/4/5
      vs. both; SID name display; whether an idle unit auto-accepts any known
      phone (true multipoint is likely out — ESP32 handles one A2DP stream)
- [ ] SBC-only (simple, universal) vs. chasing AAC via ESP-ADF
- [ ] Reuse the v6 enclosure/connector footprint or shrink the board
- [ ] Wi-Fi config/OTA in scope for the first release or later
- [ ] PSRAM (WROVER) needed for audio buffering, or WROOM enough
