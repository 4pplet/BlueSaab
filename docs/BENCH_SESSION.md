# BlueSaab 6.1.7 — bench & in-car validation session

The step-by-step for validating the firmware release candidate on a real v6
unit — first on the bench, then in the car. Print it; the blanks are for
notes. The candidate itself (version, commit, CI run, SHA-256) is pinned in
[tools/rc.env](../tools/rc.env). Nothing since 6.1.1 has touched hardware yet.

All commands run from the repo root; `tools/bench.sh` writes everything
(downloads, flash backups, console logs) under `bench/`, which is gitignored.

## What you need

- The v6 unit, removed from the car
- **USB-serial adapter(s), 3.3 V logic.** One for the FTDI header
  (flashing) and one for the UART2 header (console), or one moved between
  them. An FTDI TTL-232R-3V3 cable fits the FTDI header directly. Jumper-type
  breakouts (e.g. Kjell/Luxorparts) must be set to **3.3 V**.
- **Power:** a MicroUSB cable + phone charger (5 V into the board).
  Jumper-type adapters set to 3.3 V cannot power the board.
- For the current measurement: a 12 V bench supply and a multimeter in series
- An iPhone **and** an Android phone, if possible
- In the car: a laptop, to read `E` on the console

One-time setup: `brew install stm32flash`, then:

```sh
tools/bench.sh check          # tools, GitHub login, serial ports
tools/bench.sh fetch          # download the RC and verify its SHA-256
tools/bench.sh ports          # find your adapter(s): /dev/cu.usbserial-…
```

## Wiring

| Header | Adapter | Notes |
| --- | --- | --- |
| **FTDI** (flashing) | GND→pin 1, TXD→pin 4, RXD→pin 5 | pin 3 = 5 V in (only from a real 5 V source) |
| **UART2** (console) | adapter RXD→pin 1, adapter TXD→pin 2 | **no GND pin** — connect adapter GND to FTDI-header pin 1 or EXT VCC pin 2 |
| **MicroUSB** | charger | bench power |

BOOT0 + RESET buttons on the board enter the bootloader (the script tells you
when). Full hookup details: [FLASHING_v6_HOWTO.md](FLASHING_v6_HOWTO.md).

## 1. Bench — before flashing (records the unit's current state)

- [ ] Console on UART2, power on, and let it boot:
      `tools/bench.sh console <UART2-port> before`.
      Firmware banner: `Firmware version: ____________`
      (No banner at all = very old firmware; the backup below still tells.)
- [ ] `d` → a `BTA=…` line (proves the STM32↔RN52 link).
      `d` does **not** show the RN52 firmware version — that comes after
      flashing.
- [ ] Is "BlueSaab v6" visible on a phone right after power-up, without
      sending anything? ___ For how long? ___ (the RN52 is set to
      "discoverable on start up")
