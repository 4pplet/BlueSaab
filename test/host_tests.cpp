/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host unit tests for the platform-independent firmware logic.
 *
 * Test bodies are the previously dormant blocks from Scroller.cpp and
 * utf_convert.cpp (kept verbatim), plus cases for the 6.1.6 UTF-8 fix.
 * Built and run by CI on every push (both char signednesses; the target
 * uses -funsigned-char).
 *
 * Build (as CI does): g++ -std=gnu++98 -funsigned-char -Wall -Wextra -Werror \
 *   -fsanitize=address,undefined -I test/stubs -I . -I common \
 *   test/host_tests.cpp Scroller.cpp utf_convert.cpp IbusProtocol.cpp \
 *   common/RN52Parse.cpp -o BUILD/host_tests
 *
 * The protocol tests (IbusProtocol, RN52Parse) are characterization tests:
 * the field-proven 6.1.1 frames and button map, byte-exact, as documented in
 * docs/IBUS_PROTOCOL.md and docs/USAGE_v6.md. A change that alters a frame
 * byte or a button mapping must fail here and be deliberate.
 */
#undef NDEBUG /* the tests are asserts - never let them compile out */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Scroller.h"
#include "utf_convert.h"
#include "IbusProtocol.h"
#include "RN52Parse.h"

static void test_scroller() {
	Scroller s;

	s.set_info("", "");
	assert(strcmp(s.get(), "") == 0);
	assert(strcmp(s.get(), "") == 0);

	// just artist - short
	s.set_info("a1234", "");
	assert(strcmp(s.get(), "a1234") == 0);
	assert(strcmp(s.get(), "a1234") == 0);

	// just artist - long
	s.set_info("a123456789ABC", "");
	assert(strcmp(s.get(), "a123456789AB") == 0);
	assert(strcmp(s.get(), "123456789ABC") == 0);
	assert(strcmp(s.get(), "23456789ABC ") == 0);
	assert(strcmp(s.get(), "3456789ABC -") == 0);
	assert(strcmp(s.get(), "456789ABC - ") == 0);
	assert(strcmp(s.get(), "56789ABC - a") == 0);

	// just title - short
	s.set_info("", "t1234");
	assert(strcmp(s.get(), "t1234") == 0);
	assert(strcmp(s.get(), "t1234") == 0);

	// just title - long
	s.set_info("", "t123456789ABC");
	assert(strcmp(s.get(), "t123456789AB") == 0);
	assert(strcmp(s.get(), "123456789ABC") == 0);
	assert(strcmp(s.get(), "23456789ABC ") == 0);
	assert(strcmp(s.get(), "3456789ABC -") == 0);
	assert(strcmp(s.get(), "456789ABC - ") == 0);
	assert(strcmp(s.get(), "56789ABC - t") == 0);

	s.set_info("a1234", "t1234");
	assert(strcmp(s.get(), "a1234 - t123") == 0);
	assert(strcmp(s.get(), "1234 - t1234") == 0);
	assert(strcmp(s.get(), "234 - t1234 ") == 0);
	assert(strcmp(s.get(), "34 - t1234 -") == 0);
	assert(strcmp(s.get(), "4 - t1234 - ") == 0);
	assert(strcmp(s.get(), " - t1234 - a") == 0);
	assert(strcmp(s.get(), "- t1234 - a1") == 0);
	assert(strcmp(s.get(), " t1234 - a12") == 0);
	assert(strcmp(s.get(), "t1234 - a123") == 0);
	assert(strcmp(s.get(), "1234 - a1234") == 0);
	assert(strcmp(s.get(), "234 - a1234 ") == 0);
	assert(strcmp(s.get(), "34 - a1234 -") == 0);
	assert(strcmp(s.get(), "4 - a1234 - ") == 0);
	assert(strcmp(s.get(), " - a1234 - t") == 0);
	assert(strcmp(s.get(), "- a1234 - t1") == 0);
	assert(strcmp(s.get(), " a1234 - t12") == 0);
	assert(strcmp(s.get(), "a1234 - t123") == 0);
	assert(strcmp(s.get(), "1234 - t1234") == 0);
	assert(strcmp(s.get(), "234 - t1234 ") == 0);
	assert(strcmp(s.get(), "34 - t1234 -") == 0);
	assert(strcmp(s.get(), "4 - t1234 - ") == 0);
	assert(strcmp(s.get(), " - t1234 - a") == 0);
	assert(strcmp(s.get(), "- t1234 - a1") == 0);

	// If it fits - don't scroll
	s.set_info("a123", "t1234");
	assert(strcmp(s.get(), "a123 - t1234") == 0);
	assert(strcmp(s.get(), "a123 - t1234") == 0);

	s.set_info("a1234", "t12345");
	assert(strncmp(s.get(), "a1234 - t123", 12) == 0);
	assert(strncmp(s.get(), "1234 - t1234", 12) == 0);
	assert(strncmp(s.get(), "234 - t12345", 12) == 0);
	assert(strncmp(s.get(), "34 - t12345 ", 12) == 0);

	s.set_info("a1", "t123456789ABCD");
	assert(strncmp(s.get(), "a1 - t123456", 12) == 0);
	assert(strncmp(s.get(), "1 - t1234567", 12) == 0);
	assert(strncmp(s.get(), " - t12345678", 12) == 0);
	assert(strncmp(s.get(), "- t123456789", 12) == 0);
	s.get(); s.get(); s.get();
	assert(strncmp(s.get(), "23456789ABCD", 12) == 0);
	assert(strncmp(s.get(), "3456789ABCD ", 12) == 0);
	assert(strncmp(s.get(), "456789ABCD -", 12) == 0);
	assert(strncmp(s.get(), "56789ABCD - ", 12) == 0);
	assert(strncmp(s.get(), "6789ABCD - a", 12) == 0);

	s.set_info("a123456789ABC", "t1");
	assert(strncmp(s.get(), "a123456789AB", 12) == 0);
	s.get(); s.get(); s.get(); s.get(); s.get();
	assert(strncmp(s.get(), "6789ABC - t1", 12) == 0);
	assert(strncmp(s.get(), "789ABC - t1 ", 12) == 0);
	assert(strncmp(s.get(), "89ABC - t1 -", 12) == 0);
	assert(strncmp(s.get(), "9ABC - t1 - ", 12) == 0);
	assert(strncmp(s.get(), "ABC - t1 - a", 12) == 0);
}

