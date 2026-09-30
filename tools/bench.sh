#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Bench-session helper for BlueSaab v6 — see docs/BENCH_SESSION.md.
# Works with macOS's stock bash 3.2.
#
#   tools/bench.sh check                 tools, GitHub login, serial ports
#   tools/bench.sh ports                 list USB-serial ports
#   tools/bench.sh fetch                 download the pinned RC, verify SHA-256
#   tools/bench.sh backup  <port>        read the unit's flash (never overwrites)
#   tools/bench.sh flash   <port> [bin]  flash the verified RC, or another .bin
#                                        (e.g. a stack-monitor build); needs a backup
#   tools/bench.sh console <port> [tag]  logging console on the UART2 header
#   tools/bench.sh summary               extract the facts to record from logs
#
# Everything is written under bench/ (gitignored).

set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BENCH="$ROOT/bench"
REPO="4pplet/BlueSaab"
# shellcheck source=rc.env
. "$ROOT/tools/rc.env"
RC_DIR="$BENCH/rc-$RC_VERSION"
RC_BIN="$RC_DIR/BlueSaab.bin"

die()  { echo "error: $*" >&2; exit 1; }
say()  { echo "==> $*"; }
sha()  { if command -v shasum >/dev/null; then shasum -a 256 "$1" | cut -d' ' -f1; else sha256sum "$1" | cut -d' ' -f1; fi; }
ts()   { date +%Y%m%d-%H%M%S; }
need() { command -v "$1" >/dev/null || die "$1 not found — $2"; }

pause_for_bootloader() {
	echo
	echo "Put the board in the ROM bootloader now:"
	echo "  hold BOOT0, press and release RESET, release BOOT0."
	echo "(FTDI header: adapter GND→pin 1, TXD→pin 4, RXD→pin 5, 3.3 V logic)"
	printf "Press Enter when done... "
	read -r _
}

check_port() {
	[ -n "${1:-}" ] || die "no serial port given — run: tools/bench.sh ports"
	[ -e "$1" ] || die "$1 does not exist — run: tools/bench.sh ports"
}

cmd_ports() {
	local found=0 p
	for p in /dev/cu.usbserial* /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial* /dev/ttyUSB* /dev/ttyACM*; do
		[ -e "$p" ] || continue
		echo "  $p"
		found=1
	done
	[ $found = 1 ] || echo "  (no USB-serial adapter found — plug one in)"
	echo "On macOS use the /dev/cu.* name, not /dev/tty.*."
}

cmd_check() {
	local ok=1
	say "Tools"
	for t in gh stm32flash python3 strings; do
		if command -v "$t" >/dev/null; then echo "  ok      $t"; else echo "  MISSING $t"; ok=0; fi
	done
	command -v stm32flash >/dev/null || echo "          -> brew install stm32flash"
	if python3 -c "import serial" 2>/dev/null; then echo "  ok      pyserial"; else echo "  MISSING pyserial -> pip3 install pyserial"; ok=0; fi
	say "GitHub"
	if gh auth status >/dev/null 2>&1; then echo "  ok      gh logged in"; else echo "  MISSING gh login -> gh auth login"; ok=0; fi
	say "Release candidate: $RC_VERSION (commit $RC_COMMIT, CI run $RC_RUN)"
	if [ -f "$RC_BIN" ] && [ "$(sha "$RC_BIN")" = "$RC_BIN_SHA256" ]; then
		echo "  ok      $RC_BIN (hash verified)"
	else
		echo "  not yet fetched -> tools/bench.sh fetch"
	fi
	say "Serial ports"
	cmd_ports
	[ $ok = 1 ] && say "Ready." || say "Fix the MISSING items above first."
}

cmd_fetch() {
	need gh "brew install gh && gh auth login"
	mkdir -p "$RC_DIR"
	if [ ! -f "$RC_BIN" ]; then
		say "Downloading artifact of CI run $RC_RUN"
		local tmp="$BENCH/.fetch-$$"
		rm -rf "$tmp"
		if ! gh run download "$RC_RUN" --repo "$REPO" -p 'BlueSaab-*' -D "$tmp"; then
			rm -rf "$tmp"
			die "download failed. If the artifact expired, re-run CI on commit $RC_COMMIT (builds are reproducible) and update RC_RUN in tools/rc.env"
		fi
		cp "$tmp"/*/BlueSaab.* "$tmp"/*/SHA256SUMS "$RC_DIR"/
		rm -rf "$tmp"
	fi
	local got
	got=$(sha "$RC_BIN")
	if [ "$got" != "$RC_BIN_SHA256" ]; then
		die "SHA-256 mismatch for $RC_BIN
  expected $RC_BIN_SHA256
  got      $got
Do NOT flash this file."
	fi
	say "Verified: $RC_BIN"
	echo "  SHA-256 $got"
	echo "  $(strings "$RC_BIN" | grep 'Firmware version' | head -1)"
}

