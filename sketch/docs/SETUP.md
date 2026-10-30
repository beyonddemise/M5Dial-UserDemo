# Setup (DRAFT - Task 1)

Verified on macOS 26 (Apple Silicon). Linux, Windows and WSL2 are **untested**.
All versions are pinned in [`tools/deps.txt`](../tools/deps.txt); this page quotes them.

| Item | Pin |
|---|---|
| Core | `m5stack:esp32` 3.3.10 (FQBN `m5stack:esp32:m5stack_dial`, option `CDCOnBoot=cdc` so serial logs reach USB) |
| Libraries | M5Dial 1.0.3, M5Unified 0.2.28, M5GFX 0.2.35, NimBLE-Arduino 2.5.2, Adafruit NeoPixel 1.15.5, lvgl 9.6.0 |
| Python (venv) | pyserial 3.5, pillow 11.3.0, esptool 4.12.0, bleak 1.1.1 |
| Simulation | wokwi-cli 0.29.1, wasi-sdk 34 |

1. **Install arduino-cli:** `brew install arduino-cli` (macOS). Other OS: see the arduino-cli docs. Tested and used in CI: 1.5.1 (`ARDUINO_CLI_VERSION` in `tools/deps.txt`).
2. **Run the one-time setup:** `sketch/tools/setup.sh`. It creates `tools/.venv`, downloads
   wokwi-cli into `tools/bin/` and wasi-sdk into `tools/wasi-sdk/`, then installs the M5Stack
   core into the project-local `sketch/.arduino/` and the libraries into `sketch/libraries/`
   (git-ignored; `sketch/` is the Arduino sketchbook). It may `brew install` arduino-cli and,
   on Apple Silicon without Rosetta, universal-ctags; those are global. Downloaded wokwi-cli and
   wasi-sdk are verified against the sha256 pins in deps.txt.
   Every tool script sources `tools/env.sh`, so no shell activation is needed afterwards.
3. **Apple Silicon without Rosetta:** arduino-cli's bundled `ctags` is x86_64 (builds fail with
   `bad CPU type in executable`). setup.sh installs Homebrew `universal-ctags`, and
   `tools/env.sh` then makes `build.sh` / `gen_compile_commands.sh` pass
   `--build-property tools.ctags.path=sketch/tools/ctags`, selecting `tools/ctags/ctags`: a
   wrapper that rewrites universal-ctags output into the format arduino-cli's .ino prototype
   generator expects (raw universal-ctags silently puts `setup(); loop();` inside `setup()`,
   which recurses at boot). The installed toolchain is not modified; setup.sh restores the
   bundled binary if an older version of it had swapped it out. A bare `arduino-cli compile`
   (without the property) will not work on such a Mac - always build via `tools/build.sh`.
   `tools/tests/test_ctags_compat.py` checks the generated prototypes end to end.
4. **Back up the factory flash (once, before any upload):** connect the M5Dial via USB-C, then
   `sketch/tools/backup_flash.sh`. It writes `sketch/backup/m5dial-factory-<date>.bin`
   (exactly 8 388 608 bytes) plus `.sha256`, written as `.partial` and renamed only once verified.
   `upload.sh` refuses to run unless such a verified pair exists (`tools/check_backup.sh`).
   `tools/restore_flash.sh <file> --yes` writes it back (destructive; verifies size and checksum first).
5. **Build:** `sketch/tools/build.sh sketch/examples/Smoke` (output in `sketch/.build/Smoke/`).
6. **Upload:** `sketch/tools/upload.sh sketch/examples/Smoke`. The port is auto-detected by USB
   VID:PID `303A:1001`. If none is found, plug the cable in, or hold the knob button while
   plugging in to enter download mode.
7. **Check on the device:**
   `sketch/tools/.venv/bin/python sketch/tools/device_test.py sketch/examples/Smoke --expect 'boot: smoke' 'display: green'`
   Serial monitor: `sketch/tools/monitor.sh` (115200 baud).
8. **VSCode:** open the `sketch/` folder; tasks Build, Upload, Monitor, Build+Upload+Monitor,
   Device test and Host tests are in `.vscode/tasks.json`. Run `tools/gen_compile_commands.sh`
   once for IntelliSense. Unit tests: `sketch/tools/.venv/bin/python -m unittest discover sketch/tools/tests`.

## Known environment quirk

Some sandboxed environments make `arduino-cli`'s own downloads hang (Go TLS verification
blocked); a normal terminal is unaffected.