static void test_convert() {
	char buf[100];

	utf_convert("qwertyuiop[]\asdfghjkl;'zxcvbnm,.//", buf, sizeof(buf));
	assert(strcmp(buf, "qwertyuiop[]\asdfghjkl;'zxcvbnm,.//") == 0);

	utf_convert("QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?", buf, sizeof(buf));
	assert(strcmp(buf, "QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?") == 0);

	utf_convert("`1234567890-=~!@#$%^&*()_+", buf, sizeof(buf));
	assert(strcmp(buf, "`1234567890-=~!@#$%^&*()_+") == 0);

	utf_convert("aaaaaa", buf, 3);
	assert(strcmp(buf, "aa") == 0); // fills the size exactly, including the terminator

	utf_convert("ÄÅÇÈ", buf, sizeof(buf));
	assert(strcmp(buf, "AACE") == 0);

	utf_convert("ĚěĜĝ", buf, sizeof(buf));
	assert(strcmp(buf, "EeGg") == 0);

	utf_convert("ŘřŚś", buf, sizeof(buf));
	assert(strcmp(buf, "RrSs") == 0);

	utf_convert("ƝƞƠơƤƥ", buf, sizeof(buf));
	assert(strcmp(buf, "NnOoPp") == 0);

	utf_convert("ǍǎǏǸǹǺǻǾǿ", buf, sizeof(buf));
	assert(strcmp(buf, "AaINnAaOo") == 0);

	utf_convert("ȰȱȲȳȽȾȿ", buf, sizeof(buf));
	assert(strcmp(buf, "OoYyLTs") == 0);

	utf_convert("Glāžšķūņu rūķīši", buf, sizeof(buf));
	assert(strcmp(buf, "Glazskunu rukisi") == 0);

	// drop unrecognized 2-byte UTF8 chars
	utf_convert("0ȸ1ʖ2", buf, sizeof(buf));
	assert(strcmp(buf, "012") == 0);

	// string ends after 1st byte of 2-byte UTF8 char
	utf_convert("zz\xc6", buf, sizeof(buf));
	assert(strcmp(buf, "zz") == 0);

	// --- new in 6.1.6: 3/4-byte sequences and stray continuations drop ---

	// 3-byte sequence (em dash U+2014) is consumed, not leaked raw
	utf_convert("A\xe2\x80\x94" "B", buf, sizeof(buf));
	assert(strcmp(buf, "AB") == 0);

	// curly apostrophe U+2019 inside a word
	utf_convert("Don\xe2\x80\x99t", buf, sizeof(buf));
	assert(strcmp(buf, "Dont") == 0);

	// 4-byte sequence (emoji U+1F3B5)
	utf_convert("X\xf0\x9f\x8e\xb5Y", buf, sizeof(buf));
	assert(strcmp(buf, "XY") == 0);

	// stray continuation byte
	utf_convert("a\x80z", buf, sizeof(buf));
	assert(strcmp(buf, "az") == 0);

	// truncated 3-byte / 4-byte sequences at end of string
	utf_convert("zz\xe2\x80", buf, sizeof(buf));
	assert(strcmp(buf, "zz") == 0);
	utf_convert("zz\xf0\x9f\x8e", buf, sizeof(buf));
	assert(strcmp(buf, "zz") == 0);

	// --- new in 6.1.7: invalid UTF-8 (e.g. Latin-1 metadata) consumes ONE
	// byte, never the ASCII after it; Latin-1 letters map via the tables ---
	utf_convert("Beyonc\xE9 Live", buf, sizeof(buf));
	assert(strcmp(buf, "Beyonce Live") == 0);
	utf_convert("H\xE4r kommer", buf, sizeof(buf));
	assert(strcmp(buf, "Har kommer") == 0);
	utf_convert("H\xE5kan Hellstr\xF6m", buf, sizeof(buf));
	assert(strcmp(buf, "Hakan Hellstrom") == 0);
	utf_convert("M\xF8tley Cr\xFC" "e", buf, sizeof(buf));
	assert(strcmp(buf, "Motley Crue") == 0);
	utf_convert("\xC4rlig", buf, sizeof(buf));
	assert(strcmp(buf, "Arlig") == 0);

	// a lead byte followed by plain ASCII is not a sequence
	utf_convert("a\xC3" "b", buf, sizeof(buf));
	assert(strcmp(buf, "aAb") == 0);

	// the same Swedish words in proper UTF-8 still work
	utf_convert("H\xC3\xA5kan Hellstr\xC3\xB6m", buf, sizeof(buf));
	assert(strcmp(buf, "Hakan Hellstrom") == 0);

	// a UTF-8 lead byte cut off by the end of the string (metadata is
	// truncated by bytes) is dropped, not turned into a Latin-1 letter
	utf_convert("zz\xC3", buf, sizeof(buf));
	assert(strcmp(buf, "zz") == 0);
	utf_convert("Hellstr\xC3", buf, sizeof(buf));
	assert(strcmp(buf, "Hellstr") == 0);
	utf_convert("Don\xE2", buf, sizeof(buf));
	assert(strcmp(buf, "Don") == 0);
	utf_convert("X\xF0", buf, sizeof(buf));
	assert(strcmp(buf, "X") == 0);
	// ...but 0xF8-0xFF can't start UTF-8, so a final Latin-1 u-umlaut stays
	utf_convert("Cr\xFC", buf, sizeof(buf));
	assert(strcmp(buf, "Cru") == 0);
}

