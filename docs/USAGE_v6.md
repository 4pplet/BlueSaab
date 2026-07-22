# BlueSaab v6 — Usage & Commands

Applies to hardware v6.x with firmware 6.1.1 (STM32F103 + RN52).

## Quick start: pairing a new phone

1. **Select the CD changer source** on the head unit. On head units with a
   built-in CD player, press **CD** twice (first press = internal CD, second =
   external changer). The display switches to the changer view and BlueSaab
   wakes up. At this point BlueSaab automatically tries to reconnect the *last*
   paired phone — it is not yet visible to new phones.
2. If a previously paired phone is in the car and grabs the connection, press
   **preset 6** to disconnect it first.
3. Press **preset 1**. BlueSaab becomes discoverable and shows up as
   **"BlueSaab"** in the phone's Bluetooth settings.
4. Pair from the phone. Audio routes to the car; playback starts with
   play/pause (**NXT** on the wheel).

Discoverability is not automatic and never times in on its own — pressing
preset 1 is always required to pair a new device. Long-pressing SEEK does
**nothing** on v6 firmware (that was v5 behavior).

## Head unit / steering wheel commands

Buttons reach BlueSaab only while the CD changer source is active.

| Button | Action |
| --- | --- |
| **NXT** (steering wheel) | Play / pause |
| **Seek up / Track +** | Next track |
| **Seek down / Track −** | Previous track |
| **Preset 1** | Discoverable mode — pair a new phone |
| **Preset 3** | Reconnect to last paired phone |
| **Preset 6** | Disconnect current phone |
| Switching away from CD changer source | Disconnects Bluetooth |
| Switching to CD changer source | Connectable + auto-reconnect last phone |

Decoded by the firmware but currently **unassigned** (candidates for future
features): long press SEEK+/SEEK−, long/extra-long press of middle SEEK,
RANDOM (long press CD/RDM), pause on/off, presets 2, 4, 5.

## Differences from older firmware (v5.x / early v6)

Older documentation floating around describes a different button map. If your
notes mention any of these, they predate 6.1.1:

| Old behavior | Status in 6.1.1 |
| --- | --- |
| Long middle-SEEK (one beep) → discoverable | Removed — use **preset 1** |
| Longer middle-SEEK hold (second beep) → connectable | Removed — automatic on entering CDC mode |
| Preset 1 → volume up | Preset 1 is now **discoverable/pairing** |
| Preset 4 → volume down | Not wired |
| Preset 2 → max volume (`SS,0F`) | Not wired |
| Long CD/RDM → volume gain step | Not wired |

The RN52 volume-step commands (`AV+`, `AV-`) still exist in the driver but
are not connected to any button — volume is controlled from the phone or the
head unit's own volume knob. (Max gain `SS,0F` *is* applied automatically at
boot, so no button for it is needed.)

Note: with SID text enabled, the SID row 2 shows scrolling **artist – title**
track metadata when the phone provides it (fetched on every track change);
"BlueSaab v6" is only the fallback text.

## Debug serial console

For bench testing and troubleshooting without the car.

- **Port:** UART2 — pins PA2 (TX) / PA3 (RX) on the STM32, **115200 8N1**
- Log output prints firmware/hardware version at boot

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
| `d` | Print RN52 details (firmware version, config) |
| `u` | Reset the RN52 paired device list (PDL) — forgets all phones |
| `H` | Help — list commands |

### Bench validation recipe

1. Power the board, open the serial console at 115200.
2. Send `d` — confirms the STM32↔RN52 link is alive and shows the RN52
   firmware version.
3. Send `V` — the board should appear as "BlueSaab" on a phone within a few
   seconds. If it does, the Bluetooth side is healthy and any in-car problem
   is on the CAN/I-Bus side; if not, the RN52 module or its serial link is the
   problem.

## Troubleshooting

- **New phone can't find BlueSaab:** you must press preset 1 while in the CD
  changer source; also press preset 6 first if an old phone auto-reconnected.
- **Pairing list full / phone misbehaves after re-pairing:** send `u` over
  serial to wipe the paired device list, then pair fresh.
- **No audio but connected:** press NXT (play/pause) — some phones wait for an
  AVRCP play command before streaming.
- **SID always shows static "BlueSaab v6", never track names:** three causes,
  distinguishable over the serial console —
  1. BlueSaab firmware predates Aug 2018 (metadata scrolling didn't exist):
     check the boot banner, current source prints `Firmware version: 6.1.1`;
  2. RN52 module firmware < 1.16 (the `AD` track-data command was added in
     1.16): send `d` and read the reported RN52 version;
  3. the RN52 GPIO2 event chain isn't firing (no track-change polls): with
     music playing, change tracks and watch for `Q`/`AD` activity.
  With old RN52 firmware, static text is expected — everything else works.
