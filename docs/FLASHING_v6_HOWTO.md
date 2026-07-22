# Flashing the BlueSaab v6 — howto

Two supported paths. **Method A (serial bootloader) is the primary way —
it's how v6 units have historically been flashed** and needs nothing but a
USB-serial adapter. Method B (SWD/ST-Link) is the power-user alternative:
worth setting up if you want on-chip debugging or belt-and-braces
backup/restore.

The MicroUSB port is **not** a flashing path — the STM32F103's ROM bootloader
is serial-only (no USB DFU). Don't try.

Get a firmware binary first: build it per [BUILD_v6.md](BUILD_v6.md)
(`BUILD/BlueSaab.bin` / `.elf`).

## Method A — ROM serial bootloader (primary, historically used)

**Hardware:** any 3.3 V-logic USB-serial adapter on the **FTDI header**
(6-pin, standard FTDI cable pinout — black end = GND; the header can power
the board from the cable's 5 V). The board's BOOT0 and RESET push-buttons do
the bootloader dance; no soldering.

**Software:** `stm32flash` (`brew install stm32flash`).

```sh
# Enter the ROM bootloader: hold BOOT0, press+release RESET, release BOOT0.
# Then (adjust the serial device name):
stm32flash -r backup_v6_unit.bin /dev/tty.usbserial-XXXX   # backup (optional)
stm32flash -w BUILD/BlueSaab.bin -v /dev/tty.usbserial-XXXX # write + verify
# Press RESET to run the new firmware.
```

Note: the ROM bootloader talks on **USART1**, which is what the FTDI header
carries (the UART2 header is the debug console — wrong port for flashing).

## Method B — SWD with an ST-Link (debugging + robust backup)

**Hardware:** ST-Link v2 (clone dongles are fine) or the ST-Link end of any
Nucleo board (remove its jumpers to use it as a standalone probe), plus a
10-pin 1.27 mm ribbon or 4 jumper wires.

**Hookup:** the board's shrouded, keyed 10-pin 1.27 mm header is the standard
ARM Cortex debug pinout: pin 1 = VCC (target sense), 2 = SWDIO, 4 = SWCLK,
3/5/9 = GND, 10 = RESET. With jumper wires you need SWDIO, SWCLK, GND, VCC.
Power the board normally (12 V or FTDI 5 V) — the probe only *senses* VCC.

**Software:** OpenOCD (`brew install openocd`).

```sh
# 1. Back up the unit's current firmware (do this once, keep the file!)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "init; reset halt; flash read_bank 0 backup_v6_unit.bin; exit"

# 2. Flash + verify + reboot
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program BUILD/BlueSaab.elf verify reset exit"
```

Restore the backup later, if ever needed:

```sh
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program backup_v6_unit.bin 0x08000000 verify reset exit"
```

## Verify after flashing (either method)

1. Serial console on the **UART2** header, 115200 8N1.
2. Power-cycle: expect the boot banner (`BlueSaab / Hardware version /
   Firmware version`).
3. Send `d` — RN52 details should print (proves MCU↔RN52 link).
4. Send `V` — unit should appear as "BlueSaab" on a phone.
5. Only then reinstall in the car.

## RN52 module firmware (DFU) — only if metadata is wanted

If the RN52 reports firmware < 1.16 (`d` on the debug console), SID track
metadata cannot work (`AD` command added in 1.16); everything else is
unaffected. Upgrading is optional and carries brick risk on an EOL module —
decide deliberately.

The board anticipates it: the RN52's GPIO3 is netted as `DFU_PIN` in the
schematic, and the **UART3 header** exposes the RN52's UART directly.
Rough procedure (UNVERIFIED on this board):

1. Obtain the RN52 1.16 `.dfu` image (Microchip; EOL part — may require
   archive digging) and Microchip's `ISUpdate.exe` (Windows).
2. Hold the STM32 in reset so it releases the RN52 UART.
3. USB-serial (3.3 V) on the UART3 header; assert the DFU pin; power-cycle.
4. Run ISUpdate, flash, power-cycle, verify with `d` → 1.16.

TODO before attempting: locate where `DFU_PIN` lands physically on the v6
board (test point/jumper — schematic shows the net, not the destination).

## Troubleshooting / open unknowns

- **OpenOCD can't connect:** check SWDIO/SWCLK aren't swapped; make sure the
  board is powered (probe doesn't power it); try `reset_config none` if the
  reset line isn't wired.
- **stm32flash gets no reply:** BOOT0 dance not done, wrong serial device,
  or TX/RX swapped.
- **Readback fails / flash is protected:** readout protection (RDP) would
  block backups. UNKNOWN whether any v6 units shipped with RDP set — if you
  hit this, note it here; flashing still works (mass-erase clears RDP and
  the old firmware with it — no backup possible in that case).
- TODO (needs a board in hand to confirm): FTDI header pin-1 orientation
  marking on the v6 silkscreen; exact 10-pin header key orientation.
