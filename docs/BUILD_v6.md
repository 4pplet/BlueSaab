# Building & flashing v6 firmware

Status: documented from the repo as-is; the "verify build with a current
toolchain" TODO item is still open — expect to update this file the first
time a fresh clone is built.

## Toolchain

- `arm-none-eabi-gcc` (GNU Arm Embedded). The code is C++03-era mbed OS 2 —
  recent GCC versions should compile it but this is **unverified**; if a
  modern toolchain fights you, GCC 6–9 era is the safest bet.
- `make` — the root [Makefile](../Makefile) is a self-contained mbed-exported
  GCC ARM makefile (builds into `BUILD/`, target NUCLEO_F103RB /
  STM32F103RB, Cortex-M3).
- No mbed CLI, no internet: `mbed/` and `mbed-rtos/` libraries are vendored
  in-repo. The `mbed.bld` / `mbed-rtos.lib` files point at mbed.org URLs
  that **no longer exist** (Arm killed Mbed in 2024) — never try to update
  these libraries; they are frozen.

## Build

```sh
make
```

Output: `BUILD/` (gitignored) — final artifact is a `.bin`/`.elf` for the
STM32F103RB.

## Flash

Two options, both on the board's headers:

- **SWD/JTAG**: 10-pin 1.27 mm JTAG header; an ST-Link (or the ST-Link half
  of a Nucleo-F103RB board) + `st-flash`/OpenOCD/pyOCD works. The
  `.gitignore` references an old pyOCD launch config — pyOCD is the
  historically used flasher.
- **Serial bootloader**: BOOT0 button + FTDI header (USART1) with
  `stm32flash`, if no SWD probe is at hand.

## Debug console

UART2 header (PA2/PA3), **115200 8N1** — boot banner plus single-character
commands (see [USAGE_v6.md](USAGE_v6.md#debug-serial-console)).

## Compile-time options

- `SID_TEXT_CONTROL_ENABLED` in [SidResource.h](../SidResource.h) — set to
  `0` to build without SID text support (e.g. for nav-equipped cars).
