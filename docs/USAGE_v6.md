# BlueSaab v6 — Usage & Commands

Applies to hardware v6.x (STM32F103 + RN52) with firmware 6.1.1 and later;
features marked "6.1.x+" need that firmware version or later.

## Quick start: pairing a new phone

1. **Select the CD changer source** on the head unit. On head units with a
   built-in CD player, press **CD** twice (first press = internal CD, second =
   external changer). The display switches to the changer view and BlueSaab
   wakes up. At this point BlueSaab automatically tries to reconnect the *last*
   paired phone.
2. If a previously paired phone is in the car and grabs the connection, press
   **preset 6** to disconnect it first.
3. Press **preset 1** (or, 6.1.3+, hold the middle SEEK button for more than
   2 s). BlueSaab becomes discoverable and shows up as **"BlueSaab v6"** in the
   phone's Bluetooth settings. If the phone doesn't see it, press preset 1
   again.
4. Pair from the phone. Audio routes to the car.

On connect, the unit sends one play/pause toggle to the phone automatically.
If the phone was already playing, that toggle can pause it — press **NXT**.

Pairing a new phone is done with preset 1 or the >2 s middle-SEEK hold. The
RN52 is also configured "discoverable on start up" (`S%,1084`, bit 2), so it
may be visible briefly right after power-up — don't rely on that. Other long
SEEK presses do nothing (the v5 long-press map is gone).

## Head unit / steering wheel commands

Buttons reach BlueSaab only while the CD changer source is active. NXT, track
± and presets 4/5 act only while a phone is connected.

| Button | Action |
| --- | --- |
| **NXT** (steering wheel) | Play / pause |
| **Seek up / Track +** | Next track |
| **Seek down / Track −** | Previous track |
| **Preset 1** | Discoverable mode — pair a new phone |
| **Preset 3** | Reconnect to last paired phone |
| **Preset 4** (6.1.3+) | Volume down — AVRCP command to the phone (effect depends on the phone; unverified) |
| **Preset 5** (6.1.3+) | Volume up — AVRCP command to the phone |
| **Preset 6** | Disconnect current phone |
| **Extra-long middle SEEK, >2 s** (6.1.3+) | Discoverable mode (wheel-side pairing) |
| Switching away from CD changer source | Disconnects Bluetooth |
| Switching to CD changer source | Connectable + auto-reconnect last phone |

With SID text enabled, 6.1.3+ also flashes **"PAIRING"** when discoverable
mode is triggered and **"CONNECTED"** when a phone attaches.

Decoded by the firmware but currently **unassigned**: long press SEEK+/SEEK−,
long press of middle SEEK (<2 s), RANDOM (long press CD/RDM), pause on/off,
preset 2.

## Differences from older firmware (v5.x / early v6)

Older documentation floating around describes a different button map. If your
notes mention any of these, they predate 6.1.1:

| Old behavior | Status now |
| --- | --- |
| Long middle-SEEK (one beep) → discoverable | Removed in 6.1.1; **back in 6.1.3** as the extra-long (>2 s) press |
| Longer middle-SEEK hold (second beep) → connectable | Removed — automatic on entering CDC mode |
| Preset 1 → volume up | Preset 1 is now **discoverable/pairing**; volume moved to presets 4/5 (6.1.3+) |
| Preset 4 → volume down | Back in **6.1.3** |
| Preset 2 → max volume (`SS,0F`) | Not wired — max gain is applied automatically at every RN52 boot |
| Long CD/RDM → volume gain step | Not wired |

Since 6.1.3 the RN52 volume commands (`AV-`/`AV+`) are on presets 4/5.
Microchip documents them as AVRCP volume commands *to the phone*, not as the
RN52's local output gain, so what they do depends on the phone — not yet
verified in a car. The RN52's local gain is set to max (`SS,0F`) at every
boot. Normal volume control stays with the head unit's own volume knob.

Note: with SID text enabled, the SID row 2 shows scrolling **artist – title**
track metadata when the phone provides it (fetched on every track change);
"BlueSaab v6" is only the fallback text.

