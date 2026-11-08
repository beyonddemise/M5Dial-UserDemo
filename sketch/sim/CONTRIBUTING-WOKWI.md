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

The chips pass their host unit tests, build to wasm, and run in this repo's Wokwi scenarios,
which pass since the #39 fix (PR #50). The board definition has never been loaded in Wokwi. Release the chips as **pre-releases**
(`v0.1.0`) and say so in the release notes.

## Route 1: a chip as a community chip (own repo + release)

Wokwi loads a custom chip from any public GitHub repo with a release whose tag starts with
`v` and which carries an asset `chip.zip` holding exactly `chip.json` and `chip.wasm`. A
project then uses it by name and version:

```json
"dependencies": { "chip-gc9a01": "github:beyonddemise/chip-gc9a01@0.1.0" }
```

with a part `{ "type": "chip-gc9a01", "id": "lcd" }`. No review by Wokwi is needed.

Steps per chip, from this repo's root (`gh` logged in with rights to create repos in
`beyonddemise`; wasi-sdk from `sketch/tools/setup.sh`). Set `chip` and `ver`, then paste the
block. It runs in its own `bash -eu`, and every check (split, build, zip content, repo
visibility) runs before the first public change. A later remote step (push, tag, release)
can still fail half-way; rerun the block once the cause is fixed. The push is fast-forward
only and an existing tag stops it.

```sh
chip=gc9a01 ver=0.1.0 bash -eu <<'EOS'
# chip: encoder | ft3267 | gc9a01 | pcf8563 | ws1850s; ver: the version in its example/diagram.json
repo=beyonddemise/chip-$chip

# 1. history of the chip folder only (root = the chip folder), taken from the merged main;
#    why no checkout: main may be checked out in another worktree, and the working tree is
#    never used (step 2 builds from the split commit itself)
git fetch origin
split=$(git subtree split --prefix sketch/sim/chips/$chip origin/main)

# 2. preflight, nothing public yet: build chip.zip from an export of exactly that commit
tmp=$(mktemp -d)
git archive "$split" | tar -x -C "$tmp"
make -C "$tmp" WASI_SDK_PATH="$PWD/sketch/tools/wasi-sdk" zip
test "$(unzip -Z1 "$tmp/dist/chip.zip" | sort | tr '\n' ' ')" = "chip.json chip.wasm "

# 3. the repo: created if missing; an existing one must be public (Wokwi cannot load a private one)
vis=$(gh repo view "$repo" --json visibility --jq .visibility 2>/dev/null || echo MISSING)
if [ "$vis" = MISSING ]; then
  # why no special case for a failed lookup: create then fails on an existing repo and stops
  gh repo create "$repo" --public --description "Wokwi custom chip: $chip" --homepage "https://wokwi.com"
elif [ "$vis" != PUBLIC ]; then
  echo "$repo is $vis, not PUBLIC" >&2; exit 1
fi

# 4. publish the history as main (fast-forward only; the split is deterministic)
git push "https://github.com/$repo.git" "$split:refs/heads/main"

# 5. tag v$ver on the split commit in one API call: it fails if the tag already exists, so a
#    lookup error can never be mistaken for "no tag"; the release then uses only that tag
gh api "repos/$repo/git/refs" -f ref="refs/tags/v$ver" -f sha="$split" >/dev/null
gh release create "v$ver" "$tmp/dist/chip.zip" --repo "$repo" --verify-tag --prerelease \
  --title "v$ver" --notes "Pre-release $ver. Status, registers and known gaps: README.md."
EOS
```

Instead of building in step 2 you can download the `<chip>-chip` artefact of the `wokwi-chips` run on that
`main` commit and zip its two files as `chip.zip`; the content is the same.

Check it: open a new ESP32-S3 project on wokwi.com, paste the chip's `example/diagram.json`
and start the simulation; the chip must load without errors (browser console).

**Later versions** (each chip has its own version):

1. Change the chip here and, in the same PR, set its new version (e.g. `0.1.1`) in the
   chip's `example/diagram.json` dependency, its `README.md` and its entry in
   `board-m5dial/board.json`. The tests take each chip's version from its example and fail
   if the README or board names another; the other chips keep theirs.
2. After the merge, run the block above again with `ver` set to the new version. Step 1 splits
   the merged `main` (never the working tree), step 3 keeps the existing repo, step 4
   fast-forwards its `main`, and step 5 tags exactly the pushed split commit, so tag and
   `chip.zip` always match.

## Route 2: the M5Dial board (pull request to wokwi/wokwi-boards)

Boards live in [wokwi/wokwi-boards](https://github.com/wokwi/wokwi-boards), one folder per
board under `boards/` with `board.json` (JSON with comments) and `board.svg`. A merged board
appears in Wokwi's part list for everyone. Merged precedents: `boards/st7789` (a board made of
a custom chip from a GitHub release, plus its display) and `boards/m5stack-core-s3` /
`boards/esp32-s3-box-3` (ESP32-S3 boards with internal nets).

What it needs:

1. **All five chips released** (route 1): `board.json` refers to them as
   `github:beyonddemise/chip-<part>@0.1.0`.
2. **The board itself ran in Wokwi.** The demo's scenarios pass (#39, PR #50); offer the
   board only after it has also loaded and run there (item 3). Until then it stays
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
