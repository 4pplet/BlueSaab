/* C++ class for emulating CD changer communications on SAAB I-bus
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

#include <string.h>
#include "CDCStatus.h"
#include "SaabCan.h"
#include "Bluetooth.h"
#include "SidResource.h"

CDCStatus cdcStatus;

unsigned char cdcPoweronCmd[NODE_STATUS_TX_MSG_SIZE][8] = {
		{ 0x32, 0x00, 0x00, 0x03, 0x01, 0x02, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 }
};
unsigned char cdcActiveCmd[NODE_STATUS_TX_MSG_SIZE][8] = {
		{ 0x32, 0x00, 0x00, 0x16, 0x01, 0x02, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 }
};
unsigned char cdcPowerdownCmd[NODE_STATUS_TX_MSG_SIZE][8] = {
		{ 0x32, 0x00, 0x00, 0x19, 0x01, 0x00, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 }
};

unsigned char soundCmd[] = {0x80,0x04,0x00,0x00,0x00,0x00,0x00,0x00};

/*
 * All three node-status reply sequences share frame ID 0x6A2, so they must
 * go through ONE thread: with a sender thread per sequence (as before
 * v6.1.4), two IHU state flips in quick succession could interleave two
 * sequences on the bus - violating the >=10 ms same-ID spacing rule and
 * scrambling the reply order the 9-5 IHU is strict about.
 */
class NodeStatusSender {
	Thread thread;
	uint32_t lastFrameSent;
	bool sentAnything;

	void sendSequence(const unsigned char frames[][8]) {
		// Seam guard: >=10 ms between same-ID frames also across sequence
		// boundaries (a poll arriving mid-sequence would otherwise start
		// the next sequence back-to-back with the previous one's last frame).
		if (sentAnything) {
			uint32_t since_ms = (us_ticker_read() - lastFrameSent) / 1000;
			if (since_ms < 10)
				Thread::wait(10 - since_ms);
		}
		for (int i = 0; i < NODE_STATUS_TX_MSG_SIZE; i++) {
			if (i > 0)
				Thread::wait(NODE_STATUS_TX_INTERVAL);
			saabCan.sendCanFrame(NODE_STATUS_TX_CDC, frames[i]);
		}
		lastFrameSent = us_ticker_read();
		sentAnything = true;
	}

	void run() {
		while (1) {
			osEvent evt = Thread::signal_wait(0); // wait for any request flag
			if (evt.status != osEventSignal)
				continue;
			int32_t sig = evt.value.signals;
			// signal_wait returns and clears ALL pending flags - answer every
			// requested sequence rather than dropping the lower-priority ones
			// (the 9-5 IHU requires each poll to be answered).
			if (sig & 0x8)
				sendSequence(cdcPowerdownCmd);
			if (sig & 0x1)
				sendSequence(cdcActiveCmd);
			if (sig & 0x4)
				sendSequence(cdcPoweronCmd);
		}
	}

public:
	NodeStatusSender(): thread(osPriorityNormal, 256), lastFrameSent(0), sentAnything(false) {
		thread.start(callback(this, &NodeStatusSender::run));
		#if STACK_MONITOR_ENABLED
			getLog()->registerThread("NodeStatusSender::run", &thread);
		#endif
	}
	void send(int32_t signal) {
		thread.signal_set(signal);
	}
};

NodeStatusSender nodeStatusSender;

void CDCStatus::initialize() {
	saabCan.attach(NODE_STATUS_RX_IHU, callback(this, &CDCStatus::onIhuStatusFrame));
	saabCan.attach(CDC_CONTROL, callback(this, &CDCStatus::onCDCControlFrame));
//	getLog()->log("CDCStatus::initialize()\r\n");
	thread.start(callback(this, &CDCStatus::run));
	#if STACK_MONITOR_ENABLED
		getLog()->registerThread("CDCStatus::run", &thread);
	#endif
}

