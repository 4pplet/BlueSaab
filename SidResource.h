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

#ifndef SAAB_CDC_SIDRESOURCE_H_
#define SAAB_CDC_SIDRESOURCE_H_
#define SID_TEXT_CONTROL_ENABLED			1

#include <rtos.h>
#include "MessageSender.h"
#include "Scroller.h"


class SidResource {
	unsigned char sidMessageGroup[3][8];
	MessageSender textSender;

	bool sidDriverBreakthroughNeeded;
	bool sidWriteAccessWanted;

	char tempText[13];
	volatile int tempGrants;
	volatile bool clearPending;

	bool writeTextOnDisplayUpdateNeeded;

	Thread thread;

	void run();

	void sendDisplayRequest();
	void handleSignals(int32_t signals);
	void writeGrantedText();
	void formatTextMessage(const char textIn[], bool event);
public:
	SidResource();
	~SidResource();

	void initialize();
	// Show a fixed text (max 12 chars) for the next 'grants' display grants
	// (roughly one grant per second) instead of the scroller content.
	void showTemporary(const char *text, int grants);
	void requestDriverBreakthrough() {
		sidDriverBreakthroughNeeded = true;
		thread.signal_set(0x10);
	}
	// activate()/deactivate() are called from the CAN RX ISR. The scroller
	// clear must NOT happen here: the scroller's semaphore cannot be taken
	// in an ISR (RTX refuses and returns an error - unchecked, it silently
	// skips locking) but release DOES work in an ISR, so an ISR-side
	// wait/release pair inflates the semaphore by one token each time,
	// permanently breaking the lock. The thread does the clear instead
	// (signal 0x40).
	void activate() {
		clearPending = true;
		sidWriteAccessWanted = true;
		writeTextOnDisplayUpdateNeeded = true;
		thread.signal_set(0x40);
	}
	void deactivate() {
		clearPending = true;
		sidWriteAccessWanted = false;
		thread.signal_set(0x40);
	}

	void grantReceived(CANMessage& frame);
	void ihuRequestReceived(CANMessage& frame);
};

	#if SID_TEXT_CONTROL_ENABLED
		extern SidResource sidResource;
	#endif

#endif /* SAAB_CDC_SIDRESOURCE_H_ */
