# SAAB I-Bus CDC protocol — as implemented by BlueSaab

This documents the CD-changer protocol on the SAAB I-Bus as actually
implemented and field-proven by BlueSaab v6.1.1, with 9-5 specifics from
community research. It exists because the canonical reference — Tomi
Liljemark's `pikkupossu.1g.fi` — is **dead**; see
[archived copy](http://web.archive.org/web/20250122150340/http://pikkupossu.1g.fi/tomi/projects/i-bus/i-bus.html).

Code references are to this repo; the successor firmware must reproduce this
behavior exactly.

## Physical layer

- CAN 2.0A, 11-bit identifiers, all frames 8 bytes
- **47.619 kbit/s** (21 µs bit time) — nonstandard; verify your controller
  can hit it exactly (ESP32 TWAI: 80 MHz / (BRP 112 × 15 TQ))
- 2-wire differential (ISO 11898-2-like): I+ 2.5→3.5 V, I− 2.5→1.5 V
- Applies to OG 9-3 (1998–2002, Cab –2003) and OG 9-5 (1998–2009/10)

## Frame overview

| ID | Dir | Name | Purpose |
| --- | --- | --- | --- |
| 0x3C8 | TX | `GENERAL_STATUS_CDC` | Periodic CDC status (disc/track/married) |
| 0x6A2 | TX | `NODE_STATUS_TX_CDC` | Node-status reply to IHU poll |
| 0x430 | TX | `SOUND_REQUEST` | Ask SID to beep (avoid — see quirks) |
| 0x357 | TX | `NODE_DISPLAY_RESOURCE_REQ` | Request write access to SID row 2 |
| 0x337 | TX | `NODE_WRITE_TEXT_ON_DISPLAY` | SID text payload (3-frame group) |
| 0x3C0 | RX | `CDC_CONTROL` | IHU commands + button events to CDC |
| 0x6A1 | RX | `NODE_STATUS_RX_IHU` | IHU node-status poll |
| 0x368 | RX | `DISPLAY_RESOURCE_GRANT` | SID grants display access |
| 0x348 | RX | `IHU_DISPLAY_RESOURCE_REQ` | IHU's own display requests (watched) |
| 0x290 | RX | `STEERING_WHEEL_BUTTONS` | Raw wheel/SID buttons (defined, unused — the IHU translates wheel presses into 0x3C0) |

## Timing rules (hard requirements)

- 0x6A2 replies: ≤ **140 ms** apart (±10%) — `NODE_STATUS_TX_INTERVAL`
- 0x3C8 status: every ≤ **950 ms** (±10%) — `CDC_STATUS_TX_BASETIME`;
  also resent immediately (~50 ms) on command-induced state change
- Display resource re-request: every ~1 s — `NODE_UPDATE_BASETIME`
- **Same-ID frames ≥ 10 ms apart**; never flood the bus (can reset MIU/SID
  and is suspected of triggering spurious warning lights)

## 0x6A1 → 0x6A2: the node-status handshake ("here be dragons")

The IHU polls with 0x6A1. Look at the **low nibble of byte 3** and reply
with the matching 4-frame sequence on 0x6A2, frames ≤140 ms apart
([CDCStatus.cpp](../CDCStatus.cpp)):

| 0x6A1 byte3 & 0x0F | Meaning | Reply sequence (byte 0 / byte 3 / bytes 4-5) |
| --- | --- | --- |
| 0x3 | power-on poll | `32 00 00 03 01 02`, `42/52/62 00 00 22 00 00` |
| 0x2 | active poll | `32 00 00 16 01 02`, `42/52/62 00 00 36 00 00` |
| 0x8 | power-down poll | `32 00 00 19 01 00`, `42/52/62 00 00 38 01 00` |

Notes:

- Byte 0 sequence is always `0x32, 0x42, 0x52, 0x62`.
- **The 9-5 IHU requires this handshake** (reply only when polled); the 9-3
  is satisfied by periodic 0x3C8 alone. Same firmware serves both.
- **Getting byte 0 wrong (e.g. 0x64 instead of 0x32) lit airbag + MIL
  warnings with a chime on a 2004 9-5.** Treat this table as frozen.

## 0x3C8: CDC general status

Sent periodically (≤950 ms) and on events. Layout
([CDCStatus.cpp](../CDCStatus.cpp) `sendCdcStatus`):

- byte 0: bit7 = event (vs base-time), bit6 = due to remote command,
  bit5 = disc-presence valid. v6 sends `(event?0x07:0) | (remote?0:0x01)) << 5`
- byte 1: disc presence validation — `0xFF` when active
- byte 2: disc presence bitmap — `0x3F` (6 discs) when active, `0x01` idle
- byte 3: high nibble disc mode, low nibble disc number — `0x41` active
- byte 4: track number (`0xFF` = n/a)
- bytes 5-6: track minute/second (`0xFF`)
- byte 7: **security byte — `0xD0` = "married to car, VIN matches"**. This
  is what bypasses the Tech2 VIN-marriage of a real changer.

## 0x3C0: CDC control (IHU → CDC)

Byte 0 = `0x80` marks a command/button event. Byte 1 (+byte 2 for presets)
([Buttons.cpp](../Buttons.cpp), [CDCStatus.cpp](../CDCStatus.cpp)):

| byte1 | Meaning | v6 action |
| --- | --- | --- |
| 0x24 | CDC mode selected | activate, sound-request, connect BT |
| 0x14 | CDC mode deselected | deactivate, disconnect BT |
| 0x59 | NXT (wheel) | play/pause |
| 0x35 / 0x36 | Track + / − | next / previous |
| 0x68 + byte2 0x01–0x06 | IHU presets 1–6 | 1 = discoverable, 3 = reconnect, 6 = disconnect |
| 0x45 / 0x46 | SEEK+/− long press | unassigned |
| 0x84 / 0x88 | middle SEEK long / >2 s | unassigned |
| 0x76 | CD/RDM long press (random) | unassigned |
| 0xB1 / 0xB0 | pause on / off | unassigned |

## SID text (optional feature, `SID_TEXT_CONTROL_ENABLED`)

Write access must be **granted before every write**
([SidResource.cpp](../SidResource.cpp)):

1. **Request** on 0x357 every ~1 s:
   `[0]=0x1F` (node address), `[1]=0x02` (SID object: row 2),
   `[2]=` request type: `0x05` static text, `0x01` driver action
   ("driver breakthrough", used right after a button press), `0xFF` =
   release/don't want to write; `[3]=0x12` (function ID); rest 0.
2. **Grant** arrives on 0x368: `data[0]==0x02 && data[1]==0x12` → we may
   write **once**.
3. **Write** on 0x337 as a 3-frame group, 10 ms apart:
   byte0 = `0x42, 0x01, 0x00` (first-of-3 marker, then countdown),
   byte1 = `0x96`, byte2 = `0x82` on event / `0x02` static,
   bytes 3-7 = 5 ASCII chars each → 12 visible chars on row 2 (SID charset
   is slightly nonstandard; plain ASCII is safe).
4. Watch 0x348: if the IHU requests driver breakthrough (`data[2]` 0x03 or
   0x05), re-request breakthrough ourselves.

Known issue: text flicker on some cars (9-3 and 9-5 both reported) — the
grant/write timing is sensitive. Nav-equipped cars don't route audio text to
the SID at all; feature silently does nothing there.

## Sound request 0x430

`80 04 00 00 00 00 00 00` = one beep, sent by v6 when entering CDC mode.
Field advice on the 9-5: beeps aggravated debugging of warning-light issues —
the successor should make beeps optional or drop them.

## Sources

- v6.1.1 source (this repo) — the executable specification
- [Tomi Liljemark's I-Bus protocol pages (Wayback)](http://web.archive.org/web/20250122150340/http://pikkupossu.1g.fi/tomi/projects/i-bus/i-bus.html)
- [9-5 handshake & warning-light findings — SaabCentral (Wayback)](http://web.archive.org/web/20230323083251/https://www.saabcentral.com/threads/canbus-version-8bit-29bit-2004-9-5.286321/)
- [SAAB_9-5_NOTES.md](SAAB_9-5_NOTES.md) — model-specific research