static bool frameEq(const unsigned char *a, const unsigned char *b) {
	return memcmp(a, b, 8) == 0;
}

static void test_buttons() {
	// frame-level decode of 0x3C0: byte 0 must be 0x80 (event)
	struct { unsigned char b1, b2; ibus::Button expect; } cases[] = {
		{ 0x59, 0x00, ibus::NXT },
		{ 0x45, 0x00, ibus::SEEK_PLUS_LONG },
		{ 0x46, 0x00, ibus::SEEK_MINUS_LONG },
		{ 0x84, 0x00, ibus::SEEK_MIDDLE_LONG },
		{ 0x88, 0x00, ibus::SEEK_MIDDLE_EXTRA_LONG },
		{ 0x76, 0x00, ibus::RANDOM },
		{ 0xB1, 0x00, ibus::PAUSE_ON },
		{ 0xB0, 0x00, ibus::PAUSE_OFF },
		{ 0x35, 0x00, ibus::TRACK_PLUS },
		{ 0x36, 0x00, ibus::TRACK_MINUS },
		{ 0x68, 0x01, ibus::IHU1 }, { 0x68, 0x02, ibus::IHU2 },
		{ 0x68, 0x03, ibus::IHU3 }, { 0x68, 0x04, ibus::IHU4 },
		{ 0x68, 0x05, ibus::IHU5 }, { 0x68, 0x06, ibus::IHU6 },
		{ 0x68, 0x00, ibus::NONE }, { 0x68, 0x07, ibus::NONE },
		{ 0x24, 0x00, ibus::NONE }, // CDC-on is a command, not a button
		{ 0x14, 0x00, ibus::NONE },
		{ 0x00, 0x00, ibus::NONE }, { 0xFF, 0x00, ibus::NONE },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		unsigned char f[8] = { 0x80, cases[i].b1, cases[i].b2, 0, 0, 0, 0, 0 };
		assert(ibus::decodeButton(f) == cases[i].expect);
		f[0] = 0x00; // periodic, non-event frame: never a button
		assert(ibus::decodeButton(f) == ibus::NONE);
	}

	// The in-car interface contract (docs/USAGE_v6.md) — every button.
	assert(ibus::buttonAction(ibus::NXT) == ibus::ACT_PLAY_PAUSE);
	assert(ibus::buttonAction(ibus::TRACK_PLUS) == ibus::ACT_NEXT);
	assert(ibus::buttonAction(ibus::TRACK_MINUS) == ibus::ACT_PREV);
	assert(ibus::buttonAction(ibus::IHU1) == ibus::ACT_PAIR);
	assert(ibus::buttonAction(ibus::SEEK_MIDDLE_EXTRA_LONG) == ibus::ACT_PAIR);
	assert(ibus::buttonAction(ibus::IHU3) == ibus::ACT_RECONNECT);
	assert(ibus::buttonAction(ibus::IHU4) == ibus::ACT_VOLUME_DOWN);
	assert(ibus::buttonAction(ibus::IHU5) == ibus::ACT_VOLUME_UP);
	assert(ibus::buttonAction(ibus::IHU6) == ibus::ACT_DISCONNECT);
	// decoded but deliberately unassigned (free for additive features)
	assert(ibus::buttonAction(ibus::NONE) == ibus::ACT_NONE);
	assert(ibus::buttonAction(ibus::IHU2) == ibus::ACT_NONE);
	assert(ibus::buttonAction(ibus::SEEK_PLUS_LONG) == ibus::ACT_NONE);
	assert(ibus::buttonAction(ibus::SEEK_MINUS_LONG) == ibus::ACT_NONE);
	assert(ibus::buttonAction(ibus::SEEK_MIDDLE_LONG) == ibus::ACT_NONE);
	assert(ibus::buttonAction(ibus::RANDOM) == ibus::ACT_NONE);
	assert(ibus::buttonAction(ibus::PAUSE_ON) == ibus::ACT_NONE);
	assert(ibus::buttonAction(ibus::PAUSE_OFF) == ibus::ACT_NONE);
}

