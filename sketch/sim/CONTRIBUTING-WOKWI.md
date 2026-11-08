# Contributing the M5Dial simulation parts to Wokwi

This repo carries five Wokwi custom chips and a draft board definition for the M5Stack Dial.
They are written to be given back to the Wokwi community. This page lists the steps; every
step that creates something public (a repo, a release, a pull request to Wokwi) is the
owner's call and happens only on the owner's explicit go (issue #20, step 4).

| Folder | Part | Own repo (to create) |
|--------|------|----------------------|
| [`chips/encoder`](chips/encoder) | scriptable rotary encoder | `beyonddemise/chip-encoder` |
| [`chips/ft3267`](chips/ft3267) | FocalTech FT3267 touch controller (I2C) | `beyonddemise/chip-ft3267` |
| [`chips/gc9a01`](chips/gc9a01) | GalaxyCore GC9A01 240x240 round TFT (SPI) | `beyonddemise/chip-gc9a01` |
| [`chips/pcf8563`](chips/pcf8563) | NXP PCF8563 RTC (I2C) | `beyonddemise/chip-pcf8563` |
| [`chips/ws1850s`](chips/ws1850s) | WS1850S / MFRC522 RFID reader (I2C) + one card | `beyonddemise/chip-ws1850s` |
| [`board-m5dial`](board-m5dial) | M5Stack Dial board (draft) | a PR to `wokwi/wokwi-boards` |

## What is in place

- Each chip folder is a complete chip repo in the layout of Wokwi's chip template
  ([wokwi/inverter-chip](https://github.com/wokwi/inverter-chip)): `<chip>.chip.json`,
  `<chip>.chip.c`, `wokwi-api.h`, `Makefile` (`make`, `make zip`, `make test`), `README.md`,
  `LICENSE`, `.gitignore`, a host unit test in `test/` and a minimal Wokwi project in
  `example/diagram.json` (stock ESP32-S3-DevKitC-1 + the chip). Nothing in a chip folder
  refers to a file outside it.
- [`.github/workflows/wokwi-chips.yml`](../../.github/workflows/wokwi-chips.yml) builds every
  chip on each change and keeps an artefact `<chip>-chip` per chip. Downloaded, it is
  `<chip>-chip.zip` with exactly `chip.json` and `chip.wasm` - the release payload.
- `sketch/tools/tests/test_wokwi_contrib.py` checks the package: template files, README
  sections, examples, a `make zip` from a copy of each folder alone, the board's pin map
  against `diagram.json`, and this page.

## Status, honestly

The chips pass their host unit tests and build to wasm. None of them has yet been seen
passing a Wokwi simulation: this repo's scenarios are held up by #39 (the simulation hangs).
The board definition has never been loaded in Wokwi. Release the chips as **pre-releases**
(`v0.1.0`) and say so in the release notes.

## Route 1: a chip as a community chip (own repo + release)

Wokwi loads a custom chip from any public GitHub repo with a release whose tag starts with
`v` and which carries an asset `chip.zip` holding exactly `chip.json` and `chip.wasm`. A
project then uses it by name and version:

```json
"dependencies": { "chip-gc9a01": "github:beyonddemise/chip-gc9a01@0.1.0" }
```

with a part `{ "type": "chip-gc9a01", "id": "lcd" }`. No review by Wokwi is needed.

Steps per chip, from a clean checkout of this repo's `main` (`gh` logged in with rights to
create repos in `beyonddemise`; wasi-sdk from `sketch/tools/setup.sh`):

```sh
chip=gc9a01   # encoder | ft3267 | gc9a01 | pcf8563 | ws1850s
ver=0.1.0     # the version in the chip's example/diagram.json
git checkout main && git pull --ff-only

# 1. history of the chip folder only, as its own branch (root = the chip folder)
git subtree split --prefix sketch/sim/chips/$chip -b split/chip-$chip

# 2. the public repo, with that history as its main branch
gh repo create beyonddemise/chip-$chip --public \
  --description "Wokwi custom chip: $chip" --homepage "https://wokwi.com"
git push https://github.com/beyonddemise/chip-$chip.git split/chip-$chip:main

# 3. the release payload, built from the same commit (the folder is the split tree)
make -C sketch/sim/chips/$chip clean
make -C sketch/sim/chips/$chip WASI_SDK_PATH="$PWD/sketch/tools/wasi-sdk" zip
unzip -l sketch/sim/chips/$chip/dist/chip.zip   # exactly chip.json + chip.wasm

# 4. tag v$ver on the new repo's main and attach chip.zip, marked as pre-release
gh release create "v$ver" sketch/sim/chips/$chip/dist/chip.zip \
  --repo beyonddemise/chip-$chip --target main --prerelease \
  --title "v$ver" \
  --notes "Pre-release $ver. Host unit test passes; not yet seen passing a Wokwi simulation. See README.md."

git branch -D split/chip-$chip
```

Instead of step 3 you can download the `<chip>-chip` artefact of the `wokwi-chips` run on that
`main` commit and zip its two files as `chip.zip`; the content is the same.

Check it: open a new ESP32-S3 project on wokwi.com, paste the chip's `example/diagram.json`
and start the simulation; the chip must load without errors (browser console).

**Later versions** (each chip has its own version):

1. Change the chip here and, in the same PR, set its new version (e.g. `0.1.1`) in the
   chip's `example/diagram.json` dependency, its `README.md` and its entry in
   `board-m5dial/board.json`. The tests take each chip's version from its example and fail
   if the README or board names another; the other chips keep theirs.
2. After the merge, run the whole block above again with `ver` set to the new version. It
   starts with `git checkout main && git pull --ff-only`: the split is taken from `HEAD`, so
   an old local `main` would publish the old chip. Step 2 then skips `gh repo create` (the
   repo exists) and pushes the split branch; the split is deterministic, so the push
   fast-forwards. Push **before** the release: `--target main` tags the repo's current
   `main`, which must be the commit the payload was built from. Title and notes follow `ver`.

## Route 2: the M5Dial board (pull request to wokwi/wokwi-boards)

Boards live in [wokwi/wokwi-boards](https://github.com/wokwi/wokwi-boards), one folder per
board under `boards/` with `board.json` (JSON with comments) and `board.svg`. A merged board
appears in Wokwi's part list for everyone. Merged precedents: `boards/st7789` (a board made of
a custom chip from a GitHub release, plus its display) and `boards/m5stack-core-s3` /
`boards/esp32-s3-box-3` (ESP32-S3 boards with internal nets).

What it needs:

1. **All five chips released** (route 1): `board.json` refers to them as
   `github:beyonddemise/chip-<part>@0.1.0`.
2. **A passing Wokwi scenario first.** Offer the board only after the M5Dial demo's
   scenarios pass in Wokwi (#39 closed) and the board itself ran there. Until then it stays
   a draft in [`board-m5dial/`](board-m5dial).
3. **A local test with Wokwi's own loader:** in a new wokwi.com project, click into the code
   editor, press F1, choose "Load custom board file..." and select the `board-m5dial` folder;
   then use `{ "type": "wokwi-custom-board", "id": "board" }` as the only part of
   `diagram.json`. Check the browser console for errors, wire a button to `G42`, and run a
   sketch built for the Dial. The README there lists the format guesses to confirm.
4. **Wokwi's board contribution guidelines**, linked from the wokwi-boards README:
   <https://docs.google.com/document/d/1K59xkKL70uN_c_GNCs_Xk09F3tK12B-ygmFbw6i14t4/edit>.
   Read them before opening the PR.
5. **The PR:** fork wokwi/wokwi-boards, copy `board.json` and `board.svg` to
   `boards/m5stack-dial/`, open the PR with a screenshot of the board running.

## Licence

All chips, the board definition and its drawing are MIT-licensed (see each chip's
`LICENSE`, copyright Stephan Wissel). `wokwi-api.h` in each chip folder is Wokwi's
unmodified Chips API header, which every Wokwi chip ships. The board drawing is original;
it copies no M5Stack artwork or logo. "M5Stack" and "M5Dial" name the hardware being
simulated.
