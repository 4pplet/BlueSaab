/* C++ class for handling SID (SAAB Information Display) resource requests and write access
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

#include "SidResource.h"
#include "MessageSender.h"
#include "Scroller.h"
#include "SaabCan.h"

/**
 * Various constants used for SID text control
 */

#define NODE_APL_ADR                    0x1F
#define NODE_SID_FUNCTION_ID            0x12
#define NODE_DISPLAY_RESOURCE_REQ       0x357
#define NODE_WRITE_TEXT_ON_DISPLAY      0x337

SidResource sidResource;

SidResource::SidResource():
		textSender(0x20, NODE_WRITE_TEXT_ON_DISPLAY, sidMessageGroup, 3, 10),
		thread(osPriorityNormal, 384)
{
	sidDriverBreakthroughNeeded = false;
	sidWriteAccessWanted = false;
	writeTextOnDisplayUpdateNeeded = false;
	tempText[0] = 0;
	tempGrants = 0;
	lastTextSend = 0;

	// Fill in some default values
	memcpy(sidMessageGroup[0],"\x42\x96\x02" "BlueS",sizeof(sidMessageGroup[0]));
	memcpy(sidMessageGroup[1],"\x01\x96\x02" "aab v",sizeof(sidMessageGroup[1]));
	memcpy(sidMessageGroup[2],"\x00\x96\x02" "6\0\0\0\0",sizeof(sidMessageGroup[2]));
}

SidResource::~SidResource() {

}

void SidResource::initialize() {
	saabCan.attach(DISPLAY_RESOURCE_GRANT, callback(this, &SidResource::grantReceived));
	saabCan.attach(IHU_DISPLAY_RESOURCE_REQ, callback(this, &SidResource::ihuRequestReceived));
//	getLog()->log("SidResource::initialize()\r\n");
	thread.start(callback(this, &SidResource::run));
	#if STACK_MONITOR_ENABLED
		getLog()->registerThread("SidResource::run", &thread);
	#endif
}

/*
 * Signals: 0x10 = driver breakthrough requested, 0x20 = SID granted us the
 * display, 0x40 = scroller clear requested (CDC mode change). All text
 * assembly, scroller access and sending happens on this thread - the CAN
 * RX interrupt only sets signals. (Before v6.1.4 the text was formatted in
 * the ISR, where the scroller's semaphore silently cannot be taken - a
 * likely cause of the long-reported SID text corruption/flicker.)
 */
void SidResource::run() {
	uint32_t lastRequest = us_ticker_read() - 100 * 1000;
	while(1) {
		// Keep >=100 ms between our 0x357 requests, but keep servicing
		// grant/clear signals while we wait out the spacing.
		while (true) {
			uint32_t since_ms = (us_ticker_read() - lastRequest) / 1000;
			if (since_ms >= 100)
				break;
			osEvent result = Thread::signal_wait(0, 100 - since_ms);
			if (result.status == osEventSignal)
				handleSignals(result.value.signals);
		}

		// Capture-and-clear the breakthrough flag atomically: the CAN ISR
		// can set it between a plain read and a later clear, which would
		// silently downgrade a driver-action request to static.
		bool breakthrough;
		__disable_irq();
		breakthrough = sidDriverBreakthroughNeeded;
		sidDriverBreakthroughNeeded = false;
		__enable_irq();

		sendDisplayRequest(breakthrough);
		lastRequest = us_ticker_read();

		int32_t remaining = NODE_UPDATE_BASETIME;
		while (remaining > 0) {
			osEvent result = Thread::signal_wait(0, remaining);
			if (result.status != osEventSignal)
				break; // timeout: re-request on the 1 s schedule
			handleSignals(result.value.signals);
			if (result.value.signals & 0x10)
				break; // driver breakthrough: re-request (outer loop paces it)
			remaining = NODE_UPDATE_BASETIME - (int32_t)((us_ticker_read() - lastRequest) / 1000);
		}
	}
}

void SidResource::handleSignals(int32_t signals) {
	if (signals & 0x40)
		scroller.clear();
	if (signals & 0x20)
		writeGrantedText();
}

