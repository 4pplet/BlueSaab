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

#ifndef BUTTONS_H_
#define BUTTONS_H_
#include "SaabCan.h"

// Button decoding and the button->action map live in IbusProtocol (pure,
// host-tested); this class just wires them to the CAN frame and Bluetooth.
class Buttons {
public:
	Buttons();
	void initialize();
	void onFrame(CANMessage& frame);
};

extern Buttons buttons;

#endif /* BUTTONS_H_ */