cmd_backup() {
	local port=${1:-}
	check_port "$port"
	need stm32flash "brew install stm32flash"
	mkdir -p "$BENCH"
	local out="$BENCH/backup-$(ts).bin"
	pause_for_bootloader
	say "Reading 128 KB of flash to $out"
	stm32flash -r "$out" "$port"
	[ -s "$out" ] || die "backup is empty"
	say "Backup saved — keep this file: it is the unit's original firmware"
	echo "  SHA-256 $(sha "$out")"
	echo "  size    $(wc -c <"$out" | tr -d ' ') bytes"
	local v
	v=$(strings "$out" | grep -i 'firmware version' | head -1 || true)
	echo "  version ${v:-(no version string found — firmware older than the banner, or blank chip)}"
}

cmd_flash() {
	local port=${1:-} bin=${2:-}
	check_port "$port"
	need stm32flash "brew install stm32flash"
	ls "$BENCH"/backup-*.bin >/dev/null 2>&1 || die "no backup in $BENCH — run tools/bench.sh backup $port first"
	if [ -z "$bin" ]; then
		bin=$RC_BIN
		[ -f "$bin" ] || die "RC not fetched — run tools/bench.sh fetch"
		[ "$(sha "$bin")" = "$RC_BIN_SHA256" ] || die "RC binary fails its hash check — run tools/bench.sh fetch"
		say "Flashing the $RC_VERSION release candidate ($RC_BIN_SHA256)"
	else
		[ -f "$bin" ] || die "$bin not found"
		local h v
		h=$(sha "$bin")
		v=$(strings "$bin" | grep 'Firmware version' | head -1 || true)
		if [ "$h" = "$RC_BIN_SHA256" ]; then
			say "Flashing $bin — this IS the pinned release candidate"
		else
			echo "WARNING: $bin is NOT the pinned $RC_VERSION release candidate."
			echo "  SHA-256 $h"
			echo "  ${v:-(no version string)}"
			printf "Type 'yes' to flash it anyway: "
			local answer
			read -r answer
			[ "$answer" = "yes" ] || die "aborted"
		fi
	fi
	pause_for_bootloader
	stm32flash -w "$bin" -v "$port"
	say "Flashed and verified. Press RESET to start it."
	echo "Then: tools/bench.sh console <UART2 port>"
}

cmd_console() {
	local port=${1:-} tag=${2:-session}
	check_port "$port"
	mkdir -p "$BENCH"
	local log="$BENCH/console-$tag-$(ts).log"
	exec python3 "$ROOT/tools/bench_console.py" "$port" "$log"
}

# Last value logged for a pattern across all console logs ("-" if none)
last_match() {
	local v
	v=$(cat "$BENCH"/console-*.log 2>/dev/null | grep -a "$1" | tail -1 | sed "s/^[0-9:]* //; s/.*$1[[:space:]]*//" || true)
	echo "${v:--}"
}

cmd_summary() {
	ls "$BENCH"/console-*.log >/dev/null 2>&1 || die "no console logs in $BENCH yet"
	echo "## Bench findings (from $(ls "$BENCH"/console-*.log | wc -l | tr -d ' ') console log(s))"
	echo
	echo "- Firmware version (latest boot): $(last_match 'Firmware version:')"
	echo "- RN52 version:                   $(last_match 'RN52 version:')"
	echo "- RN52 Bluetooth address:         $(last_match 'BTA=')"
	echo "- CAN TX write failures:          $(last_match 'CAN TX write failures:')"
	echo "- CAN TX dropped:                 $(last_match 'CAN TX dropped (queue full):')"
	echo "- CAN RX FIFO overruns:           $(last_match 'CAN RX FIFO overruns:')"
	echo "- CAN REC / TEC:                  $(last_match 'CAN RX error counter (REC):') / $(last_match 'CAN TX error counter (TEC):')"
	echo "- CAN ESR:                        $(last_match 'CAN ESR:')"
	local b
	for b in "$BENCH"/backup-*.bin; do
		[ -f "$b" ] || continue
		echo "- Backup $(basename "$b"): $(strings "$b" | grep -i 'firmware version' | head -1 || echo 'no version string')"
	done
	echo
	echo "Warnings seen in the logs:"
	cat "$BENCH"/console-*.log | grep -a -i -e "failed" -e "queue full" -e "dropped" -e "error" \
		| grep -a -v -e "CAN TX dropped (queue full): 0" -e "error counter" || echo "  (none)"
}

case "${1:-}" in
	check)   cmd_check ;;
	ports)   cmd_ports ;;
	fetch)   cmd_fetch ;;
	backup)  shift; cmd_backup "$@" ;;
	flash)   shift; cmd_flash "$@" ;;
	console) shift; cmd_console "$@" ;;
	summary) cmd_summary ;;
	*) sed -n '4,16p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