static void test_cdc_command() {
	const unsigned char on[8]    = { 0x80, 0x24, 0, 0, 0, 0, 0, 0 };
	const unsigned char off[8]   = { 0x80, 0x14, 0, 0, 0, 0, 0, 0 };
	const unsigned char noev[8]  = { 0x00, 0x24, 0, 0, 0, 0, 0, 0 };
	const unsigned char other[8] = { 0x80, 0x59, 0, 0, 0, 0, 0, 0 };
	assert(ibus::decodeCdcCommand(on) == ibus::CDC_CMD_ON);
	assert(ibus::decodeCdcCommand(off) == ibus::CDC_CMD_OFF);
	assert(ibus::decodeCdcCommand(noev) == ibus::CDC_CMD_NONE);
	assert(ibus::decodeCdcCommand(other) == ibus::CDC_CMD_NONE);
}

static void test_node_status() {
	// 0x6A1 poll: low nibble of byte 3 selects the reply; high nibble ignored
	for (int hi = 0; hi < 16; hi++) {
		for (int lo = 0; lo < 16; lo++) {
			unsigned char poll[8] = { 0, 0, 0, (unsigned char)(hi << 4 | lo), 0, 0, 0, 0 };
			ibus::NodeReply r = ibus::nodeReplyFor(poll);
			if (lo == 0x3)
				assert(r == ibus::NODE_REPLY_POWER_ON);
			else if (lo == 0x2)
				assert(r == ibus::NODE_REPLY_ACTIVE);
			else if (lo == 0x8)
				assert(r == ibus::NODE_REPLY_POWER_DOWN);
			else
				assert(r == ibus::NODE_REPLY_NONE);
		}
	}
	assert(ibus::nodeStatusFrames(ibus::NODE_REPLY_NONE) == NULL);

	// 0x6A2 reply sequences, byte-exact (docs/IBUS_PROTOCOL.md). FROZEN:
	// a wrong first byte lit airbag/MIL warnings on a 2004 9-5.
	static const unsigned char powerOn[4][8] = {
		{ 0x32, 0x00, 0x00, 0x03, 0x01, 0x02, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00 } };
	static const unsigned char active[4][8] = {
		{ 0x32, 0x00, 0x00, 0x16, 0x01, 0x02, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00 } };
	static const unsigned char powerDown[4][8] = {
		{ 0x32, 0x00, 0x00, 0x19, 0x01, 0x00, 0x00, 0x00 },
		{ 0x42, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 },
		{ 0x52, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 },
		{ 0x62, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00 } };
	assert(ibus::NODE_STATUS_FRAMES == 4);
	for (int i = 0; i < 4; i++) {
		assert(frameEq(ibus::nodeStatusFrames(ibus::NODE_REPLY_POWER_ON)[i], powerOn[i]));
		assert(frameEq(ibus::nodeStatusFrames(ibus::NODE_REPLY_ACTIVE)[i], active[i]));
		assert(frameEq(ibus::nodeStatusFrames(ibus::NODE_REPLY_POWER_DOWN)[i], powerDown[i]));
	}
}