- [ ] Quit the console (Ctrl-]). Back up the flash — **never skip this**:
      `tools/bench.sh backup <FTDI-port>`. It prints the backup's version
      string: ____________
      Only a complete 128 KB read of real firmware is saved. The **oldest**
      backup in `bench/` is the unit's original firmware and what `restore`
      puts back — never delete or rename the backups, and bench **one unit
      per `bench/`** (move it aside for another unit; backups aren't tied to
      a chip). If the read fails with read protection, see
      [Rolling back](#rolling-back) before going further.

## 2. Bench — flash 6.1.7

- [ ] **Stack check first** — a stack overflow is a permanent halt, and the
      stacks have never been measured. Build the stack-monitor variant
      (`make clean && make EXTRA_FLAGS=-DSTACK_MONITOR_ENABLED=1` — the
      `make clean` matters), flash it with
      `tools/bench.sh flash <FTDI-port> BUILD/BlueSaab.bin` (it warns that
      this isn't the pinned RC — type `yes`). On the console it prints every
      thread's stack size and peak once a second. While it runs, exercise
      everything the bench can: pair, stream long titles, `P`/`N`/`R`,
      `V`/`I`, `C`/`D`, `B`, `d`, `E`, `H`. Highest peak / size: ________
      Any thread above ~85 % = stop, it's a fix before release.
- [ ] `tools/bench.sh flash <FTDI-port>` — the pinned RC. It refuses without
      a backup or if the binary's hash doesn't match, and verifies after
      writing. Then press RESET.

## 3. Bench — test 6.1.7

`tools/bench.sh console <UART2-port> after`, power-cycle, then:

- [ ] Banner shows `Firmware version: 6.1.7`
- [ ] ~6 s later: `RN52 version: ______`. This answers the static-SID-text
      question: below 1.16 = no track metadata, by design.
- [ ] `d` → `BTA=…` line
- [ ] `I` → "BlueSaab v6" disappears from the phone's scan list;
      `V` → it reappears. Pair, stream music; `P` / `N` / `R` control
      playback.
- [ ] Early connect: power-cycle, and tap "BlueSaab v6" on the paired phone
      right away, so it connects **before** `RN52 version:` appears. Then
      `N` alone must skip a track (the console takes keys only after the
      ~5 s RN52 boot wait).
- [ ] `B` (RN52 reboot), then `C` once it's back: the phone reconnects and
      `P` / `N` work.
- [ ] Command flood: `N` ~30× within 3 s. `queue full` lines may appear;
      5 s later `P` must still work. Repeat 3×.
- [ ] `d` 3× while a title containing `%` plays (any song name with a
      percent sign): a `BTA=` line each time, console still alive. `H`
      lists `E`.
- [ ] `E` → off-car these are **expected** to look bad: TX write failures
      climbing, TEC ≈ 128, ESR bit 1 (error-passive) set. Bus-off (bit 2)
      should be clear and RX overruns 0.
- [ ] RN52 auto power-off: the module is set to power off after 10 min
      without a connection, and Microchip doesn't document what wakes it.
      Disconnect the phone, wait **> 10 min**, then `V` and `C`: does it
      respond? ___ If not, Bluetooth stays dead until a power cycle, and the
      6.2.0 RN52 recovery becomes urgent.
- [ ] Current draw — 12 V bench supply on CDC connector pin 6 (GND pin 12),
      meter in series:

      | State | mA |
      | --- | --- |
      | idle, < 10 min after last disconnect | |
      | idle, > 10 min after last disconnect | |
      | streaming | |
      | pairing (peak) | |

      These are the first real numbers — every mA figure in the docs is an
      estimate so far.
- [ ] Only last, and only if re-pairing your phones is fine: `u` wipes
      **all** pairings.
- [ ] `tools/bench.sh summary` → paste the output into
      [V6_CODE_AUDIT.md](V6_CODE_AUDIT.md) "Deployed-unit findings".

## 4. In the car (any 9-3/9-5; a 9-5 is extra valuable)

Keep the laptop and the UART2 adapter connected if you can:
`tools/bench.sh console <UART2-port> car`.

- [ ] **No warning lights** (airbag/MIL) after entering and leaving CD mode
      several times. **Stop the test and remove the unit if any appear**,
      then [roll back](#rolling-back).
      The node-status replies were restructured, and a malformed reply lit
      warnings on a 2004 9-5 in the past.
- [ ] CD mode activates normally; audio plays
- [ ] SID shows `6.1.7 R<ver>` for ~4 s on entering CD mode. `R?` is
      expected only if CD mode is entered within ~6 s of power-up (version
      not read yet); later, `R?` means the version parse failed.
- [ ] "CONNECTED" when the phone attaches
- [ ] Preset 1 → "PAIRING" + phone sees "BlueSaab v6"
- [ ] Hold middle SEEK > 2 s → same pairing behaviour
- [ ] Presets 4/5 — what happens? iPhone: ______ Android: ______
      (they send AVRCP volume commands to the phone)
- [ ] Presets 3 / 6, NXT, track ± behave as before
- [ ] 30+ min drive with scrolling track names: clean " - " seam, no flicker
      or garbled text. Include titles with ’ curly quotes, emoji, å/ä/ö.
- [ ] Switch source away and back repeatedly → the buttons never go dead
- [ ] Leave CD mode mid-song and re-enter: banner, then CONNECTED/"BlueSaab
      v6", then the **current** track — never the previous title or garbage
- [ ] Ignition cycles with the radio left in CD mode: ignition off, wait
      until radio and SID go dark (~1 min), ignition on — 3×. No warning
      lights; CD mode and audio come back; `E` clean. (This is the only
      test of the power-on/power-down node-status replies.)
- [ ] One beep on entering CD mode (default build)
- [ ] With the ignition on and the radio in CD mode, power-cycle the unit
      ~10× (unplug / power switch), console logging. Every time: banner, no
      `CAN:` error line, CD mode back, no warning lights, the phone
      reconnects by itself and the **first** NXT/track+ press works. `E`
      within ~10 s of the first and last boot: REC/TEC near 0, ESR 0.
- [ ] Rapid track+ on the wheel (~30 presses in 3 s): playback controls
      still work 5 s later
- [ ] `E` after the drive: TX failures and RX overruns ≈ 0, REC/TEC low,
      ESR clean
- [ ] Park overnight with the unit powered. Before starting the car, read
      `E`: TEC ≈ 128 / error-passive means the unit transmitted into the
      sleeping bus all night (a known 6.2.0 item). Then drive: does
      Bluetooth reconnect on CD mode? ___
- [ ] Optional, valuable: one ~15 min drive on the stack-monitor build (CD
      on/off, presets, scrolling) — the only way to measure the SID and
      node-status threads under real traffic. Then flash the RC back.
- [ ] `tools/bench.sh summary` again — it scans all logs, car logs
      included, for `failed` / `queue full` / `dropped`; paste into
      [V6_CODE_AUDIT.md](V6_CODE_AUDIT.md).

## Rolling back

Anything wrong — on the bench or in the car — put the old firmware back
before driving with the unit again. Same wiring and bootloader steps as
flashing; each command shows the file, hash and version and asks for `yes`.

- **Normal case:** `tools/bench.sh restore <FTDI-port>` flashes the oldest
  backup (the unit's original firmware) and verifies it.
- **No usable backup:** `tools/bench.sh restore <FTDI-port> release`
  downloads the last published release (v6.1.1, pinned in
  [tools/rc.env](../tools/rc.env)), checks its hash and flashes it.
- **Read protection (RDP)** blocks both reading and writing through the
  bootloader. `stm32flash -k <FTDI-port>` lifts it by **erasing the whole
  chip** — the original firmware is gone for good. The chip resets, so
  enter the bootloader again, then `restore <FTDI-port> release`, then
  `backup` (now possible) so `flash` will run. (`backup` refuses an erased
  chip, so the order matters.) The "original" for this
  unit is then v6.1.1. Record the RDP finding in
  [FLASHING_v6_HOWTO.md](FLASHING_v6_HOWTO.md) — it's unknown whether any
  v6 units shipped protected.

After a rollback, `tools/bench.sh console <UART2-port> rollback`: the
banner should show the old version again, and the unit should work in the
car as before. Note what failed — it becomes a stop-ship fix for a new RC.

## 5. Release (only if everything above passed)

Per [BUILD_v6.md](BUILD_v6.md):

1. Fast-forward `master` to the RC commit.
2. Tag it `v6.1.7`.
3. Check that the draft release's `.bin` SHA-256 equals `RC_BIN_SHA256` in
   `tools/rc.env`.
4. Publish.

Anything that failed becomes either a stop-ship fix (new RC, new pin,
re-test) or a 6.2.0 item.
