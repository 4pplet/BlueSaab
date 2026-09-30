#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Static analysis of the firmware sources (the vendored mbed/ and mbed-rtos/
# are excluded). Fails on any finding:
#   - cppcheck: warning/portability checks (style is informational only)
#   - clang-tidy: the Clang static analyzer (path-sensitive: null derefs,
#     uninitialized values, memory errors), compiled exactly like the
#     firmware (ARM target, the Makefile's include paths and defines, the
#     ARM toolchain's own system headers)
# Needs: cppcheck, clang-tidy, arm-none-eabi-g++ on PATH (or set CLANG_TIDY /
# ARM_GXX). Run from anywhere; CI runs it on every push.

set -euo pipefail
cd "$(dirname "$0")/.."

CLANG_TIDY=${CLANG_TIDY:-$(command -v clang-tidy || echo /opt/homebrew/opt/llvm/bin/clang-tidy)}
ARM_GXX=${ARM_GXX:-arm-none-eabi-g++}
SOURCES=(./*.cpp common/*.cpp)
fail=0

echo "== cppcheck $(cppcheck --version | cut -d' ' -f2)"
if ! cppcheck --enable=warning,portability --std=c++03 --platform=unix32 \
	--inline-suppr --error-exitcode=1 --quiet \
	-I . -I common \
	-DSID_TEXT_CONTROL_ENABLED=1 -DCDC_ENTRY_BEEP_ENABLED=1 -DSTACK_MONITOR_ENABLED=0 \
	--suppress=missingInclude --suppress=missingIncludeSystem \
	--template='{file}:{line}: {severity}/{id}: {message}' \
	"${SOURCES[@]}" common/can_api.c; then
	fail=1
fi

echo "== clang-tidy ($("$CLANG_TIDY" --version | grep -o 'version [0-9.]*'))"
SYS=$("$ARM_GXX" -mcpu=cortex-m3 -mthumb -E -x c++ - -v </dev/null 2>&1 \
	| sed -n '/#include <...> search starts here:/,/End of search list./p' \
	| grep '^ /' | sed 's/^ /-isystem /' | tr '\n' ' ')
INC=$(grep '^INCLUDE_PATHS += ' Makefile | sed 's/INCLUDE_PATHS += -I\.\.\//-I/; s/-I$/-I./' | tr '\n' ' ')
DEF=$(grep '^CXX_FLAGS += -D' Makefile | sed 's/CXX_FLAGS += //' | tr '\n' ' ')
# The analyzer's insecureAPI (strcpy/strcat) checks flag bounded-by-
# construction uses, and optin.* flags mbed's own Callback internals.
CHECKS='-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.*,-clang-analyzer-optin.*'
out=$(
	for f in "${SOURCES[@]}"; do
		# shellcheck disable=SC2086  # flag lists must word-split
		"$CLANG_TIDY" --quiet --checks="$CHECKS" "$f" -- \
			--target=arm-none-eabi -mcpu=cortex-m3 -mthumb -std=gnu++98 -funsigned-char \
			-nostdinc -nostdinc++ $SYS $INC $DEF -include mbed_config.h -Wno-everything 2>&1 || true
	done | grep -E '(warning|error):' | grep -v -e '/mbed/' -e '/mbed-rtos/' -e '^mbed' || true
)
if [ -n "$out" ]; then
	echo "$out"
	fail=1
fi

if [ $fail = 0 ]; then echo "static analysis: clean"; else echo "static analysis: FINDINGS"; fi
exit $fail