void CDCStatus::onCDCControlFrame(CANMessage& frame) {
	if (frame.data[0] == 0x80) {
		switch (frame.data[1]) {
		case 0x24:
			cdcActive = true;
			#if SID_TEXT_CONTROL_ENABLED
				{
					// Show firmware + RN52 versions for a few seconds, e.g.
					// "6.1.4 R1.16". Built by hand - this runs in the CAN RX
					// interrupt, where printf-family calls are not safe.
					char verText[13];
					strcpy(verText, FIRMWARE_VERSION " R"); // 8 chars
					strncat(verText, bluetooth.getRN52Version(), sizeof(verText) - 9);
					sidResource.showTemporary(verText, 4);
				}
				sidResource.activate();
			#endif
			#if CDC_ENTRY_BEEP_ENABLED
				saabCan.sendCanFrame(SOUND_REQUEST, soundCmd);
			#endif
			thread.signal_set(0x2);
			bluetooth.connectable();
			bluetooth.reconnect();
			break;
		case 0x14:
			#if SID_TEXT_CONTROL_ENABLED
				sidResource.deactivate();
			#endif
			cdcActive = false;
			thread.signal_set(0x2);
			bluetooth.disconnect();
			break;
		}
	}
}

void CDCStatus::onIhuStatusFrame(CANMessage& frame) {

	/*
	 Here be dragons... This part of the code is responsible for causing lots of headache
	 We look at the bottom half of 3rd byte of '6A1' frame to determine what the "reply" should be
	 */

	switch (frame.data[3] & 0x0F) {
	case (0x3):
		nodeStatusSender.send(0x4);
		break;
	case (0x2):
		nodeStatusSender.send(0x1);
		break;
	case (0x8):
		nodeStatusSender.send(0x8);
		break;
	}
}

void CDCStatus::run() {
//	getLog()->log("CDCStatus::run()\r\n");
	bool cdcStatusResendNeeded = false;
	bool cdcStatusResendDueToCdcCommand = false;

	while(1) {
//		getLog()->log("cdcActive %d\r\n", cdcActive);
		sendCdcStatus(cdcStatusResendNeeded, cdcStatusResendDueToCdcCommand, cdcActive);
		Thread::wait(50);
		cdcStatusResendNeeded = false;
		cdcStatusResendDueToCdcCommand = false;
		osEvent result = Thread::signal_wait(0x2, CDC_STATUS_TX_BASETIME-50);
		if (result.status == osEventSignal) {
			cdcStatusResendNeeded = true;
			cdcStatusResendDueToCdcCommand = true;
		}
	}
}

void CDCStatus::sendCdcStatus(bool event, bool remote, bool cdcActive) {

	/* Format of GENERAL_STATUS_CDC frame:
	 ID: CDC node ID
	 [0]:
	 byte 0, bit 7: FCI NEW DATA: 0 - sent on base time, 1 - sent on event
	 byte 0, bit 6: FCI REMOTE CMD: 0 - status change due to internal operation, 1 - status change due to CDC_COMMAND frame
	 byte 0, bit 5: FCI DISC PRESENCE VALID: 0 - disc presence signal is not valid, 1 - disc presence signal is valid
	 [1]: Disc presence validation (boolean)
	 byte 1-2, bits 0-15: DISC PRESENCE: (bitmap) 0 - disc absent, 1 - disc present. Bit 0 is disc 1, bit 1 is disc 2, etc.
	 [2]: Disc presence (bitmap)
	 byte 1-2, bits 0-15: DISC PRESENCE: (bitmap) 0 - disc absent, 1 - disc present. Bit 0 is disc 1, bit 1 is disc 2, etc.
	 [3]: Disc number currently playing
	 byte 3, bits 7-4: DISC MODE
	 byte 3, bits 3-0: DISC NUMBER
	 [4]: Track number currently playing
	 [5]: Minute of the current track
	 [6]: Second of the current track
	 [7]: CD changer status; D0 = Married to the car
	 */

	unsigned char cdcGeneralStatusCmd[8] = { 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xD0 };
	cdcGeneralStatusCmd[0] = ((event ? 0x07 : 0x00) | (remote ? 0x00 : 0x01)) << 5;
	cdcGeneralStatusCmd[1] = (cdcActive ? 0xFF : 0x00); // Validation for presence of six discs in the magazine
	cdcGeneralStatusCmd[2] = (cdcActive ? 0x3F : 0x01); // There are six discs in the magazine
	cdcGeneralStatusCmd[3] = (cdcActive ? 0x41 : 0x01); // ToDo: check 0x01 | (discMode << 4) | 0x01

	saabCan.sendCanFrame(GENERAL_STATUS_CDC, cdcGeneralStatusCmd);
}
