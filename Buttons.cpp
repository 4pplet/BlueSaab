/* C++ class for SAAB infotainment head unit and steering wheel button event handling
 * Copyright (C) 2018 Girts Linde
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Buttons.h"
#include "IbusProtocol.h"
#include "Bluetooth.h"
#include "SidResource.h"

Buttons buttons;

Buttons::Buttons() {
}

void Buttons::initialize() {
	saabCan.attach(CDC_CONTROL, callback(this, &Buttons::onFrame));
}

void Buttons::onFrame(CANMessage& frame) {
	ibus::Button button = ibus::decodeButton(frame.data);
	if (button != ibus::NONE) {
#if SID_TEXT_CONTROL_ENABLED
		// Driver breakthrough only for actual (recognized) button presses -
		// not for every 0x80-flagged frame (mode changes, pause, unmapped
		// codes), which needlessly escalated our display requests.
		sidResource.requestDriverBreakthrough();
#endif
//		getLog()->log("Buttons::onFrame button %d", button);
		switch (ibus::buttonAction(button)) {
		case ibus::ACT_PLAY_PAUSE:
			bluetooth.play();
			break;
		case ibus::ACT_NEXT:
			bluetooth.next();
			break;
		case ibus::ACT_PREV:
			bluetooth.prev();
			break;
		case ibus::ACT_PAIR:
			bluetooth.discoverable();
			#if SID_TEXT_CONTROL_ENABLED
				sidResource.showTemporary("PAIRING", 10); // ~10 s, like STP,10 (actual discoverable window undocumented)
			#endif
			break;
		case ibus::ACT_RECONNECT:
			bluetooth.reconnect();
			break;
		case ibus::ACT_VOLUME_DOWN:
			bluetooth.volumeDown();
			break;
		case ibus::ACT_VOLUME_UP:
			bluetooth.volumeUp();
			break;
		case ibus::ACT_DISCONNECT:
			bluetooth.disconnect();
			break;
		case ibus::ACT_NONE:
			break;
		}
	} else {
//		getLog()->log("Buttons::onFrame button unknown");
	}
}
