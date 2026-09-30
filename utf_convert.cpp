/* Convert UTF8 Latin extended chars to plain ASCII
 *
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

//#include <string.h>
//#include <stdio.h>
//#include <assert.h>

/*
https://en.wikipedia.org/wiki/Latin_script_in_Unicode

UTF8   unicode
c3 80  00C0  ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏ
c3 90  00D0  ÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß
c3 a0  00E0  àáâãäåæçèéêëìíîï
c3 b0  00F0  ðñòóôõö÷øùúûüýþÿ

c4 80  0100  ĀāĂăĄąĆćĈĉĊċČčĎď
c4 90  0110  ĐđĒēĔĕĖėĘęĚěĜĝĞğ
c4 a0  0120  ĠġĢģĤĥĦħĨĩĪīĬĭĮį
c4 b0  0130  İıĲĳĴĵĶķĸĹĺĻļĽľĿ

c5 80  0140  ŀŁłŃńŅņŇňŉŊŋŌōŎŏ
c5 90  0150  ŐőŒœŔŕŖŗŘřŚśŜŝŞş
c5 a0  0160  ŠšŢţŤťŦŧŨũŪūŬŭŮů
c5 b0  0170  ŰűŲųŴŵŶŷŸŹźŻżŽžſ

c6 80  0180  ƀƁƂƃƄƅƆƇƈƉƊƋƌƍƎƏ
c6 90  0190  ƐƑƒƓƔƕƖƗƘƙƚƛƜƝƞƟ
c6 a0  01A0  ƠơƢƣƤƥƦƧƨƩƪƫƬƭƮƯ
c6 b0  01B0  ưƱƲƳƴƵƶƷƸƹƺƻƼƽƾƿ

c7 80  01C0  ǀǁǂǃǄǅǆǇǈǉǊǋǌǍǎǏ
c7 90  01D0  ǐǑǒǓǔǕǖǗǘǙǚǛǜǝǞǟ
c7 a0  01E0  ǠǡǢǣǤǥǦǧǨǩǪǫǬǭǮǯ
c7 b0  01F0  ǰǱǲǳǴǵǶǷǸǹǺǻǼǽǾǿ

c8 80  0200  ȀȁȂȃȄȅȆȇȈȉȊȋȌȍȎȏ
c8 90  0210  ȐȑȒȓȔȕȖȗȘșȚțȜȝȞȟ
c8 a0  0220  ȠȡȢȣȤȥȦȧȨȩȪȫȬȭȮȯ
c8 b0  0230  ȰȱȲȳȴȵȶȷȸȹȺȻȼȽȾȿ
 */

const char *conversion[] = {
		"ÀÁÂÃÄÅÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝàáâãäåçèéêëìíîïðñòóôõöøùúûüýÿ", // c3
		"AAAAAACEEEEIIIIDNOOOOOOUUUUYaaaaaaceeeeiiiidnoooooouuuuyy",
		"ĀāĂăĄąĆćĈĉĊċČčĎďĐđĒēĔĕĖėĘęĚěĜĝĞğĠġĢģĤĥĦħĨĩĪīĬĭĮįİıĴĵĶķĸĹĺĻļĽľĿ", // c4
		"AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiJjKkkLlLlLlL",
		"ŀŁłŃńŅņŇňŉŊŋŌōŎŏŐőŔŕŖŗŘřŚśŜŝŞşŠšŢţŤťŦŧŨũŪūŬŭŮůŰűŲųŴŵŶŷŸŹźŻżŽž", // c5
		"lLlNnNnNnnNnOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZz",
		"ƀƁƂƃƄƅƇƈƉƊƐƑƒƓƕƘƙƚƝƞƠơƤƥƦƫƬƭƮƯưƳƴƵƶ", // c6
		"bBBbbbCcDDEFfGhKklNnOoPpRtTtTUuYyZz",
		"ǍǎǏǐǑǒǓǔǕǖǗǘǙǚǛǜǞǟǠǡǤǥǦǧǨǩǪǫǬǭǴǵǸǹǺǻǾǿ", // c7
		"AaIiOoUuUuUuUuUuAaAaGgGgKkOoOoGgNnAaOo",
		"ȀȁȂȃȄȅȆȇȈȉȊȋȌȍȎȏȐȑȒȓȔȕȖȗȘșȚțȞȟȤȥȦȧȨȩȪȫȬȭȮȯȰȱȲȳȴȵȶȺȻȼȽȾȿ", // c8
		"AaAaEeEeIiIiOoOoRrRrUuUuSsTtHhZzAaEeOoOoOoOoYylntACcLTs",
		0
};

static char find2(const char *cp, const char *from, const char *to) {
	// Assuming the first byte of all chars is the same, so compare only the second byte
	char c = cp[1];
	while (*from) {
		from++;
		if (*from == c) {
			return *to;
		}
		from++;
		to++;
	}
	return 0;
}

static inline bool is_cont(char b) { return ((unsigned char)b & 0xC0) == 0x80; }

// Transliterate one code point given as a UTF-8 lead byte + one trail byte
// (2-byte sequences only); 0 if not in the tables.
static char lookup2(char lead, char trail) {
	const char cp[2] = { lead, trail };
	for (const char **p = conversion; *p; p += 2) {
		if (lead == *p[0])
			return find2(cp, p[0], p[1]);
	}
	return 0;
}

void utf_convert(const char *from, char *to, int size) {
	char *to_last = to + size - 1;
	while (*from && to < to_last) {
		unsigned char b = (unsigned char)*from;
		int len = (b < 0x80) ? 1
		        : ((b & 0xE0) == 0xC0) ? 2
		        : ((b & 0xF0) == 0xE0) ? 3
		        : ((b & 0xF8) == 0xF0) ? 4 : 0;
		// A multi-byte sequence counts only if every trail byte is a
		// continuation byte (10xxxxxx). NUL is not, so this also stops at a
		// string end cut mid-sequence without reading past the terminator.
		bool valid = len > 1;
		for (int i = 1; valid && i < len; i++)
			valid = is_cont(from[i]);
		if (len == 1) {
			*to++ = *from++;
		} else if (valid) {
			if (len == 2) {
				char c = lookup2(from[0], from[1]);
				if (c)
					*to++ = c;
			} // 3/4-byte (quotes, dashes, emoji): dropped
			from += len;
		} else {
			// Not UTF-8: stray continuation, 0xF8-0xFF, or a lead byte whose
			// trail is missing/ASCII. Assume Latin-1 (U+0080-U+00FF == UTF-8
			// C2/C3 + (b & 0x3F | 0x80)) and consume exactly ONE byte, so the
			// following ASCII is never swallowed.
			if (len > 1 && from[1] == 0) {
				// A UTF-8 lead byte cut off by the end of the string - the RN52
				// and copy_text() truncate metadata by bytes. Drop it rather
				// than guessing a Latin-1 letter. (A Latin-1 string ending in
				// a letter from 0xC0-0xF7 loses that last letter.)
				from++;
			} else if (len > 1 && is_cont(from[1])) {
				// Broken/truncated UTF-8 (lead + some trail bytes): drop it
				int n = 1;
				while (n < len && is_cont(from[n]))
					n++;
				from += n;
			} else {
				char c = (b >= 0xC0) ? lookup2((char)0xC3, (char)(0x80 | (b & 0x3F))) : 0;
				if (c)
					*to++ = c;
				from++;
			}
		}
	}
	*to = 0; // terminate the string
}

/*
void test_convert() {
	printf("start\n");

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

	printf("end\n");
}
*/




