# M5Dial UserDemo → Arduino Sketch Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (owner's choice) to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every app of the ESP-IDF M5Dial UserDemo runs again as Arduino `.ino` sketches under `/sketch`, built with arduino-cli from VSCode, written directly against the official M5Dial library, and tested automatically on a virtual M5Dial in Wokwi and on the attached device.

**Architecture:** `/sketch` is an Arduino sketchbook. Local library `M5DialDemo` holds the kept app framework (Mooncake `APP_BASE`, `GUI_Base`, `smooth_menu`) and all apps; apps call `M5Dial.*` directly (no hardware wrappers). `UserDemo.ino` is the launcher; `examples/<App>/<App>.ino` run one app each. `sketch/sim/` is a Wokwi project (stock parts + custom chips) that M5Unified/M5GFX accept as an M5Dial, so the same ELF runs in sim and on hardware.

**Tech Stack:** arduino-cli, M5Stack ESP32 board package (Arduino-ESP32 3.x), M5Dial 1.0.3 + M5Unified + M5GFX, NimBLE-Arduino, Adafruit NeoPixel, lvgl 9.x (LvglDemo only), Wokwi (wokwi-cli, Chips API, wasi-sdk), Python 3 (pyserial, Pillow, esptool, bleak), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-30-arduino-sketch-port-design.md` (read first; decisions D1–D13 are binding).

**Original source (read-only reference):** `main/` — never modified.

**Owner prerequisites before Task 2:** Wokwi account, `WOKWI_CLI_TOKEN` exported in the shell and set as GitHub repo secret. Before Task 13: macOS Bluetooth permission for the terminal app.

## Global Constraints

- All new files under `/sketch` (plus `.github/workflows/`); `main/`, `components/`, root build files untouched.
- **No compatibility layer (D7).** Ported code uses M5Dial naming: `M5Dial.Display`, `DialEncoder.read()` (D12 — not `M5Dial.Encoder`, whose interrupts don't attach on GPIO40/41), `M5Dial.BtnA.isPressed()/wasPressed()/wasReleased()/pressedFor()`, `M5Dial.Touch.getDetail()`, `M5Dial.Rtc.getDateTime()/setAlarmIRQ()/clearIRQ()/disableIRQ()`, `M5Dial.Speaker.tone()`, `M5Dial.Power.powerOff()/timerSleep()`, `M5Dial.Rfid`, `M5Dial.Ex_I2C`, `M5Canvas`. No `HAL` class, no `hal->`, no `LGFX_Sprite`, no `btn.read()`, no `tp.` names in new code. Shared input helpers: `DialEncoder` (interrupt counter, M5Dial `Encoder` API) and `EncoderDetent` (pure math). Sketches call `M5Dial.begin(cfg, /*enableEncoder*/ false, /*enableRFID*/ true)` and `dialEncoder.begin()` instead.
- Every loop that waits on input calls `M5Dial.update()` each iteration.
- Logging only via M5Unified `M5_LOGI/W/E`; tests match on the message body, form `<tag>: <message>` (e.g. `rfid: uid=AA:BB:CC:DD`). Serial 115200.
- Library versions pinned once in `sketch/tools/deps.txt`; SETUP.md quotes the same values. Choose latest stable in Task 1; never float. Do **not** install LovyanGFX.
- Wi-Fi credentials only in git-ignored `sketch/UserDemo/secrets.h`; `secrets.example.h` committed.
- `ENABLE_FACTORY_TEST` defaults on in `config.h`. Fonts: `GUI_FONT_CN_BIG = &fonts::efontCN_24`, `GUI_FONT_CN_SMALL = &fonts::efontCN_16_b`.
- Doc comments on every class and public method (`/** */`); `// why:` on non-obvious logic. Keep upstream author headers/licences on ported files.
- One task ↔ one issue ↔ branch `port/<NN>-<slug>` ↔ one PR against `main` (`gh pr create -R beyonddemise/M5Dial-UserDemo --base main`; never the upstream). CodeRabbit review addressed, all checks green, then **merge commit + delete branch** (D13). A task is done when its automated checks pass; **no task before Task 19 waits for a human**.
- No upload to the device before Task 1's flash backup exists.
- Same compiled ELF for sim and device; sim-only behaviour only via the documented fallback flag (Task 2).
- **Wokwi parts are donation-ready (owner wants to contribute them back to Wokwi):** each chip in `sim/chips/<chip>/` is self-contained (no includes from outside its folder, no M5DialDemo dependency), follows the official Wokwi chip template layout (`<chip>.chip.json`, `<chip>.chip.c`, `Makefile`, `README.md` with pins/controls/supported commands/limitations, `LICENSE` = MIT), is generic to the real part (no M5Dial-specific behaviour hard-coded beyond defaults), and builds a `chip.zip` the way the template's release workflow does. `diagram.json` is kept as the reference "M5Dial" assembly.

## Review Focus

