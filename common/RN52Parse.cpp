/* Parsers for RN52 command responses — see RN52Parse.h.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ctype.h>
#include <string.h>
#include "RN52Parse.h"

namespace rn52 {

static bool isDigit(char c) {
	return isdigit((unsigned char)c) != 0;
}

bool parseVersion(const char *line, char *out, size_t outSize) {
	if (outSize == 0)
		return false;
	for (const char *p = line; *p; p++) {
		if (isDigit(p[0]) && p[1] == '.' && isDigit(p[2])) {
			size_t i = 0;
			while (i < outSize - 1 && (isDigit(*p) || *p == '.')) {
				out[i++] = *p++;
			}
			out[i] = 0;
			return true;
		}
	}
	return false;
}

static int hexVal(char c) {
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	return (c - 'A' + 10);
}

bool decodeQResponse(const char *line, bool *a2dpConnected, bool *trackChanged) {
	if (strlen(line) != 6)
		return false;
	for (int i = 0; i < 4; i++) {
		if (!isxdigit((unsigned char)line[i]))
			return false;
	}
	int profile = (hexVal(line[0]) << 4 | hexVal(line[1])) & 0x0f;
	*a2dpConnected = (profile & 0x04) != 0;
	*trackChanged = (hexVal(line[0]) & 0x02) != 0;
	return true;
}

} // namespace rn52
