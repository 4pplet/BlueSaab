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

## 2. Bench — flash 6.1.7

- [ ] `tools/bench.sh flash <FTDI-port>`. It refuses without a backup or if
      the binary's hash doesn't match, and verifies after writing. Then press
      RESET.
- [ ] Optional, first: build a stack-monitor variant
      (`make EXTRA_FLAGS=-DSTACK_MONITOR_ENABLED=1`), flash it with
      `tools/bench.sh flash <FTDI-port> BUILD/BlueSaab.bin` (it warns that
      this isn't the pinned RC — type `yes`), and check on the console that
      no thread's peak nears its stack size. Then flash the RC.

## 3. Bench — test 6.1.7

`tools/bench.sh console <UART2-port> after`, power-cycle, then:

- [ ] Banner shows `Firmware version: 6.1.7`
- [ ] ~6 s later: `RN52 version: ______`. This answers the static-SID-text
      question: below 1.16 = no track metadata, by design.
- [ ] `d` → `BTA=…` line
- [ ] `I` → "BlueSaab v6" disappears from the phone's scan list;
      `V` → it reappears. Pair, stream music; `P` / `N` / `R` control
      playback.
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
      several times. **Stop the test and remove the unit if any appear.**
      The node-status replies were restructured, and a malformed reply lit
      warnings on a 2004 9-5 in the past.
- [ ] CD mode activates normally; audio plays
- [ ] SID shows `6.1.7 R<ver>` for ~4 s on entering CD mode
- [ ] "CONNECTED" when the phone attaches
- [ ] Preset 1 → "PAIRING" + phone sees "BlueSaab v6"
- [ ] Hold middle SEEK > 2 s → same pairing behaviour
- [ ] Presets 4/5 — what happens? iPhone: ______ Android: ______
      (they send AVRCP volume commands to the phone)
- [ ] Presets 3 / 6, NXT, track ± behave as before
- [ ] 30+ min drive with scrolling track names: clean " - " seam, no flicker
      or garbled text. Include titles with ’ curly quotes, emoji, å/ä/ö.
- [ ] Switch source away and back repeatedly → the buttons never go dead
- [ ] One beep on entering CD mode (default build)
- [ ] With the ignition on, power-cycle the unit ~10× (unplug / power
      switch) → it comes up every time
- [ ] `E` after the drive: TX failures and RX overruns ≈ 0, REC/TEC low,
      ESR clean
- [ ] Park overnight with the unit powered. Before starting the car, read
      `E`: TEC ≈ 128 / error-passive means the unit transmitted into the
      sleeping bus all night (a known 6.2.0 item). Then drive: does
      Bluetooth reconnect on CD mode? ___

## 5. Release (only if everything above passed)

Per [BUILD_v6.md](BUILD_v6.md):

1. Fast-forward `master` to the RC commit.
2. Tag it `v6.1.7`.
3. Check that the draft release's `.bin` SHA-256 equals `RC_BIN_SHA256` in
   `tools/rc.env`.
4. Publish.

Anything that failed becomes either a stop-ship fix (new RC, new pin,
re-test) or a 6.2.0 item.