1. **Missing `M5Dial.update()` in blocking waits:** the original has many `while (!btn.read())` loops; ported naïvely to `while (M5Dial.BtnA.isPressed())` without `update()` they hang forever. → Task 6 scenario `press_hold_release_opens_app`.
2. **Encoder edges lost under load / detent size / zero crossing:** counting must be interrupt-driven (D12) so a 20 ms canvas push can't drop edges; M5Dial `ENCODER` is x4; one detent must give exactly one step, including detents around count 0 (truncating division makes a double-wide dead zone). → Task 4 host tests + Task 3 probe scenario.
3. **Direction:** clockwise must mean "next item" everywhere. → Task 3 probe scenario (KY-040 CW → `steps=+1`), Task 6 launcher scenario, Task 19 physical check.
4. **Serial port changes / download mode:** port name differs per plug-in; crashed sketch can drop USB-CDC. Tools auto-detect VID:PID `303A:1001` and fail with a clear message. → Task 1 test.
5. **Heap exhaustion (no PSRAM):** 115 KB canvas + Wi-Fi and BLE used sequentially (spec §4) across repeated app open/close. → Task 14 `MemSoak` device test.

---

## File Structure (locked)

```
sketch/
  README.md  arduino-cli.yaml  .gitignore
  docs/SETUP.md  PORTING_NOTES.md  TEST_CHECKLIST.md  FACTORY_TEST_JIG.md
  tools/deps.txt setup.sh build.sh upload.sh monitor.sh find_port.py device_test.py
        sim.sh backup_flash.sh restore_flash.sh gen_compile_commands.sh png2rgb565.py
        ble_check.py test_host.sh  tests/test_find_port.py  tests/test_png2rgb565.py
  sim/  wokwi.toml.in  diagram.json  chips/{gc9a01,ft3267,pcf8563,ws1850s}/  scenarios/<Sketch>/*.test.yaml  golden/
  .vscode/tasks.json settings.json extensions.json c_cpp_properties.json
  backup/ (git-ignored)
  libraries/M5DialDemo/
    library.properties
    src/M5DialDemo.h  config.h  app_context.h  quadrature_decoder.h  dial_encoder.{h,cpp}  encoder_detent.h  m5dd_begin.{h,cpp}
    src/mooncake/app.h  gui_base.{h,cpp}
    src/smooth_menu/**                       (from main/apps/utilities/smooth_menu/src + LICENSE)
    src/assets/launcher_icons/*.h  temp_demo_icons/*.h  png/*.png
    src/apps/{launcher,lcd_test,rtc_test,rfid_test,set_brightness,temp_demo,wifi_scan,ble_server,more_menu,factory_test}/
    test/host/*.cpp                          (host unit tests, not built by Arduino)
  UserDemo/UserDemo.ino  secrets.example.h
  examples/DialProbe LcdTest RtcTest RfidTest Brightness TempDemo WifiScan BleServer MemSoak LvglDemo/
```

---

## Phase 1 — Foundation and simulation

### Task 1: Toolchain, skeleton, device tooling, flash backup  *(issue 1)*

**Files:** `sketch/{arduino-cli.yaml,.gitignore,README.md}`, `sketch/tools/{deps.txt,setup.sh,build.sh,upload.sh,monitor.sh,find_port.py,device_test.py,backup_flash.sh,restore_flash.sh,gen_compile_commands.sh}`, `sketch/tools/tests/test_find_port.py`, `sketch/.vscode/*`, `sketch/docs/SETUP.md` (draft), `sketch/examples/Smoke/Smoke.ino` (deleted in Task 3).

**Interfaces — Produces:**
- `find_port.py [--vidpid 303A:1001]` → prints port, exit 0; else exit 2 with `no M5Dial found (VID:PID 303A:1001). Plug in, or hold the knob button while plugging in for download mode.`
- `build.sh <sketch-dir>` → `arduino-cli compile --config-file sketch/arduino-cli.yaml --fqbn "$M5DIAL_FQBN" --build-path sketch/.build/<Name> <dir>`; FQBN in `deps.txt` (expected `m5stack:esp32:m5stack_dial`; confirm via `arduino-cli board listall dial`). Outputs `<Name>.ino.elf` / `.bin` there.
- `upload.sh <sketch-dir> [port]`, `monitor.sh [port]`.
- `device_test.py <sketch-dir> --expect <regex>... [--forbid <regex>...] [--timeout 20] [--no-upload]` → upload, reset via RTS, pass when all `--expect` match in order and no `--forbid` matched; prints log; exit 1 listing unmatched.
- VSCode tasks `Build`, `Upload`, `Monitor`, `Build+Upload+Monitor`, `Device test`, `Host tests` with `${input:sketch}` pick-list.

