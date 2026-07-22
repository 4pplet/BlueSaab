/*
 "BlueSaab v6.1": a CD changer emulator for older SAAB cars with RN52 Bluetooth module by Microchip Technology Inc.

 * Copyright (C) 2018 Girts Linde and Karlis Veilands
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

 Credits:

 Hardware design:           Seth Evans (http://bluesaab.blogspot.com)
 Initial code:              Seth Evans and Emil Fors
 Information on SAAB I-Bus: Tomi Liljemark (http://pikkupossu.1g.fi/tomi/projects/i-bus/i-bus.html)
 RN52 handling:             based on code by Tim Otto (https://github.com/timotto/RN52lib)
*/

#include "mbed.h"
#include "rtos.h"
#include "SaabCan.h"
#include "Buttons.h"
#include "CDCStatus.h"
#include "MessageSender.h"
#include "Bluetooth.h"
#include "SidResource.h"

DigitalOut aliveLed(PA_1);

Thread logThread(osPriorityLow, 1536); // full-newlib vfprintf needs headroom

int main() {
	logThread.start(callback(getLog(), &SerialLog::run));

	getLog()->log("BlueSaab\r\n");
	getLog()->log("Hardware version: 6.1\r\n");
	getLog()->log("Firmware version: " FIRMWARE_VERSION "\r\n");
	#if STACK_MONITOR_ENABLED
		getLog()->registerThread("logThread", &logThread);
	#endif

	aliveLed = 1;

	// CAN handlers first: bluetooth.initialize() blocks ~5 s waiting for the
	// RN52 to reboot, and with the old order the node sat on a live bus with
	// the RX IRQ running but nobody answering 0x6A1 polls or 0x3C0 commands
	// for that whole window. Bluetooth commands queued meanwhile are held in
	// the RN52 command queue and sent once its thread starts.
	saabCan.initialize(47619);
	buttons.initialize();
	cdcStatus.initialize();

	#if SID_TEXT_CONTROL_ENABLED
		sidResource.initialize();
	#endif

	bluetooth.initialize();

	while (1) {
		Thread::wait(1000);
	}
}
