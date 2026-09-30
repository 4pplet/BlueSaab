# Flashing the BlueSaab v6 — howto

Two supported paths. **Method A (serial bootloader) is the primary way —
it's how v6 units have historically been flashed** and needs nothing but a
USB-serial adapter. Method B (SWD/ST-Link) is the power-user alternative:
worth setting up if you want on-chip debugging or belt-and-braces
backup/restore.

The MicroUSB jack is **not** a flashing path — the STM32F103's ROM bootloader
is serial-only (no USB DFU), and the firmware doesn't use USB data. It is,
however, a convenient bench **power** input (5 V via diode D2 → the 3.3 V
regulator). Plugged into a computer it may show up as an unrecognized USB
device (D+ has a pull-up but nothing answers); a phone charger avoids that.

Get a firmware binary first: download the CI artifact or build it per
[BUILD_v6.md](BUILD_v6.md) (`BlueSaab.bin` / `.elf`).

**Shortcut:** `tools/bench.sh` automates Method A for the pinned release
candidate — `fetch` (download + SHA-256 check), `backup <port>`,
`flash <port>` (refuses without a backup or with a hash mismatch) and
`console <port>` (logging console). Session walkthrough:
[BENCH_SESSION.md](BENCH_SESSION.md).

## Method A — ROM serial bootloader (primary, historically used)