- [ ] **Step 1:** `brew install arduino-cli`; venv `sketch/tools/.venv` with `pyserial pillow esptool bleak`; **simulation toolchain:** `wokwi-cli` (official install script, version pinned in `deps.txt`, installed into `sketch/tools/bin/`) and **wasi-sdk** (pinned release tarball for the host OS, unpacked to `sketch/tools/wasi-sdk/`, path exported by `setup.sh`). `setup.sh` does all of this + core/lib install from `deps.txt`; all versions recorded in `deps.txt`. Run → `arduino-cli lib list` shows M5Dial 1.0.3; `wokwi-cli --version` and `$WASI_SDK_PATH/bin/clang --version` print the pinned versions.
- [ ] **Step 2: Failing test** `test_find_port.py`: `test_picks_303a_1001`, `test_none_found_exits_2_with_message` (mock `serial.tools.list_ports.comports`). Run `.venv/bin/python -m unittest discover sketch/tools/tests` → FAIL.
- [ ] **Step 3:** Implement `find_port(vidpid: str) -> str | None` + `main()`. Tests PASS; real run prints `/dev/cu.usbmodem…`.
- [ ] **Step 4: Backup before any upload:** `backup_flash.sh` = `esptool --chip esp32s3 -p $(find_port) read_flash 0 0x800000 sketch/backup/m5dial-factory-<date>.bin` + `.sha256`. Run → 8 388 608-byte file. Write `restore_flash.sh` (`write_flash 0 <file>`), don't run.
- [ ] **Step 5:** `Smoke.ino`: `M5Dial.begin(); M5_LOGI("boot: smoke");` fill screen green. `device_test.py examples/Smoke --expect 'boot: smoke'` → PASS.
- [ ] **Step 6:** `gen_compile_commands.sh` (`--only-compilation-database` → `sketch/.build/compile_commands.json`); `.vscode/settings.json` points C/C++ at it.
- [ ] **Step 7:** SETUP.md draft from spec §5.6 items 1–8 as executed (macOS verified; Linux/Windows/WSL marked untested).
- [ ] **Step 8: Commit** `chore(sketch): toolchain, VSCode tasks, device test harness, flash backup`.

### Task 2: Virtual M5Dial spike — display autodetect (go/no-go)  *(issue 2)*

**Files:** `sketch/sim/{wokwi.toml.in,diagram.json}`, `sketch/sim/chips/gc9a01/{gc9a01.chip.json,gc9a01.chip.c,Makefile}`, `sketch/tools/sim.sh`, `sketch/sim/scenarios/Smoke/boot.test.yaml`.