void SidResource::writeGrantedText() {
	// Pace consecutive text groups: the previous 3-frame 0x337 group takes
	// ~20 ms to transmit (10 ms spacing); writing sidMessageGroup or
	// triggering the sender before it finishes would tear the group or
	// violate the >=10 ms same-ID rule at the group seam.
	uint32_t since_ms = (us_ticker_read() - lastTextSend) / 1000;
	if (since_ms < 35)
		Thread::wait(35 - since_ms);

	// Snapshot the temporary-text state under a critical section: the CAN
	// ISR (showTemporary) can otherwise interleave with the decrement or
	// rewrite tempText mid-copy.
	char localTemp[sizeof(tempText)];
	bool useTemp = false;
	__disable_irq();
	if (tempGrants > 0) {
		tempGrants--;
		memcpy(localTemp, tempText, sizeof(localTemp));
		useTemp = true;
	}
	__enable_irq();

	if (useTemp) {
		formatTextMessage(localTemp, writeTextOnDisplayUpdateNeeded);
	} else {
		const char *buffer = scroller.get();
		formatTextMessage(buffer[0] ? buffer : MODULE_NAME, writeTextOnDisplayUpdateNeeded);
	}
	// One-shot: only the first write after activation is an "event" write
	// (0x82); subsequent scroll updates are static (0x02). This flag was
	// never cleared before - every write since activation carried the event
	// mark, a candidate cause of SID flicker.
	writeTextOnDisplayUpdateNeeded = false;
	textSender.send();
	lastTextSend = us_ticker_read();
}

/**
 * Sends a request for using the SID, row 2. We may NOT start writing until we've received a grant frame with the correct function ID!
 */

void SidResource::sendDisplayRequest(bool driverBreakthrough) {

	/* Format of NODE_DISPLAY_RESOURCE_REQ frame:
	 ID: Node ID requesting to write on SID
	 [0]: Request source
	 [1]: SID object to write on; 0 = entire SID; 1 = 1st row; 2 = 2nd row
	 [2]: Request type: 1 = Engineering test; 2 = Emergency; 3 = Driver action; 4 = ECU action; 5 = Static text; 0xFF = We don't want to write on SID
	 [3]: Request source function ID
	 [4-7]: Zeroed out; not in use
	 */

	unsigned char displayRequestCmd[8];
	displayRequestCmd[0] = NODE_APL_ADR;
	displayRequestCmd[1] = 0x02;
	displayRequestCmd[2] = (sidWriteAccessWanted ? (driverBreakthrough ? 0x01 : 0x05) : 0xFF);
	displayRequestCmd[3] = NODE_SID_FUNCTION_ID;
	displayRequestCmd[4] = 0x00;
	displayRequestCmd[5] = 0x00;
	displayRequestCmd[6] = 0x00;
	displayRequestCmd[7] = 0x00;

	saabCan.sendCanFrame(NODE_DISPLAY_RESOURCE_REQ, displayRequestCmd);
}

void SidResource::showTemporary(const char *text, int grants) {
	tempGrants = 0; // disarm while the text is being swapped (callers may race the CAN ISR)
	strncpy(tempText, text, sizeof(tempText) - 1);
	tempText[sizeof(tempText) - 1] = 0;
	tempGrants = grants;
}

void SidResource::grantReceived(CANMessage& frame) {
	// ISR context: just signal the thread, which formats and sends.
	if (sidWriteAccessWanted) {
		if ((frame.data[0] == 0x02) && (frame.data[1] == NODE_SID_FUNCTION_ID)) {
			thread.signal_set(0x20);
		}
	}
}

void SidResource::ihuRequestReceived(CANMessage& frame) {
	if (sidWriteAccessWanted) {
		if ((frame.data[2] == 0x03) || (frame.data[2] == 0x05)) { // IHU requested DriverBreakthrough
			requestDriverBreakthrough();
		}
	}
}

/**
 * Formats provided text for writing on the SID. This function assumes that we have been granted write access. Do not call it if we haven't!
 * Note: the character set used by the SID is slightly nonstandard. "Normal" characters should work fine.
 */

void SidResource::formatTextMessage(const char textIn[], bool event) {
	// Copy the provided string and make sure we have a new array of the correct length
	unsigned char textToSid[15];
	int n = strnlen(textIn, 12); // 12 is the number of characters SID can display on each row; anything beyond 12 is going to be zeroed out
	for (int i = 0; i < n; i++) {
		textToSid[i] = textIn[i];
	}
	for (int i = n; i < 15; i++) {
		textToSid[i] = 0;
	}

	unsigned char eventByte = event ? 0x82 : 0x02;
	sidMessageGroup[0][2] = eventByte;
	sidMessageGroup[1][2] = eventByte;
	sidMessageGroup[2][2] = eventByte;
	memcpy(&sidMessageGroup[0][3], textToSid, 5);
	memcpy(&sidMessageGroup[1][3], textToSid + 5, 5);
	memcpy(&sidMessageGroup[2][3], textToSid + 10, 5);
}
