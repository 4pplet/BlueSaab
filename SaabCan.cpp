/* C++ class for handling/emulating node communications on SAAB I-bus
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

#include "mbed.h"
#include "rtos.h"
#include "SaabCan.h"

// Construct at the real I-Bus bitrate: the default CAN() ctor would join
// the live car bus at 100 kbit/s error-active during static init, actively
// corrupting frames every ignition-on until initialize() fixed the rate.
CAN iBus(PB_8, PB_9, 47619);
CANMessage canRxFrame;
SaabCan saabCan;

void SaabCan::initialize(int hz) {
	// The constructor already set the bitrate; re-applying it here is the
	// second chance if that failed. Neither step may halt the unit.
	if (!iBus.frequency(hz))
		getLog()->log("CAN: bitrate setup failed\r\n");
	if (!iBus.mode(CAN::Normal))
		getLog()->log("CAN: normal mode setup failed\r\n");

	iBus.attach(callback(this,&SaabCan::onRx), mbed::CAN::RxIrq);
	send_thread.start(callback(this, &SaabCan::sendFunc));
	#if STACK_MONITOR_ENABLED
		getLog()->registerThread("SaabCan::sendFunc", &send_thread);
	#endif
}

void SaabCan::sendCanFrame(int canId, const unsigned char *data) {
	CANMessage *box = canFrameQueue.alloc();
	if (box == NULL) {
		txDropped++;
		return; // TX queue full - drop the frame rather than hardfault
	}
	CANMessage *canTxFrame = new (box) CANMessage();
	canTxFrame->id = canId;
	for (int i = 0; i < canTxFrame->len; i++) {
		canTxFrame->data[i] = data[i];
	}
	canFrameQueue.put(canTxFrame);
}

extern DigitalOut aliveLed;

void SaabCan::onRx() {
	while (iBus.read(canRxFrame)) {
		// The I-Bus carries 11-bit data frames only. Anything else could
		// alias a handled ID (a remote frame would replay stale data[]).
		if (canRxFrame.format != CANStandard || canRxFrame.type != CANData)
			continue;
		for (int i = 0; i < CAN_MAX_CALLBACKS; i++) {
			// Unused slots have id 0 and an empty callback: calling one is
			// an MBED_ASSERT -> permanent halt, so an ID-0 frame on the bus
			// used to kill the unit.
			if (callBacks[i].id == canRxFrame.id && callBacks[i].callBack) {
				callBacks[i].callBack.call(canRxFrame);
			}
		}
	}
}

void SaabCan::sendFunc() {
	while (true) {
		osEvent evt = canFrameQueue.get();
		if (evt.status == osEventMail) {
			CANMessage *message = (CANMessage*) evt.value.p;
//			getLog()->logFrame(message);
//			unsigned tde = iBus.tderror();

			// All 3 TX mailboxes busy (lost arbitration under load): retry for
			// up to ~20 ms instead of dropping at once - a lost 0x6A2 breaks
			// the 9-5 handshake, a lost 0x337 tears SID text. Single consumer,
			// so frame order is kept.
			int tries = 0;
			while (iBus.write(*message) == 0) {
				if (++tries > 20) {
					txErrors++; // dropped
					break;
				}
				Thread::wait(1);
			}
//			unsigned rde = iBus.rderror();
//			tde = iBus.tderror();
//			getLog()->log("    rde=%d\r\n", rde);
//			getLog()->log("    tde=%d\r\n", tde);
//
//			uint32_t esr = iBus.read_ESR();
//			getLog()->log("    ESR=%08x\r\n", esr);
//			getLog()->log("    BOFF=%x\r\n", esr & 4);
//			getLog()->log("    MCR=%08x\r\n", iBus.read_MCR());

			canFrameQueue.free(message);
			aliveLed = !aliveLed;
		}
	}
}

unsigned SaabCan::getRxErrorCounter() {
	return iBus.rderror();
}

unsigned SaabCan::getTxErrorCounter() {
	return iBus.tderror();
}

uint32_t SaabCan::getESR() {
	return iBus.read_ESR();
}

void SaabCan::attach(unsigned int canId, Callback<void(CANMessage&)> callBack) {
	for (int i = 0; i < CAN_MAX_CALLBACKS; i++) {
		if (callBacks[i].id == 0) {
			callBacks[i].callBack = callBack;
			callBacks[i].id = canId;
			return;
		}
	}
	getLog()->log("SaabCan::attach: table full, callback for id %x DROPPED\r\n", canId);
}
