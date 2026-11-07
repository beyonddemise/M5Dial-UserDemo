# Setup

Verified on macOS 26 (Apple Silicon). CI builds on ubuntu-24.04. Linux and WSL2 are untested
by hand. Native Windows is **not supported** (`setup.sh` stops there): use WSL2.
On Ubuntu/WSL2, install `python3-venv` first; if a failed run left `sketch/tools/.venv`, delete it
before retrying (setup.sh skips the venv step once its python exists). WSL2 does not see USB
devices by itself: attach the Dial with [usbipd-win](https://learn.microsoft.com/windows/wsl/connect-usb)
before the backup step. If the port is there but cannot be opened, check its group
(`ls -l "$(sketch/tools/.venv/bin/python sketch/tools/find_port.py)"`, usually `dialout`), run `sudo usermod -aG dialout "$USER"`, log in again.
Commands below start at the repository root.
All versions are pinned in [`tools/deps.txt`](../tools/deps.txt); this page quotes them.

| Item | Pin |
|---|---|
| Core | `m5stack:esp32` 3.3.10 (FQBN `m5stack:esp32:m5stack_dial`, options `CDCOnBoot=cdc` so serial logs reach USB, `FlashSize=8M,PartitionScheme=huge_app` for a 3 MB app, no OTA; UserDemo overrides the table with its own `UserDemo/partitions.csv`: 4 MB app, no OTA, no FAT, same 8 MB flash) |
| Libraries | M5Dial 1.0.3, M5Unified 0.2.28, M5GFX 0.2.35, NimBLE-Arduino 2.5.2, Adafruit NeoPixel 1.15.5, lvgl 9.6.0 |
| Python (venv) | pyserial 3.5, pillow 11.3.0, esptool 4.12.0, bleak 1.1.1, pyyaml 6.0.3 (venv from the system `python3`; macOS's 3.9.6 works) |
| Simulation | wokwi-cli 0.29.1, wasi-sdk 34 |

1. **Install arduino-cli:** `brew install arduino-cli` (macOS). Other OS: see the arduino-cli docs. Tested and used in CI: 1.5.1 (`ARDUINO_CLI_VERSION` in `tools/deps.txt`).
2. **Run the one-time setup:** `sketch/tools/setup.sh`. It creates `tools/.venv`, downloads
   wokwi-cli into `tools/bin/` and wasi-sdk into `tools/wasi-sdk/`, then installs the M5Stack
   core into the project-local `sketch/.arduino/` and the libraries into `sketch/libraries/`
   (git-ignored; `sketch/` is the Arduino sketchbook). It may `brew install` arduino-cli and,
   on Apple Silicon, universal-ctags; those are global. Downloaded wokwi-cli and
   wasi-sdk are verified against the sha256 pins in deps.txt.
   Every tool script sources `tools/env.sh`, so no shell activation is needed afterwards.
   (`env.sh` is bash-only; don't `source` it from zsh, it then resolves the wrong paths.)
   It is idempotent: if it stops (e.g. `Some indexes could not be updated`), run it again; finished
   steps are skipped. It needs network access to github.com, downloads.arduino.cc and
   m5stack.oss-cn-shenzhen.aliyuncs.com (the M5Stack board index).
3. **Apple Silicon (with or without Rosetta):** arduino-cli's bundled `ctags` is x86_64 (builds fail with
   `bad CPU type in executable`). setup.sh installs Homebrew `universal-ctags`, and
   `tools/env.sh` then makes `build.sh` / `gen_compile_commands.sh` pass
   `--build-property tools.ctags.path=sketch/tools/ctags`, selecting `tools/ctags/ctags`: a
   wrapper that rewrites universal-ctags output into the format arduino-cli's .ino prototype
   generator expects (raw universal-ctags silently puts `setup(); loop();` inside `setup()`,
   which recurses at boot). The installed toolchain is not modified; setup.sh restores the
   bundled binary if an older version of it had swapped it out. A bare `arduino-cli compile`
   (without the property) will not work on such a Mac - always build via `tools/build.sh`.
   `tools/tests/test_ctags_compat.py` checks the generated prototypes end to end.
   *Upgrading:* if you set up before the wrapper was used on Rosetta-enabled Macs, rerun
   `sketch/tools/setup.sh` (or just `brew install universal-ctags`) so the wrapper has a real
   universal-ctags; the restore of the bundled binary is tidy-up only, builds do not need it.
4. **Back up the factory flash (once, before any upload):** connect the M5Dial via USB-C, then
   `sketch/tools/backup_flash.sh`. It writes `sketch/backup/m5dial-factory-<date>.bin`
   (exactly 8 388 608 bytes) plus `.sha256`, written as `.partial` and renamed only once verified.
   `upload.sh` refuses to run unless such a verified pair exists (`tools/check_backup.sh`).
   `tools/restore_flash.sh <file> --yes` writes it back (destructive; verifies size and checksum first).
5. **Build:** `sketch/tools/build.sh sketch/UserDemo` (output in `sketch/.build/UserDemo/`; the size line
   is checked against UserDemo's own 4 MB app partition). Any example the same way, e.g.
   `sketch/tools/build.sh sketch/examples/DialProbe`. `sketch/tools/build_all.sh` builds all 11 sketches.
   Optional, for the factory test's Wi-Fi: `cp sketch/UserDemo/secrets.example.h sketch/UserDemo/secrets.h`
   and fill in your values (git-ignored). Without it UserDemo builds with placeholder credentials.
6. **Upload:** `sketch/tools/upload.sh sketch/examples/DialProbe`. The port is auto-detected by USB
   VID:PID `303A:1001`. If none is found, check the cable. If a crashed sketch dropped USB, enter
   download mode: hold **G0**, the small button on the StampS3 inside the Dial (not the knob
   button), while you plug in USB, then release it (M5Stack Dial docs).
7. **Check on the device:**
   `sketch/tools/.venv/bin/python sketch/tools/device_test.py sketch/examples/DialProbe --expect 'boot: DialProbe' 'init: rfid ok' 'init: display 240x240' 'init: rtc ok' 'init: touch ok' 'rtc: 20'`
   Serial monitor: `sketch/tools/monitor.sh` (115200 baud).
8. **VSCode:** open the `sketch/` folder; tasks Build, Upload, Monitor, Build+Upload+Monitor,
   Device test, Sim prepare, Simulate, Sim test and Host tests are in `.vscode/tasks.json`
   (the "Host tests" task runs the Python tool tests). Run `sketch/tools/gen_compile_commands.sh`
   once for IntelliSense.
   Tests: `sketch/tools/.venv/bin/python -m unittest discover sketch/tools/tests` (Python tools) and
   `sketch/tools/test_host.sh` (C++ host tests of the library; needs a host `c++`).
9. **Virtual M5Dial (Wokwi):** needs `make` (Xcode Command Line Tools on macOS,
   `build-essential` on Linux), which builds the chips. Install the recommended Wokwi VSCode extension and get a CI token
   at https://wokwi.com/dashboard/ci (`export WOKWI_CLI_TOKEN=...`).
   - VSCode task **Simulate**: pick a sketch; it builds the sim variant and the chips
     (`tools/sim.sh <sketch> --prepare-only`), then starts the Wokwi panel on `sim/wokwi.toml`
     (select that file once with "Wokwi: Select Config File").
   - VSCode task **Sim test**: pick a sketch and a scenario; runs `tools/sim.sh <sketch> <scenario>`.
     The pick-list is a static list in `.vscode/tasks.json` (input `scenario`): a new file under
     `sim/scenarios/` appears there only once you add it to that list too.
   - Command line: `sketch/tools/sim.sh sketch/examples/DialProbe sketch/sim/scenarios/DialProbe/probe.test.yaml`.
   - The free Wokwi plan has a monthly CI-minute quota; every scenario run (local or CI) counts against it.
     CI therefore runs scenarios only on manual dispatch and on same-repo pull requests labelled `sim`
     (see PORTING_NOTES "CI").

## Known environment quirk

Some sandboxed environments (e.g. an AI agent's shell) make `arduino-cli`'s own HTTPS downloads
hang, while `curl` to the same URL works. Seen as
`Download failed: performing HEAD request: Head "https://m5stack.oss-cn-shenzhen.aliyuncs.com/...": context deadline exceeded`
and `Some indexes could not be updated`; setup.sh then stops before the core install. A longer
`ARDUINO_NETWORK_CONNECTION_TIMEOUT` does not help. A normal terminal is unaffected.
