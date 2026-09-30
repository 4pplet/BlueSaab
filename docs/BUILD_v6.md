# Building & flashing v6 firmware

Status: **build verified 2026-07-22** — clean build from this repo with ARM
GCC 8.5.0 on macOS (Apple Silicon), and continuously in CI with GCC
8-2019-q3. Newer GCC major versions are untested; the code is
`gnu++98`-flagged mbed OS 2, so stay on GCC 8.

## Toolchain

- **Canonical (releases): CI**, GCC `8-2019-q3` — see below. CI builds are
  byte-reproducible (same commit → same `.bin`).
- **Known good locally:** Homebrew `arm-none-eabi-gcc@8` (8.5.0) +
  `arm-none-eabi-binutils`. Both are keg-only, so put them on PATH explicitly:

  ```sh
  export PATH="/opt/homebrew/opt/arm-none-eabi-gcc@8/bin:/opt/homebrew/opt/arm-none-eabi-binutils/bin:$PATH"
  ```

  Local builds are reproducible too, but differ from CI's (different GCC 8
  point release) — both are valid; never mix them within one release.
- `make` — the root [Makefile](../Makefile) is a self-contained mbed-exported
  GCC ARM makefile (builds into `BUILD/`, target NUCLEO_F103RB /
  STM32F103RB, Cortex-M3).
- No mbed CLI, no internet: `mbed/` (mbed 2 library rev 148, `mbed.bld`
  fd96258d940d — the HAL is prebuilt by Arm as `libmbed.a` + objects, no HAL
  sources) and `mbed-rtos/` are vendored in-repo. Arm has retired Mbed and the
  URLs in `mbed.bld` / `mbed-rtos.lib` are dead — never try to update these
  libraries; they are frozen. HAL bugs can only be worked around by
  overriding a file, as `common/can_api.c` does.

## Build

```sh
make                                              # release build
make clean && make EXTRA_FLAGS="-DSTACK_MONITOR_ENABLED=1"   # a debug variant
```

**Always `make clean` when switching `EXTRA_FLAGS`** — the Makefile doesn't
track flag changes, so a plain `make` just relinks the old objects (and a
later plain `make` would reuse the variant's objects).

Output: `BUILD/` (gitignored) — `BlueSaab.bin` / `.hex` / `.elf`. Footprint:
about **88 KB flash with the CI toolchain, ~102 KB with Homebrew 8.5** (of
128 KB), and **~9.8 KB static RAM** (of 20 KB; thread stacks and heap come
out of the rest). v6.1.1 (Homebrew 8.5): text 100524 + data 2720 + bss 7160.

## Flash

Full step-by-step (hookup, backup, bootloader dance, verification):
**[FLASHING_v6_HOWTO.md](FLASHING_v6_HOWTO.md)**. Summary — two options,
both on the board's headers:

- **Serial bootloader** (the primary, historically used method): BOOT0
  button + FTDI header (USART1) with `stm32flash` or STM32CubeProgrammer.
- **SWD**: 10-pin 1.27 mm Cortex debug header; an ST-Link (or the ST-Link
  half of a Nucleo board) with OpenOCD.

After flashing, bench-verify on the UART2 console (boot banner incl.
`RN52 version:`, `d` → `BTA=` line, `I` then `V` → phone sees
"BlueSaab v6") before reinstalling in the car. (The pyOCD launch config
referenced in `.gitignore` was the original developers' debug setup, not the
flashing workflow.)

## Debug console

2-pin UART2 header (PA2/PA3, 3.3 V logic, **no GND pin** — take ground from
the FTDI header), **115200 8N1** — boot banner plus single-character commands
(see [USAGE_v6.md](USAGE_v6.md#debug-serial-console)).

## CI and release process

Every push runs `.github/workflows/build.yml`:

- **firmware** — builds four variants with `-Werror` (release, no-SID,
  no-beep, stack-monitor) so none can bit-rot; the release variant is
  uploaded as artifact `BlueSaab-<version>-<sha7>` with a `SHA256SUMS` file
  (hashes also shown on the run's summary page). Artifacts expire after 90
  days — the release is the permanent copy.
- **host-tests** — Scroller + utf_convert asserts (gnu++98, `-Werror`,
  ASan/UBSan, both char signednesses).
- **lint** — `tools/lint.sh`: cppcheck + the Clang static analyzer on the
  firmware (`tools/static-analysis.sh` compiles it exactly like the real
  build: ARM target, the Makefile's includes/defines, the ARM toolchain's
  headers), shellcheck on `tools/*.sh`, ruff on `tools/*.py`. Known false
  positives carry inline `cppcheck-suppress` comments with the reason.

Run the same locally with `tools/lint.sh` (needs cppcheck, clang-tidy from
Homebrew LLVM, shellcheck, ruff; actionlint optional).

Actions are pinned by commit SHA; Dependabot proposes updates monthly.

**Always pass `--repo 4pplet/BlueSaab`** — this checkout also has the
upstream repo as a remote, and `gh` may pick it. Fetch the artifact of a
*specific* commit (a bare `-n name` download takes the newest artifact from
any branch):

```sh
RUN=$(gh run list --repo 4pplet/BlueSaab --workflow build \
      --commit "$(git rev-parse HEAD)" --json databaseId -q '.[0].databaseId')
gh run watch "$RUN" --repo 4pplet/BlueSaab
gh run download "$RUN" --repo 4pplet/BlueSaab -p 'BlueSaab-*' -D dist
shasum -a 256 dist/*/BlueSaab.bin     # record this hash when you validate
```

Releasing a firmware version:

1. Bump `FIRMWARE_VERSION` in `SaabCan.h` (single source of truth — feeds
   the boot banner and the SID version display) and add the CHANGELOG
   section `## x.y.z …`.
2. Push; wait for CI green; pin the candidate in `tools/rc.env` (version,
   commit, CI run, `.bin` SHA-256) and **validate exactly that artifact on
   real hardware** (docs/BENCH_SESSION.md; `tools/bench.sh fetch` verifies
   the hash).
3. Fast-forward `master` (the release branch; `revival` is the dev branch —
   never rebase it) and tag the exact commit you validated:
   `git push origin revival:master`, then
   `git tag -a v6.x.y <sha> -m "BlueSaab firmware v6.x.y"` and
   `git push origin v6.x.y` (not `--tags`).
4. The tag triggers `.github/workflows/release.yml`: it checks that the tag
   matches `FIRMWARE_VERSION`, rebuilds, and creates a **draft** release with
   the binaries, `SHA256SUMS` and the CHANGELOG section as notes.
5. Check that the `.bin` line in `SHA256SUMS` equals `RC_BIN_SHA256` from
   `tools/rc.env` (CI is reproducible, so it will), then publish the draft.

## Compile-time options

All three can be set in the header or overridden per build with
`make EXTRA_FLAGS="-D<NAME>=<value>"`.

- `SID_TEXT_CONTROL_ENABLED` (default `1`, [SidResource.h](../SidResource.h))
  — `0` removes all SID writes: the version banner, PAIRING/CONNECTED
  notices and track metadata (e.g. for nav-equipped cars).
- `CDC_ENTRY_BEEP_ENABLED` (default `1`, [SaabCan.h](../SaabCan.h)) — `0`
  suppresses the beep on entering CDC mode.
- `STACK_MONITOR_ENABLED` (default `0`,
  [common/SerialLog.h](../common/SerialLog.h)) — `1` builds a debug variant
  that prints every thread's stack high-water mark once per second on the
  console. Use it on the bench to verify stack margins after thread changes;
  keep `0` for releases.
