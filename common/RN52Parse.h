/* Parsers for RN52 command responses — pure functions, no mbed/RTOS
 * dependencies, unit-tested on the host (test/host_tests.cpp).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RN52_PARSE_H_
#define RN52_PARSE_H_

#include <stddef.h>

namespace rn52 {

// Response line of the V (version) command, e.g. "RN52-I Ver 1.16 ..." or
// "Ver 1.10 04/04/13". Copies the first digit.digit token ("1.16") into out
// (NUL-terminated, truncated to outSize-1). Returns false if the line has
// no such token.
bool parseVersion(const char *line, char *out, size_t outSize);

// Response line of the Q (status) command: 4 hex digits + "\r\n" (exactly
// 6 characters). a2dpConnected = bit 2 of the 2nd digit (connected profile
// mask); trackChanged = bit 1 of the 1st digit (track-change event).
// Returns false if the line isn't a valid Q response.
bool decodeQResponse(const char *line, bool *a2dpConnected, bool *trackChanged);

} // namespace rn52

#endif /* RN52_PARSE_H_ */