static void test_cdc_status() {
	unsigned char f[8];
	// command-induced resend while active (event == remote in practice)
	static const unsigned char activeEvent[8] = { 0xE0, 0xFF, 0x3F, 0x41, 0xFF, 0xFF, 0xFF, 0xD0 };
	ibus::buildCdcStatus(f, true, true, true);
	assert(frameEq(f, activeEvent));
	// base-time send while idle
	static const unsigned char idleBase[8] = { 0x20, 0x00, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xD0 };
	ibus::buildCdcStatus(f, false, false, false);
	assert(frameEq(f, idleBase));
	// byte 0 bit layout: bit7 event, bit6 remote, bit5 presence-valid (always)
	ibus::buildCdcStatus(f, true, false, true);
	assert(f[0] == 0xA0);
	ibus::buildCdcStatus(f, false, true, true);
	assert(f[0] == 0x60);
	ibus::buildCdcStatus(f, false, false, true);
	assert(f[0] == 0x20 && f[7] == 0xD0); // 0xD0 = "married to the car"
}

static void test_sid_text() {
	// bytes 0..1 of each frame are sequence/row markers, never touched
	unsigned char g[3][8] = {
		{ 0x42, 0x96, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE },
		{ 0x01, 0x96, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE },
		{ 0x00, 0x96, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE } };

	ibus::formatSidText(g, "ABCDEFGHIJKL", false);
	static const unsigned char full[3][8] = {
		{ 0x42, 0x96, 0x02, 'A', 'B', 'C', 'D', 'E' },
		{ 0x01, 0x96, 0x02, 'F', 'G', 'H', 'I', 'J' },
		{ 0x00, 0x96, 0x02, 'K', 'L', 0, 0, 0 } };
	for (int i = 0; i < 3; i++) assert(frameEq(g[i], full[i]));

	// longer than 12 characters: cut at 12
	ibus::formatSidText(g, "ABCDEFGHIJKLMNOP", false);
	for (int i = 0; i < 3; i++) assert(frameEq(g[i], full[i]));

	// short text, event write: 0x82, rest zero-padded
	ibus::formatSidText(g, "PAIRING", true);
	static const unsigned char pairing[3][8] = {
		{ 0x42, 0x96, 0x82, 'P', 'A', 'I', 'R', 'I' },
		{ 0x01, 0x96, 0x82, 'N', 'G', 0, 0, 0 },
		{ 0x00, 0x96, 0x82, 0, 0, 0, 0, 0 } };
	for (int i = 0; i < 3; i++) assert(frameEq(g[i], pairing[i]));

	ibus::formatSidText(g, "", false);
	for (int i = 0; i < 3; i++) {
		assert(g[i][2] == 0x02);
		for (int j = 3; j < 8; j++) assert(g[i][j] == 0);
	}
}