**Hardware:** the board's 6-pin FTDI header is laid out for the **FTDI
TTL-232R-3V3** cable (the schematic labels the pins BLACK/BROWN/RED/ORANGE/
YELLOW/GREEN, that cable's wire colors). That cable's red wire is 5 V from
USB, so it also **powers the board** — no other supply needed.

A generic USB-serial breakout (FT232RL/CP2102) + jumper wires works too:

| Adapter | FTDI header pin |
| --- | --- |
| GND | pin 1 (black) |
| TXD | pin 4 (orange, board RX — USART1_RX) |
| RXD | pin 5 (yellow, board TX — USART1_TX) |
| 5 V out (only if it really is 5 V) | pin 3 (red) |

**Keep the adapter at 3.3 V logic.** The debug console pins (PA2/PA3) are not
5 V tolerant, and it's easiest to use one adapter setting for both headers.

**Power caveat for generic adapters:** breakouts with a 3.3 V/5 V jumper
usually output the *jumper* voltage on their VCC pin. At 3.3 V that is too
little to run the board through diode D3 and the regulator. With such an
adapter, leave pin 3 unconnected and power the board from the MicroUSB jack
instead.

The on-board BOOT0 and RESET push-buttons do the bootloader dance; no
soldering.

**Software:** `stm32flash` (`brew install stm32flash`), or if you prefer a
GUI: **STM32CubeProgrammer** (ST's official free macOS app) supports the same
UART bootloader — pick the serial port, connect after the BOOT0 dance, open
the `.bin` at address `0x08000000`, program+verify. (This is likely the
"user-friendly Mac app" used historically.)

```sh
# Enter the ROM bootloader: hold BOOT0, press+release RESET, release BOOT0.
# Then (adjust the serial device name):
stm32flash -r backup_v6_unit.bin /dev/tty.usbserial-XXXX   # backup — do this!
stm32flash -w BlueSaab.bin -v /dev/tty.usbserial-XXXX      # write + verify
# Press RESET to run the new firmware.
```

The backup is the only copy of the unit's original firmware, so keep it. It
also tells you which firmware the unit had, even without a serial console:
`strings backup_v6_unit.bin | grep -i "firmware version"`.

Note: the ROM bootloader talks on **USART1**, which is what the FTDI header
carries (the UART2 header is the debug console — wrong port for flashing).

## Method B — SWD with an ST-Link (debugging + robust backup)

**Hardware:** ST-Link v2 (clone dongles are fine) or the ST-Link end of any
Nucleo board (remove its jumpers to use it as a standalone probe), plus a
10-pin 1.27 mm ribbon or 4 jumper wires.

**Hookup:** the board's shrouded, keyed 10-pin 1.27 mm header is the standard
ARM Cortex debug pinout: pin 1 = VCC (target sense), 2 = SWDIO, 4 = SWCLK,
3/5/9 = GND, 10 = RESET. With jumper wires you need SWDIO, SWCLK, GND, VCC.
Power the board normally (12 V, FTDI 5 V or MicroUSB) — the probe only
*senses* VCC.

**Software:** OpenOCD (`brew install openocd`).

```sh
# 1. Back up the unit's current firmware (do this once, keep the file!)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "init; reset halt; flash read_bank 0 backup_v6_unit.bin; exit"

# 2. Flash + verify + reboot
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program BlueSaab.elf verify reset exit"
```

Restore the backup later, if ever needed:

```sh
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program backup_v6_unit.bin 0x08000000 verify reset exit"
```

## Verify after flashing (either method)

1. Serial console on the 2-pin **UART2** header, 115200 8N1, 3.3 V:
   pin 1 = board TX → adapter RXD, pin 2 = board RX ← adapter TXD. **This
   header has no GND pin** — connect the adapter's GND to FTDI-header pin 1,
   EXT VCC header pin 2, or SWD pin 3.
2. Power-cycle and expect:
   `BlueSaab` / `Hardware version: 6.1` / `Firmware version: <new version>` /
   `Configuring RN52...` / `RN52 configuration commands queued, rebooting
   module...` / (about 5 s later, 6.1.2+) `RN52 version: x.xx`.
3. Send `d` — a `BTA=<12 hex digits>` line (the RN52's Bluetooth address)
   proves the MCU↔RN52 link. `d` does *not* show the RN52 firmware version.
4. Send `I` (unit disappears from the phone's scan list), then `V` — the unit
   should appear on the phone as **"BlueSaab v6"**.
5. Only then reinstall in the car.

## RN52 module firmware (DFU) — only if metadata is wanted

If the RN52 firmware is < 1.16 (read the `RN52 version:` boot line on 6.1.2+
firmware, or the `R<ver>` part of the SID banner), SID track metadata cannot
work (`AD` command added in 1.16); everything else is unaffected. Upgrading is
optional and carries brick risk on a module that can no longer be bought —
decide deliberately.

The board anticipates it: the RN52's GPIO3 is netted as `DFU_PIN` in the
schematic, and the **UART3 header** exposes the RN52's UART directly. UART3 is
also 2-pin with no GND: pin 1 → RN52 RX, pin 2 ← RN52 TX (adapter TXD → pin 1,
RXD → pin 2), ground from FTDI pin 1 or EXT VCC pin 2. Rough procedure
(UNVERIFIED on this board):

1. Obtain the RN52 1.16 `.dfu` image (Microchip; may require archive
   digging) and Microchip's `ISUpdate.exe` (Windows).
2. Hold the STM32 in reset so it releases the RN52 UART. Note that this also
   floats the RN52's power-enable (PC8) and command (PA7) lines — confirm the
   module stays powered; PWREN may need tying high.
3. USB-serial (3.3 V) on the UART3 header; assert the DFU pin; power-cycle.
4. Run ISUpdate, flash, power-cycle, and check the `RN52 version: 1.16` boot
   line.

TODO before attempting: locate where `DFU_PIN` lands physically on the v6
board (test point/jumper — schematic shows the net, not the destination).

## Troubleshooting / open unknowns

- **OpenOCD can't connect:** check SWDIO/SWCLK aren't swapped; make sure the
  board is powered (probe doesn't power it); try `reset_config none` if the
  reset line isn't wired.
- **stm32flash gets no reply:** BOOT0 dance not done, wrong serial device,
  TX/RX swapped, or no common GND.
- **Board doesn't power up from the adapter:** its VCC pin is probably 3.3 V —
  use the MicroUSB jack for power.
- **Readback fails / flash is protected:** readout protection (RDP) would
  block backups. UNKNOWN whether any v6 units shipped with RDP set — if you
  hit this, note it here; flashing still works (mass-erase clears RDP and
  the old firmware with it — no backup possible in that case).
- TODO (needs a board in hand to confirm): FTDI header pin-1 orientation
  marking on the v6 silkscreen; exact 10-pin header key orientation.
