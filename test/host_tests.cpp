/* Host unit tests for the platform-independent firmware logic.
 *
 * Test bodies are the previously dormant blocks from Scroller.cpp and
 * utf_convert.cpp (kept verbatim), plus cases for the 6.1.6 UTF-8 fix.
 * Built and run by CI on every push (both char signednesses; the target
 * uses -funsigned-char).
 *
 * Build: g++ -funsigned-char -I test/stubs -I . \
 *            test/host_tests.cpp Scroller.cpp utf_convert.cpp -o host_tests
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "Scroller.h"
#include "utf_convert.h"

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
}

int main() {
	test_scroller();
	test_convert();
	printf("ALL HOST TESTS PASSED\n");
	return 0;
}
