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

CAN iBus(PB_8, PB_9);
CANMessage canRxFrame;
SaabCan saabCan;

void SaabCan::initialize(int hz) {
	if (iBus.frequency(hz) && iBus.mode(CAN::Normal)) {
//		getLog()->log("CAN OK\r\n");
	} else {
//		getLog()->log("CAN NOT OK\r\n");
	}

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
		for (int i = 0; i < CAN_MAX_CALLBACKS; i++) {
			if (callBacks[i].id == canRxFrame.id) {
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

			if (iBus.write(*message) == 0)
				txErrors++;
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
