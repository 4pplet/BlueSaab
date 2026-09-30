#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Logging serial console for the BlueSaab v6 debug header (UART2).

Every key you press is sent immediately (the firmware takes single-character
commands: V I C D P N R A B d u E H). Everything the board prints is shown
and appended, timestamped, to the log file together with the keys you sent.
Quit with Ctrl-] or Ctrl-C.

Usage: bench_console.py <serial-port> <logfile> [baud]
Needs pyserial (pip3 install pyserial).
"""
import os
import sys
import threading
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing: pip3 install pyserial")


def stamp():
    return time.strftime("%H:%M:%S ")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    port, logpath = sys.argv[1], sys.argv[2]
    baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

    ser = serial.Serial(port, baud, timeout=0.1)
    log = open(logpath, "a", buffering=1)
    log.write(stamp() + f"=== console opened {port} @{baud} ===\n")
    stop = threading.Event()

    def rx():
        buf = b""
        while not stop.is_set():
            try:
                data = ser.read(256)
            except serial.SerialException as e:
                log.write(stamp() + f"=== serial error: {e} ===\n")
                sys.stdout.write(f"\r\n[serial error: {e}]\r\n")
                stop.set()
                return
            if not data:
                continue
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").rstrip("\r")
                log.write(stamp() + text + "\n")

    reader = threading.Thread(target=rx, daemon=True)
    reader.start()

    fd = sys.stdin.fileno()
    interactive = os.isatty(fd)
    old = None
    if interactive:
        import termios
        import tty
        old = termios.tcgetattr(fd)
        tty.setcbreak(fd)  # single keys, no echo; output processing intact
        sys.stdout.write(f"Connected to {port} @{baud}, logging to {logpath}\n"
                         "Keys are sent as typed. Ctrl-] or Ctrl-C quits.\n")
        sys.stdout.flush()
    try:
        while not stop.is_set():
            ch = os.read(fd, 1)
            if not ch or ch == b"\x1d":  # EOF or Ctrl-]
                break
            ser.write(ch)
            log.write(stamp() + f">>> sent {ch.decode('ascii', 'replace')!r}\n")
    except KeyboardInterrupt:
        pass
    finally:
        if old is not None:
            import termios
            termios.tcsetattr(fd, termios.TCSADRAIN, old)
        time.sleep(0.3)  # let the reader catch the last output
        stop.set()
        reader.join(1)
        ser.close()
        log.write(stamp() + "=== console closed ===\n")
        log.close()


if __name__ == "__main__":
    main()