**Interfaces — Produces:**
- `sim.sh <sketch-dir> <scenario.yaml> [--timeout-ms 30000]` → builds via `build.sh`, renders `wokwi.toml` from `wokwi.toml.in` (firmware/elf = that sketch's build output, `[[chip]]` entries for every built chip), runs `wokwi-cli sketch/sim --scenario <yaml> --timeout <ms>`; exit code = wokwi-cli's. Fails fast with a message if `WOKWI_CLI_TOKEN` unset.
- `diagram.json`: `board-esp32-s3-devkitc-1` + `gc9a01` chip wired to the M5Dial display pins (copy MOSI/SCLK/DC/CS/RST/BL from M5GFX's M5Dial board setup in its source, record file+line in a comment in `PORTING_NOTES.md`).
- `gc9a01` chip: SPI device; implements command set M5GFX's GC9A01 panel driver sends (init sequence accepted, `CASET`/`RASET`/`RAMWR` into a 240×240 RGB565 framebuffer); answers ID reads M5GFX autodetect performs.
- Chips build with wasi-sdk following the official Wokwi chip template Makefile; `.chip.wasm` git-ignored, built by `sim.sh`.

- [ ] **Step 1: Failing scenario** `boot.test.yaml`: `wait-serial: 'boot: smoke'` within 20 s; plus screenshot step if supported (display fully green). `sim.sh examples/Smoke sim/scenarios/Smoke/boot.test.yaml` → FAIL (no chip/diagram).
- [ ] **Step 2:** Implement diagram + `gc9a01` chip. Read M5GFX autodetect code path for the Dial to see exactly which reads must answer.
- [ ] **Step 3:** Run → PASS: serial line seen and Smoke reports `M5.getBoard() == board_M5Dial` (add `M5_LOGI("board: %d", M5.getBoard())` to Smoke; expect `board: <id of board_M5Dial>`).
- [ ] **Step 4: Go/no-go.** If autodetect fails: try `cfg.fallback_board = m5::board_t::board_M5Dial` behind `-DM5DD_SIM` (only allowed sim-only difference; record in PORTING_NOTES). If that also fails, **stop and report to owner** (re-plan: spec §5.7 fallback).
- [ ] **Step 5:** Same Smoke ELF still passes `device_test.py` on hardware.
- [ ] **Step 6: Commit** `feat(sim): virtual M5Dial display chip and wokwi harness`.

### Task 3: Virtual M5Dial completion + DialProbe  *(issue 3)*

**Files:** `sketch/sim/chips/{ft3267,pcf8563,ws1850s}/*`, `diagram.json` (add KY-040 on 40/41, pushbutton on 42, buzzer on 3, NeoPixel ×14 on 2, potentiometer on 1, Wokwi Wi-Fi), `sketch/examples/DialProbe/DialProbe.ino`, `sketch/sim/scenarios/DialProbe/*.test.yaml`. Delete `examples/Smoke` (move its scenario/device check to DialProbe).

**Interfaces — Produces:**
- Chip controls (names fixed, used by every later scenario): `ft3267`: `x` (0–239), `y` (0–239), `touched` (0/1). `pcf8563`: starts from host UTC; implements time regs + alarm/timer + IRQ pin. `ws1850s`: `card_present` (0/1), `uid0..uid3` (0–255); MFRC522 register subset for REQA, anticollision CL1, SELECT, HLTA. Pins/addresses copied from M5Unified's M5Dial setup.
- If the KY-040 can't be driven from scenarios, add chip `encoder` with control `steps` (signed; each change emits that many CW/CCW quadrature cycles on 40/41, with a configurable edge interval so fast turns can be simulated) — decision recorded in PORTING_NOTES.
- `DialProbe.ino` (permanent diagnostic sketch, uses Task 4 `EncoderDetent` once it exists — until then raw counts): logs `init: display 240x240`, `init: rtc ok|fail`, `init: touch ok|fail`, `init: rfid ok|fail`, then events `enc: steps=<±n> raw=<n>`, `btn: pressed|released`, `touch: x=<n> y=<n>`, `rtc: YYYY-MM-DD HH:MM:SS` (1 Hz), `rfid: uid=<HEX:..>`.

- [ ] **Step 1: Failing scenarios** in `scenarios/DialProbe/`: `init.test.yaml` (all four `init: … ok`), `encoder.test.yaml` (one CW detent → `enc: steps=1`; one CCW → `enc: steps=-1`), `button.test.yaml` (press → `btn: pressed`, release → `btn: released`), `touch.test.yaml` (x=120,y=60,touched=1 → `touch: x=120 y=60`), `rtc.test.yaml` (two `rtc:` lines 1 s apart), `rfid.test.yaml` (uid DE:AD:BE:EF, card_present=1 → `rfid: uid=DE:AD:BE:EF`). Run all → FAIL.
- [ ] **Step 2:** Implement chips + DialProbe until all PASS.
- [ ] **Step 3:** `device_test.py examples/DialProbe --expect 'init: display 240x240' 'init: rtc ok' 'init: touch ok' 'init: rfid ok' 'rtc: 20'` → PASS on hardware (proves sim and device agree on init).
- [ ] **Step 4:** VSCode tasks `Simulate` (opens Wokwi panel) and `Sim test` (`sim.sh` with scenario pick-list). SETUP.md item 9.
- [ ] **Step 5: Commit** `feat(sim): touch, RTC, RFID chips, input parts, DialProbe`.

### Task 4: Shared library core  *(issue 4)*

**Files:** `libraries/M5DialDemo/{library.properties,src/M5DialDemo.h,src/config.h,src/app_context.h,src/encoder_detent.h,src/m5dd_begin.{h,cpp},src/mooncake/*}`, `test/host/test_encoder_detent.cpp`, `sketch/tools/test_host.sh`. Update DialProbe to use the library.

**Interfaces — Produces:**
- `library.properties`: `name=M5DialDemo`, `depends=M5Dial,M5Unified,M5GFX`, `architectures=esp32`.
- `struct AppContext { M5Canvas* canvas; };`
- `class QuadratureDecoder { public: void reset(uint8_t ab); int8_t update(uint8_t ab); };` (`quadrature_decoder.h`) — header-only, Arduino-free; `ab` = (pinA<<1)|pinB; returns the count delta for the transition using the same state table as M5Dial's (PJRC) `Encoder.h` (±1 per edge, ±2 for a skipped state, 0 for no change), so `DialEncoder` counts exactly like `M5Dial.Encoder` would with working interrupts.
- `class DialEncoder { public: void begin(uint8_t pinA = DIAL_ENCODER_PIN_A, uint8_t pinB = DIAL_ENCODER_PIN_B); int32_t read(); int32_t readAndReset(); void write(int32_t position); };` (`dial_encoder.h/.cpp`) — `INPUT_PULLUP`, `attachInterrupt(CHANGE)` on both pins, `IRAM_ATTR` ISR reads both pins and feeds `QuadratureDecoder`; position guarded by a `portMUX_TYPE` critical section. Global instance `extern DialEncoder dialEncoder;`. `// why:` comment cites m5stack/M5Dial#8.
- `class EncoderDetent { public: explicit EncoderDetent(int countsPerDetent = M5DD_ENCODER_COUNTS_PER_DETENT, int cwSign = M5DD_ENCODER_CW_SIGN); int32_t update(int32_t rawCount); void reset(int32_t rawCount); };` — header-only, no Arduino includes; detent index = floor(raw / countsPerDetent); `update` returns detent delta since last call × `cwSign` (positive = clockwise). `config.h`: `M5DD_ENCODER_COUNTS_PER_DETENT 4`, `M5DD_ENCODER_CW_SIGN` (value from Task 3 encoder scenario).
- `namespace m5dd { void begin(const char* sketchName, M5Canvas& canvas); void runStandalone(MOONCAKE::APP_BASE& app, AppContext& ctx); }` — `begin`: `M5Dial.begin(M5.config(), false, true)` + `dialEncoder.begin()`, serial log level INFO, `canvas.createSprite(240, 240)` 16 bpp, `M5_LOGI("boot: %s", sketchName)`. `runStandalone`: `setUserData(&ctx)`, `onSetup/onCreate`, then forever `M5Dial.update(); app.onRunning();` and on `isGoingClose()||isGoingDestroy()` → `onDestroy()`, `M5_LOGI("app: restart")`, `ESP.restart()`.
- `mooncake/app.h` = `MOONCAKE::APP_BASE` from `main/apps/app.h` (framework, unchanged API); `gui_base.*` with `LGFX_Sprite*` → `M5Canvas*`, `<LovyanGFX.hpp>` → `<M5GFX.h>`.

- [ ] **Step 0: Failing host test** `test_quadrature_decoder.cpp`: one full cycle 00→01→11→10→00 sums to ±4 and the reverse cycle to the opposite sign; A bouncing 00→01→00 nets 0; skipped state 00→11 gives ±2; repeated same state gives 0. Implement `quadrature_decoder.h` → PASS.
- [ ] **Step 1: Failing host test** `test_encoder_detent.cpp`: raw 0→3 → 0 steps; →4 → +1 (cwSign=+1); →0 → −1; →−1 → −1 (floor); →−4 → 0; jump 0→12 → +3; cwSign=−1 inverts. `test_host.sh` = `c++ -std=c++17 -Isrc test/host/*.cpp && ./a.out`. Run → FAIL.
- [ ] **Step 2:** Implement `encoder_detent.h` → PASS.
- [ ] **Step 3:** Implement rest; DialProbe switched to `m5dd::begin` + `dialEncoder` + `EncoderDetent`. All DialProbe sim scenarios + device test still PASS. Add DialProbe scenario `encoder_fast.test.yaml`: 20 CW detents in quick succession while the canvas is pushed every loop → `enc:` lines sum to exactly +20 with no negative step (Review Focus 2).
- [ ] **Step 4: Commit** `feat(lib): M5DialDemo core — app framework, encoder detents, bootstrap`.

### Task 5: Assets + png2rgb565  *(issue 5)*

**Files:** `tools/png2rgb565.py`, `tools/tests/test_png2rgb565.py`, `libraries/M5DialDemo/src/assets/**` (headers + PNGs copied from `main/apps/launcher/launcher_icons`, `main/apps/app_temp_demo/gui/temp_demo_icons`).

**Interfaces — Produces:** CLI `png2rgb565.py <in.png> <out.h> --name <ident> [--bg 0x000000]` emitting `static const uint16_t image_data_<name>[W*H]` like the existing lcd-image-converter headers; API `convert(png_path, bg=0x000000) -> (w, h, list[int])`, `parse_header(path) -> list[int]`.

- [ ] **Step 1: Failing test** `test_regenerates_all_existing_headers`: all 18 PNG/H pairs, `convert(png)[2] == parse_header(h)`. → FAIL.
- [ ] **Step 2:** Implement; pick alpha handling (composite on bg vs ignore) that makes all 18 match, `// why:` note. → PASS.
- [ ] **Step 3:** Copy headers; umbrella `assets/launcher_icons.h`. DialProbe still compiles with assets included.
- [ ] **Step 4: Commit** `feat(lib): image assets and png2rgb565 tool`.

---

## Phase 2 — Port (each task: port app → standalone sketch → launcher wiring → sim scenarios → device smoke)

Common rules for Tasks 6–16:
- Port = copy original app + gui into `src/apps/<app>/`, then replace every hardware call per spec §5.2 (no wrappers), `VideoShit`→`TempDemo`, `LGFX_Sprite`→`M5Canvas`, `_log/printf`→`M5_LOGI`.
- Standalone sketch = `m5dd::begin("<Sketch>", canvas); m5dd::runStandalone(app, ctx);` (~15 lines).
- Every app logs `app: open <Name>` in `onCreate` and `app: close <Name>` in `onDestroy`.
- Scenarios live in `sim/scenarios/<Sketch>/`; device smoke = `device_test.py <sketch> --expect 'boot: <Sketch>' 'app: open <Name>'`.

### Task 6: smooth_menu + Launcher + UserDemo  *(issue 6)*

**Files:** `src/smooth_menu/**`, `src/apps/launcher/**`, `sketch/UserDemo/{UserDemo.ino,secrets.example.h}`, `sim/scenarios/UserDemo/*.test.yaml`.
**Interfaces:** `MOONCAKE::USER_APP::Launcher` (framework API unchanged). Launcher factory (original `launcher.cpp:276-297`) creates apps; unported entries log `app: <name> not ported` and return. Launcher logs `launcher: ready`, `launcher: select <index> <name>` on each step. `UserDemo.ino`: `m5dd::begin("UserDemo", canvas)`; if `ENABLE_FACTORY_TEST` and `M5Dial.BtnA.isPressed()` stays pressed > 1000 ms at boot (looping with `M5Dial.update()`) → double 4 kHz/30 ms tone → factory test (Task 15; until then `factory: not ported`); else launcher loop.
- [ ] **Step 1: Failing scenarios:** `boot.test.yaml` (`launcher: ready`); `cw_selects_next.test.yaml` (one CW detent → `launcher: select 1`); `ccw_wraps.test.yaml` (one CCW from 0 → `launcher: select 7`); `press_hold_release_opens_app.test.yaml` (press, hold 500 ms, release → `app: open` or `not ported` line, then `launcher: ready` again after app quits) — Review Focus 1; `touch_icon_opens.test.yaml` (touch an icon position → its `select`).
- [ ] **Step 2:** Port smooth_menu (include paths) + launcher + UserDemo until scenarios PASS. Golden screenshot `golden/UserDemo/launcher.png` committed if screenshots supported.
- [ ] **Step 3:** Device smoke `--expect 'boot: UserDemo' 'launcher: ready'` → PASS.
- [ ] **Step 4: Commit** `feat(sketch): launcher and smooth_menu on M5Dial API`.

### Tasks 7–10: LCD test, RTC test, Brightness, Temp demo  *(issues 7–10, one each)*

| Task | App class | Source | Sketch | Scenario assertions (serial) |
|---|---|---|---|---|
| 7 | `LCD_Test` | `main/apps/app_lcd_test` | `LcdTest` | `lcd: pattern 0`; CW → `lcd: pattern 1`; press quits → `app: close LCD_Test` |
| 8 | `RTC_Test` | `main/apps/app_rtc_test` | `RtcTest` | two `rtc: 20\d\d-…` lines with increasing seconds |
| 9 | `Set_Brightness` | `main/apps/app_set_brightness` | `Brightness` | CW ×3 → `brightness: <n>` increasing, CCW decreasing, clamped 0–255 |
| 10 | `TempDemo` | `main/apps/app_temp_demo` | `TempDemo` | `temp: frame` lines; CW changes `temp: target=<n>` |

- [ ] **Step 1:** Failing scenarios per table (sketch dir + also via UserDemo launcher: select → open → quit).
- [ ] **Step 2:** Port + sketch + launcher wiring → scenarios PASS.
- [ ] **Step 3:** Device smoke → PASS.
- [ ] **Step 4: Commit** `feat(apps): port <App>`.

### Task 11: RFID test  *(issue 11)*

**Files:** `src/apps/rfid_test/{app_rfid_test.*,gui/*,uid_format.h}`, `test/host/test_uid_format.cpp`, `examples/RfidTest`, scenarios.
**Interfaces:** `std::string formatUid(const uint8_t* bytes, uint8_t len)` → uppercase hex joined by `:`. Poll in `onRunning()`: `M5Dial.Rfid.PICC_IsNewCardPresent() && M5Dial.Rfid.PICC_ReadCardSerial()` → GUI update, `M5Dial.Speaker.tone(4000, 30)`, `M5_LOGI("rfid: uid=%s", …)`, `PICC_HaltA()`. Log `rfid: ready` after init.
- [ ] **Step 1: Failing host test** (4-byte, 7-byte, leading 0x00 → `00:…`) → implement → PASS.
- [ ] **Step 2: Failing scenario** `card.test.yaml`: `rfid: ready`; set uid 04:A1:B2:C3 + card_present=1 → `rfid: uid=04:A1:B2:C3`; remove card, present again → second uid line (HaltA/re-detect works).
- [ ] **Step 3:** Port → PASS. Device smoke `--expect 'rfid: ready'`.
- [ ] **Step 4: Commit** `feat(apps): port RFID test to MFRC522 API`.

### Task 12: Wi-Fi scan  *(issue 12)*

**Files:** `src/apps/wifi_scan/**`, `examples/WifiScan`, scenarios.
**Interfaces:** `WiFi.mode(WIFI_STA); WiFi.scanNetworks(true)`; poll `WiFi.scanComplete()` in `onRunning()`; read count first, then `SSID(i)/RSSI(i)` (d49e20e intent); `WiFi.scanDelete()`; `WiFi.mode(WIFI_OFF)` in `onDestroy`. Logs `wifi: ap_count=<n>`, `wifi: <rssi> <ssid>`.
- [ ] **Step 1: Failing scenario** `scan.test.yaml`: `wifi: ap_count=` ≥1 and a line containing `Wokwi-GUEST`; trigger rescan via press → second `ap_count`.
- [ ] **Step 2:** Port → PASS. Device smoke `--expect 'wifi: ap_count=[1-9]' --timeout 30`.
- [ ] **Step 3: Commit** `feat(apps): port Wi-Fi scan to WiFi.h`.

### Task 13: BLE server  *(issue 13)*

**Files:** `src/apps/ble_server/{app_ble_server.*,gui/*,ble_heart_rate.{h,cpp}}`, `examples/BleServer`, `tools/ble_check.py`.
**Interfaces:** `class BleHeartRateServer { void begin(const char* deviceName); bool isConnected() const; bool isSubscribed() const; uint8_t lastValue() const; void loop(); }` on NimBLE-Arduino (`NimBLEDevice`, `NimBLEServerCallbacks`, `NimBLECharacteristicCallbacks`). Services/values from `gatt_svr.c`/`ble_demo.c`: HRS `0x180D` (`0x2A37` notify, `0x2A38` read), Device Info `0x180A` (`0x2A29`, `0x2A24`), same device name, notify interval and counter behaviour. Logs `ble: advertising <name>`, `ble: connected=0|1`, `ble: subscribed=0|1`.
`ble_check.py --name <name> [--notifications 3] [--timeout 30]` (bleak): scan → connect → read 0x2A38 → subscribe 0x2A37 → exit 0 after N notifications with increasing counter.
- [ ] **Step 1:** Port. No sim scenario (Wokwi has no BLE); record BLE as device-only in PORTING_NOTES.
- [ ] **Step 2: Device test:** `device_test.py examples/BleServer --expect 'ble: advertising'` then (no upload) run `ble_check.py` while `device_test.py --no-upload --expect 'ble: connected=1' 'ble: subscribed=1'` → both PASS.
- [ ] **Step 3: Commit** `feat(apps): port BLE heart-rate server to NimBLE-Arduino`.

### Task 14: More menu, power, memory soak, partitions  *(issue 14)*

**Files:** `src/apps/more_menu/**` (drop the three LVGL entries + `lv_demos.h`), `examples/MemSoak/MemSoak.ino`, scenarios.
**Interfaces:** Power Off → `M5Dial.Power.powerOff()` (log `power: off` first). `MemSoak`: programmatically runs WifiScan app (until `ap_count`), destroys it, BleServer app (5 s), destroys it, ×3; logs `heap: free=<n> min=<n>` after each; ends with `soak: done`.
- [ ] **Step 1: Failing scenarios:** `more_menu_quit.test.yaml` (Quit → back to `launcher: ready`); `power_off.test.yaml` (`power: off` logged; sim end accepted).
- [ ] **Step 2:** Port → PASS.
- [ ] **Step 3: Soak (Review Focus 5):** `device_test.py examples/MemSoak --expect 'soak: done' --forbid 'boot: MemSoak.*boot: MemSoak' --timeout 180` → PASS (no reboot); record min heap in PORTING_NOTES.
- [ ] **Step 4: Partition check:** UserDemo build size vs max; if > 90 %, add `UserDemo/partitions.csv` (4 MB app as original, no FAT) and note it; else record percentage.
- [ ] **Step 5: Commit** `feat(apps): port More menu and power-off; memory soak`.

### Task 15: Factory test  *(issue 15)*

**Files:** `src/apps/factory_test/{app_factory_test.*,steps/*,report.h,verdict_prompt.*}`, `test/host/test_report.cpp`, scenarios; deps add Adafruit NeoPixel.
**Interfaces:**
- `enum class Step { Lcd, Rtc, Rfid, Io }; enum class Verdict { Pending, Pass, Fail, Skip };`
- `class FactoryReport { void set(Step, Verdict); Verdict get(Step) const; bool allPassedOrSkipped() const; std::string line(Step) const; std::string serial() const; };` — `line(Io)` e.g. `"IO: SKIP"`; `serial()` exactly `"factory: LCD=PASS RTC=PASS RFID=FAIL IO=SKIP"`.
- `Verdict promptVerdict(M5Canvas& canvas, const char* stepName)` — round screen, options PASS/FAIL/SKIP, `dialEncoder` + `EncoderDetent` move the highlight (CW = next), `BtnA.wasPressed()` confirms, default PASS; logs `factory: verdict <step>=<v>`.
- IO step: NeoPixel 14 px GPIO2; `analogReadMilliVolts(1)`; LED count = original (`3000 - mv`, `>2900`→3000, `/500`); Port A scan via `M5Dial.Ex_I2C` every 800 ms. Logs `factory: io adc=<mv> leds=<n> i2c=[<addrs>]`.
- Flow: LCD → RTC → RFID → IO, each + `promptVerdict` → summary screen + `M5_LOGI("%s", report.serial().c_str())` → press → RTC wake-up (`M5Dial.Rtc.setAlarmIRQ(4)`, `M5Dial.Power.powerOff()`).
- [ ] **Step 1: Failing host test** `test_report.cpp` (all Pending initially; `line`/`serial` exact strings; `allPassedOrSkipped` false on any Fail/Pending) → implement → PASS.
- [ ] **Step 2: Failing scenarios:** `enter.test.yaml` (button held at boot 1.2 s → `factory: step LCD`); `skip_io.test.yaml` (PASS, PASS, present card + PASS, IO: pot set mid → `factory: io adc=` then CW×2 → SKIP → `factory: LCD=PASS RTC=PASS RFID=PASS IO=SKIP`); `io_leds.test.yaml` (pot at 0 V vs 3.3 V → `leds=` 6 vs 0, matching original mapping).
- [ ] **Step 3:** Port → PASS. Device smoke: boot without button → `launcher: ready` (factory not entered by accident).
- [ ] **Step 4:** `docs/FACTORY_TEST_JIG.md` wiring: Port B GPIO2 → WS2812 DIN (14 LEDs, 5 V, GND), GPIO1 ← pot wiper 0–3.3 V, Port A any I2C device.
- [ ] **Step 5: Commit** `feat(apps): factory test with operator verdicts and summary`.

### Task 16: LvglDemo  *(issue 16)*

**Files:** `examples/LvglDemo/{LvglDemo.ino,lv_conf.h}`, scenarios; deps add `lvgl` 9.x.
**Interfaces:** Uses M5Dial directly + `dialEncoder` + `EncoderDetent`; not the app framework. `build.sh` adds `--build-property compiler.cpp.extra_flags=-DLV_CONF_INCLUDE_SIMPLE -I<sketch dir>` only when the sketch dir contains `lv_conf.h`. Display `lv_display_create(240,240)`, partial buffer 240×24 px, flush → `M5Dial.Display.pushImage`. Encoder indev (`LV_INDEV_TYPE_ENCODER`, diff from `dialEncoder` via `EncoderDetent`, press from `BtnA`), pointer indev from `Touch.getDetail()`. UI: `lv_arc` 0–100 in an `lv_group`, centred value label, button toggles arc colour. Logs `lvgl: ready`, `lvgl: value=<n>`.
- [ ] **Step 1: Failing scenario** `knob.test.yaml`: `lvgl: ready`; CW×5 → `lvgl: value=5`.
- [ ] **Step 2:** Implement → PASS; all other sketches still compile (no `lv_conf.h` leakage). Device smoke `--expect 'lvgl: ready'`.
- [ ] **Step 3: Commit** `feat(examples): LVGL 9 starter sketch`.

---

## Phase 3 — Wrap-up

### Task 17: CI  *(issue 17)*

**Files:** `.github/workflows/sketch.yml`.
- [ ] **Step 1:** On push/PR touching `sketch/**`: setup arduino-cli (cache `~/.arduino15`), `setup.sh --ci`, `test_host.sh`, python unit tests, `build.sh` every sketch, build chips (wasi-sdk), run every `sim/scenarios/**/*.test.yaml` via `sim.sh` with `WOKWI_CLI_TOKEN` secret; CI copies `secrets.example.h` → `secrets.h`.
- [ ] **Step 2:** Push → green. Scratch commit breaking one scenario → red; drop it.
- [ ] **Step 3: Commit** `ci: build sketches, host tests, wokwi scenarios`.

### Task 18: Documentation  *(issue 18)*

**Files:** `sketch/docs/{SETUP.md,PORTING_NOTES.md,TEST_CHECKLIST.md}`, `sketch/README.md`, root `README.md` (one link line).
- [ ] **Step 1:** Fresh-environment run of SETUP.md verbatim (temp `ARDUINO_DIRECTORIES_DATA` + new venv) → build, simulate, upload UserDemo; fix every deviation.
- [ ] **Step 2:** PORTING_NOTES: spec §5.2 as built, original→M5Dial idiom table (e.g. `!btn.read()` loop → `BtnA.isPressed()` + `update()`), encoder detent/sign, sim fallback (if used), heap numbers, partition result, known differences.
- [ ] **Step 3:** TEST_CHECKLIST = the Task 19 list, one section per sketch.
- [ ] **Step 4: Commit** `docs(sketch): setup, porting notes, test checklist`.

### Task 19: Human acceptance on the real device  *(issue 19 — the only task needing the owner)*

Builder flashes each sketch, then walks the owner through `TEST_CHECKLIST.md`, recording results in the issue:
- [ ] Launcher: icon ring looks like original; knob scroll smooth; **clockwise = next**; touch opens icon.
- [ ] LCD, RTC, Brightness, TempDemo: visuals as original; backlight visibly changes; RTC shows correct local time.
- [ ] Buzzer audible on RFID read and factory entry.
- [ ] RFID: a real card/tag shows its UID.
- [ ] Wi-Fi scan lists the home APs.
- [ ] BLE: optional phone check with nRF Connect (automated Mac check already passed).
- [ ] More → Power Off powers down; button wakes.
- [ ] Factory test on bare device (IO = SKIP) and, once built, with the jig (LEDs follow pot, Port A device listed); RTC wake-up restarts device after ~4 s.
- [ ] LvglDemo arc follows the knob.
- [ ] Optional: `restore_flash.sh` round-trip to confirm the backup restores the original firmware.
- [ ] Any defect → new issue; close epic when all ticked.

### Task 20: Wokwi contribution package  *(issue 20 — submission itself is the owner's call)*

**Files:** `sketch/sim/CONTRIBUTING-WOKWI.md`, `sketch/sim/chips/*/README.md` (final), `sketch/sim/board-m5dial/` (Wokwi board definition draft: `board.json` + `board.svg` with M5Dial pin map, if Wokwi's board format allows a custom board), `.github/workflows/wokwi-chips.yml`.
- [ ] **Step 1:** Workflow builds every chip and attaches `<chip>-chip.zip` artefacts (template-compatible), so each chip can be used from a GitHub release by `wokwi.toml`/`diagram.json` like community chips.
- [ ] **Step 2:** Per-chip README completed from what Tasks 2–3 learned: implemented registers/commands, controls, known gaps; one minimal example `diagram.json` per chip using only stock ESP32-S3 + that chip.
- [ ] **Step 3:** `CONTRIBUTING-WOKWI.md`: how to split each chip into its own repo (`git subtree split --prefix sketch/sim/chips/<chip>`), Wokwi's community-chip / board submission routes and what each needs, licence statement (MIT).
- [ ] **Step 4:** Owner reviews; any outward step (new public repos, PR/post to Wokwi) only on the owner's explicit go.
- [ ] **Step 5: Commit** `chore(sim): package Wokwi chips for upstream contribution`.

---

## GitHub tracking

**Created 2026-10-30:** epic [#21](https://github.com/beyonddemise/M5Dial-UserDemo/issues/21); issues #1–#20 = Tasks 1–20 (numbers match), attached as sub-issues of #21.

- Epic "Port UserDemo to Arduino sketches (M5Dial library)" with spec + plan links and the 20-item task list.
- Issues 1–20 = Tasks 1–20. Labels: `arduino-port` + `infra` (1, 4, 5, 17), `sim` (2, 3, 20), `app` (6–16), `docs` (18), `human_needed` (19), `upstream` (20).
- Repo `beyonddemise/M5Dial-UserDemo`. Dependencies: 2←1; 3←2; 4←3; 5←1; 6←4,5; 7–13←6; 14←6,12,13; 15←6,11; 16←4; 17←1–16; 18←all of 1–17; 19←18; 20←3,17.

## Review (to be filled in after execution)
