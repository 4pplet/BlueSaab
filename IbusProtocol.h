/* SAAB I-Bus CD-changer protocol logic — pure functions, no mbed/RTOS
 * dependencies, so the exact code the firmware runs is unit-tested on the
 * host (test/host_tests.cpp). The frame formats are documented in
 * docs/IBUS_PROTOCOL.md; the tests assert those documented bytes.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IBUS_PROTOCOL_H_
#define IBUS_PROTOCOL_H_

namespace ibus {

// ---- 0x3C0 CDC_CONTROL (IHU -> CDC) ----------------------------------------

// Buttons decoded from 0x3C0 (byte 0 = 0x80 marks a command/button event).
enum Button {
	NONE,
	IHU1, IHU2, IHU3, IHU4, IHU5, IHU6,
	NXT,
	SEEK_PLUS_LONG,
	SEEK_MINUS_LONG,
	SEEK_MIDDLE_LONG,
	SEEK_MIDDLE_EXTRA_LONG,
	RANDOM,
	PAUSE_ON,
	PAUSE_OFF,
	TRACK_PLUS,
	TRACK_MINUS
};

// What the unit does for a button — the in-car interface contract
// (docs/USAGE_v6.md). Recognized buttons without an action are ACT_NONE.
enum Action {
	ACT_NONE,
	ACT_PLAY_PAUSE,
	ACT_NEXT,
	ACT_PREV,
	ACT_PAIR,        // discoverable mode
	ACT_RECONNECT,
	ACT_VOLUME_DOWN, // AVRCP volume to the phone
	ACT_VOLUME_UP,
	ACT_DISCONNECT
};

// CDC mode commands carried in the same frame.
enum CdcCommand {
	CDC_CMD_NONE,
	CDC_CMD_ON,  // 0x24: CD-changer source selected
	CDC_CMD_OFF  // 0x14: source deselected
};

// data = the 8 data bytes of a 0x3C0 frame.
Button decodeButton(const unsigned char data[8]);
Action buttonAction(Button button);
CdcCommand decodeCdcCommand(const unsigned char data[8]);

// ---- 0x6A1 node-status poll -> 0x6A2 reply sequence ------------------------

enum NodeReply {
	NODE_REPLY_NONE,
	NODE_REPLY_POWER_ON,
	NODE_REPLY_ACTIVE,
	NODE_REPLY_POWER_DOWN
};

enum { NODE_STATUS_FRAMES = 4 };

// Which reply a 0x6A1 poll asks for (low nibble of its byte 3).
NodeReply nodeReplyFor(const unsigned char pollData[8]);

// The 4-frame 0x6A2 reply sequence; NULL for NODE_REPLY_NONE.
// FROZEN: a wrong byte here lit airbag/MIL warnings on a 9-5.
const unsigned char (*nodeStatusFrames(NodeReply reply))[8];

// ---- 0x3C8 GENERAL_STATUS_CDC -----------------------------------------------

void buildCdcStatus(unsigned char out[8], bool event, bool remote, bool cdcActive);

// ---- 0x337 SID text (3-frame group, row 2) ----------------------------------

// Writes the event byte and up to 12 chars of text (zero-padded) into the
// group's bytes 2..7; bytes 0..1 of each frame (sequence/row markers) are
// left untouched.
void formatSidText(unsigned char group[3][8], const char *text, bool event);

} // namespace ibus

#endif /* IBUS_PROTOCOL_H_ */