From firmware **6.1.2**: entering CDC mode first shows the version banner —
e.g. `6.1.7 R1.16` (BlueSaab firmware + RN52 module firmware) — for a few
seconds. `R?` means the RN52 hasn't reported a version: either it didn't
answer, or CD mode was entered within ~6 s of power-up, before the version
query ran. This makes any unit self-identifying without a serial console.

## Debug serial console

For bench testing and troubleshooting without the car.

- **Port:** the 2-pin **UART2** header, **115200 8N1, 3.3 V logic only**
  (PA2/PA3 are not 5 V tolerant). Pin 1 = board TX (PA2) → adapter RXD;
  pin 2 = board RX (PA3) ← adapter TXD.
- **The UART2 header has no GND pin.** Connect the adapter's GND to FTDI
  header pin 1 (black), EXT VCC header pin 2, or SWD pin 3/5/9.
- **Power** the board from the MicroUSB jack (5 V via diode D2), the FTDI
  header (5 V on pin 3), or 12 V on CDC connector pin 6 (GND pin 12).
- Log output prints the firmware/hardware version at boot; 6.1.2+ also logs
  `RN52 version: x.xx` about 6 s after power-up.

Single-character commands (no Enter needed):

| Key | Action |
| --- | --- |
| `V` | Discoverable mode (same as preset 1) |
| `I` | Connectable / non-discoverable mode |
| `C` | Reconnect to last paired phone (same as preset 3) |
| `D` | Disconnect current phone (same as preset 6) |
| `P` | Play / pause |
| `N` | Next track |
| `R` | Previous track |
| `A` | Invoke phone voice assistant (Siri etc.) |
| `B` | Reboot the RN52 module |
| `d` | Print the RN52's Bluetooth address (`BTA=` line) — proves the STM32↔RN52 link. It does **not** show the RN52 firmware version |
| `u` | Reset the RN52 paired device list — **forgets all phones**, including the owner's |
| `E` | CAN health: TX failures/drops, RX overruns, live REC/TEC error counters, ESR (bus-off/error-passive flags) |
| `H` | Help — list commands |

`E` on the bench (no CAN bus connected) is *expected* to look bad: TX write
failures climbing, TEC around 128 and ESR bit 1 (error-passive) set, because
nothing acknowledges our frames. Bus-off (ESR bit 2) should stay clear and RX
overruns at 0. In the car, all counters should stay at or near 0.

### Bench validation recipe

1. Power the board and open the serial console at 115200. Expect
   `BlueSaab` / `Hardware version: 6.1` / `Firmware version: …`, and on
   6.1.2+ `RN52 version: x.xx` about 6 s later.
2. Send `d` — a `BTA=…` line (the module's Bluetooth address) confirms the
   STM32↔RN52 link is alive.
3. Send `I`, check that "BlueSaab v6" drops off the phone's Bluetooth scan
   list, then send `V` — it should reappear within a few seconds. If it does,
   the Bluetooth side is healthy and any in-car problem is on the CAN/I-Bus
   side; if not, the RN52 module or its serial link is the problem.

## Troubleshooting

- **New phone can't find BlueSaab:** you must press preset 1 while in the CD
  changer source; also press preset 6 first if an old phone auto-reconnected.
- **Pairing list full / phone misbehaves after re-pairing:** send `u` over
  serial to wipe the paired device list (all phones), then pair fresh.
- **No audio but connected:** press NXT (play/pause) — some phones wait for an
  AVRCP play command before streaming.
- **SID always shows static "BlueSaab v6", never track names:** three causes —
  1. BlueSaab firmware predates Aug 2018 (metadata scrolling didn't exist):
     any boot banner `Firmware version: 6.1.1` or later means the metadata
     code is present. Without a console, the version string can be read from
     a flash backup: `strings backup_v6_unit.bin | grep -i "firmware version"`;
  2. RN52 module firmware < 1.16 (the `AD` track-data command was added in
     1.16): on 6.1.2+ read the `RN52 version:` boot line or the `R<ver>` part
     of the SID banner. Firmware 6.1.1 and older cannot report it — flash
     6.1.2+ (flashing the STM32 does not touch the RN52's own firmware);
  3. the RN52 GPIO2 event chain isn't firing (no track-change polls): with
     music playing, change tracks and watch for `Q`/`AD` activity.
  With old RN52 firmware, static text is expected — everything else works.
