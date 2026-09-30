#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# All linters in one go (CI runs this too):
#   - tools/static-analysis.sh  (cppcheck + Clang static analyzer, firmware)
#   - shellcheck                (tools/*.sh)
#   - ruff                      (tools/*.py)
#   - actionlint                (.github/workflows, skipped if not installed)
# Exits non-zero if anything reports a finding.

set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
fail=0

run() {
	local name=$1
	shift
	echo "== $name"
	if "$@"; then echo "   ok"; else echo "   FAILED: $name"; fail=1; fi
}

run "static analysis" tools/static-analysis.sh
run "shellcheck" shellcheck tools/*.sh
if command -v ruff >/dev/null; then
	run "ruff" ruff check --select E,F,W,B,UP --target-version py39 tools/
else
	run "ruff" pipx run ruff==0.16.9 check --select E,F,W,B,UP --target-version py39 tools/
fi
if command -v actionlint >/dev/null; then
	run "actionlint" actionlint
else
	echo "== actionlint: not installed, skipped (brew install actionlint)"
fi

if [ $fail = 0 ]; then echo "lint: all clean"; else echo "lint: FINDINGS"; fi
exit $fail