static void test_rn52_version() {
	char v[8];
	assert(rn52::parseVersion("RN52-I Ver 1.16 (c) Microchip Technology Inc.\r\n", v, sizeof(v)));
	assert(strcmp(v, "1.16") == 0);
	assert(rn52::parseVersion("Ver 1.10 04/04/13\r\n", v, sizeof(v)));
	assert(strcmp(v, "1.10") == 0);
	assert(rn52::parseVersion("\xff\xfe Ver 1.16", v, sizeof(v))); // high bytes: no UB
	assert(strcmp(v, "1.16") == 0);
	// the token starts at the digit right before the first '.', so a
	// multi-digit major is cut ("12.3" -> "2.3") - fine, RN52 versions are
	// 1.xx; and a token longer than the buffer is truncated, terminated
	assert(rn52::parseVersion("Ver 12.345.678", v, sizeof(v)));
	assert(strcmp(v, "2.345.6") == 0);
	char small[3];
	assert(rn52::parseVersion("Ver 1.16", small, sizeof(small)));
	assert(strcmp(small, "1.") == 0);
	// no digit.digit token: false, output untouched
	strcpy(v, "?");
	assert(!rn52::parseVersion("(c) Microchip Technology Inc.\r\n", v, sizeof(v)));
	assert(!rn52::parseVersion("Ver 1. 16", v, sizeof(v)));
	assert(!rn52::parseVersion("", v, sizeof(v)));
	assert(strcmp(v, "?") == 0);
	assert(!rn52::parseVersion("Ver 1.16", v, 0));
}

static void test_rn52_q() {
	bool a2dp = false, track = false;
	// 2nd hex digit bit 2 = A2DP connected
	assert(rn52::decodeQResponse("0400\r\n", &a2dp, &track) && a2dp && !track);
	assert(rn52::decodeQResponse("0C00\r\n", &a2dp, &track) && a2dp && !track);
	assert(rn52::decodeQResponse("0c00\r\n", &a2dp, &track) && a2dp && !track); // lowercase
	assert(rn52::decodeQResponse("0B21\r\n", &a2dp, &track) && !a2dp && !track);
	// 1st hex digit bit 1 = track-change event
	assert(rn52::decodeQResponse("2000\r\n", &a2dp, &track) && !a2dp && track);
	assert(rn52::decodeQResponse("2400\r\n", &a2dp, &track) && a2dp && track);
	assert(rn52::decodeQResponse("0000\r\n", &a2dp, &track) && !a2dp && !track);
	// invalid: wrong length or non-hex
	assert(!rn52::decodeQResponse("0400", &a2dp, &track));
	assert(!rn52::decodeQResponse("040\r\n", &a2dp, &track));
	assert(!rn52::decodeQResponse("04000\r\n", &a2dp, &track));
	assert(!rn52::decodeQResponse("04G0\r\n", &a2dp, &track));
	assert(!rn52::decodeQResponse("", &a2dp, &track));
}

int main() {
	test_scroller();
	test_convert();
	test_buttons();
	test_cdc_command();
	test_node_status();
	test_cdc_status();
	test_sid_text();
	test_rn52_version();
	test_rn52_q();
	printf("ALL HOST TESTS PASSED\n");
	return 0;
}
