/* SAAB I-Bus CD-changer protocol logic — see IbusProtocol.h.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stddef.h>
#include <string.h>
#include "IbusProtocol.h"

namespace ibus {

Button decodeButton(const unsigned char data[8]) {
	if (data[0] != 0x80) // not a command/button event
		return NONE;
	switch (data[1]) {
	case 0x59: // NXT
		return NXT;
	case 0x45: // SEEK+ button long press on IHU
		return SEEK_PLUS_LONG;
	case 0x46: // SEEK- button long press on IHU
		return SEEK_MINUS_LONG;
	case 0x84: // SEEK button (middle) long press on IHU
		return SEEK_MIDDLE_LONG;
	case 0x88: // > 2 second long press of SEEK button (middle) on IHU
		return SEEK_MIDDLE_EXTRA_LONG;
	case 0x76: // Random ON/OFF (Long press of CD/RDM button)
		return RANDOM;
	case 0xB1: // Pause ON
		return PAUSE_ON;
	case 0xB0: // Pause OFF
		return PAUSE_OFF;
	case 0x35: // Track +
		return TRACK_PLUS;
	case 0x36: // Track -
		return TRACK_MINUS;
	case 0x68: // IHU buttons "1-6"
		switch (data[2]) {
		case 0x01:
			return IHU1;
		case 0x02:
			return IHU2;
		case 0x03:
			return IHU3;
		case 0x04:
			return IHU4;
		case 0x05:
			return IHU5;
		case 0x06:
			return IHU6;
		}
	}
	return NONE;
}

Action buttonAction(Button button) {
	switch (button) {
	case NXT:
		return ACT_PLAY_PAUSE;
	case TRACK_PLUS:
		return ACT_NEXT;
	case TRACK_MINUS:
		return ACT_PREV;
	case IHU1:
	case SEEK_MIDDLE_EXTRA_LONG: // wheel-only pairing, pre-v6 muscle memory
		return ACT_PAIR;
	case IHU3:
		return ACT_RECONNECT;
	case IHU4:
		return ACT_VOLUME_DOWN;
	case IHU5:
		return ACT_VOLUME_UP;
	case IHU6:
		return ACT_DISCONNECT;
	default:
		return ACT_NONE;
	}
}

CdcCommand decodeCdcCommand(const unsigned char data[8]) {
	if (data[0] != 0x80)
		return CDC_CMD_NONE;
	switch (data[1]) {
	case 0x24:
		return CDC_CMD_ON;
	case 0x14:
		return CDC_CMD_OFF;
	default:
		return CDC_CMD_NONE;
	}
}

NodeReply nodeReplyFor(const unsigned char pollData[8]) {
	// "Here be dragons": the bottom half of byte 3 of the 0x6A1 poll
	// selects the reply.
	switch (pollData[3] & 0x0F) {
	case 0x3:
		return NODE_REPLY_POWER_ON;
	case 0x2:
		return NODE_REPLY_ACTIVE;
	case 0x8:
		return NODE_REPLY_POWER_DOWN;
	default:
		return NODE_REPLY_NONE;
	}
}

static const unsigned char kPowerOn[NODE_STATUS_FRAMES][8] = {
		{ 0x32, 0x00, 0x00, 0x03, 0x01, 0x02, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 }
};
static const unsigned char kActive[NODE_STATUS_FRAMES][8] = {
		{ 0x32, 0x00, 0x00, 0x16, 0x01, 0x02, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 }
};
static const unsigned char kPowerDown[NODE_STATUS_FRAMES][8] = {
		{ 0x32, 0x00, 0x00, 0x19, 0x01, 0x00, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 }
};

const unsigned char (*nodeStatusFrames(NodeReply reply))[8] {
	switch (reply) {
	case NODE_REPLY_POWER_ON:
		return kPowerOn;
	case NODE_REPLY_ACTIVE:
		return kActive;
	case NODE_REPLY_POWER_DOWN:
		return kPowerDown;
	default:
		return NULL;
	}
}

void buildCdcStatus(unsigned char out[8], bool event, bool remote, bool cdcActive) {
	/* Format of GENERAL_STATUS_CDC frame:
	 [0]: bit 7: FCI NEW DATA: 0 - sent on base time, 1 - sent on event
	      bit 6: FCI REMOTE CMD: 0 - internal change, 1 - due to CDC_COMMAND
	      bit 5: FCI DISC PRESENCE VALID (always 1)
	 [1]: disc presence validation      [2]: disc presence bitmap (6 discs)
	 [3]: bits 7-4 disc mode, 3-0 disc number
	 [4]: track number   [5]: minute   [6]: second (0xFF = n/a)
	 [7]: CD changer status; 0xD0 = married to the car (bypasses Tech2 VIN
	      marriage of a real changer)
	 */
	out[0] = ((event ? 0x4 : 0x0) | (remote ? 0x2 : 0x0) | 0x1) << 5;
	out[1] = (cdcActive ? 0xFF : 0x00);
	out[2] = (cdcActive ? 0x3F : 0x01);
	out[3] = (cdcActive ? 0x41 : 0x01); // ToDo: check 0x01 | (discMode << 4) | 0x01
	out[4] = 0xFF;
	out[5] = 0xFF;
	out[6] = 0xFF;
	out[7] = 0xD0;
}

void formatSidText(unsigned char group[3][8], const char *text, bool event) {
	// 12 is the number of characters the SID shows per row; anything beyond
	// is cut, and the rest of the 15 text bytes is zero.
	unsigned char textToSid[15];
	size_t n = strnlen(text, 12);
	for (size_t i = 0; i < n; i++) {
		textToSid[i] = (unsigned char)text[i];
	}
	for (size_t i = n; i < sizeof(textToSid); i++) {
		textToSid[i] = 0;
	}

	unsigned char eventByte = event ? 0x82 : 0x02;
	for (int f = 0; f < 3; f++) {
		group[f][2] = eventByte;
		memcpy(&group[f][3], textToSid + 5 * f, 5);
	}
}

} // namespace ibus
