# M5Dial UserDemo → Arduino Sketch Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every app of the ESP-IDF M5Dial UserDemo runs again as Arduino `.ino` sketches under `/sketch`, built with arduino-cli from VSCode, on the official M5Dial library.

**Architecture:** `/sketch` is an Arduino sketchbook. A local library `M5DialDemo` holds a thin HAL facade over `M5Dial` that preserves the original `hal->encoder/tp/rtc/display/canvas/buzz` call shape, plus the ported Mooncake app base, `smooth_menu` and all apps. `UserDemo.ino` is the launcher; `examples/<App>/<App>.ino` run one app each. Verification is host-side unit tests for pure logic plus hardware-in-the-loop (flash + serial log assertions) on the attached M5Dial.

**Tech Stack:** arduino-cli, M5Stack ESP32 board package (Arduino-ESP32 3.x), M5Dial 1.0.3 + M5Unified + M5GFX, NimBLE-Arduino, Adafruit NeoPixel, lvgl 9.x (LvglDemo only), Python 3 (Pillow, pyserial) for tools, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-30-arduino-sketch-port-design.md` (read it first; this plan argues from it).

**Original source (read-only reference):** `main/` — never modified.

## Global Constraints

- All new files live under `/sketch` (plus `.github/workflows/` for CI); `main/`, `components/`, root build files untouched.
- Hardware access only via `M5Dial` (`Display`, `Touch`, `Rtc`, `Speaker`, `Power`, `BtnA`, `Encoder`, `Rfid`, `Ex_I2C`). No vendored LovyanGFX/ESP32Encoder/FT3267/PCF8563/rc522 code. Do **not** install the LovyanGFX library (clashes with M5GFX).
- Library versions pinned in one place: `sketch/arduino-cli.yaml` comment block + `sketch/tools/deps.txt`; SETUP.md quotes the same values. Pick latest stable at Task 1, record them, never float.
- Wi-Fi credentials only in git-ignored `sketch/UserDemo/secrets.h`; `secrets.example.h` committed.
- Serial: 115200 baud. All diagnostic output goes through `M5DD_LOG(tag, fmt, ...)` → line format `[M5DD] <tag>: <message>` (HIL tests grep this).
- `ENABLE_FACTORY_TEST` defaults to on in `config.h`.
- Fonts: `GUI_FONT_CN_BIG = &fonts::efontCN_24`, `GUI_FONT_CN_SMALL = &fonts::efontCN_16_b`.
- Code docs: class + method level doc comments (Doxygen `/** */`), `// why:` comments on non-obvious logic (owner's CLAUDE.md).
- Keep upstream author headers/licences on ported files (Forairaaaaa, smooth_menu MIT, NimBLE Apache-2.0 example origin).
- One GitHub issue ↔ one task ↔ one branch `port/<NN>-<slug>` ↔ one PR against `main`. Issue closes after the owner ticks the on-device checklist.
- First flash only after the full flash backup in Task 1 exists.

## Review Focus

1. **Button polarity:** original `encoder.btn.read()` returns the debounced *pin level* — `true` = released, `false` = pressed (42 call sites test `!btn.read()`). Facade must keep this; `M5Dial.BtnA.isPressed()` is the inverse. → host test in Task 2.
2. **Encoder step size and zero crossing:** original = half-quad (2 counts/cycle) ÷ 2; M5Dial `ENCODER` = x4. One physical detent must produce exactly one `wasMoved()`; detents around count 0 must behave like any other (truncating division makes a double-wide dead zone). → host test + HIL in Task 2.
3. **Encoder direction sign:** original sets `_direction = 1` when the count *decreases*. Same physical rotation must move menus the same way as the original firmware. → HIL check in Task 2 against the backup firmware observation recorded in Task 1.
4. **Serial port changes / device in download mode:** port name differs per plug-in; a crashed sketch can drop USB-CDC. Scripts must auto-detect by VID:PID `303A:1001` and fail with a clear message. → test in Task 1.
5. **Heap exhaustion with Wi-Fi + BLE + 115 KB canvas (no PSRAM):** opening Wi-Fi scan then BLE then returning to launcher must not crash. → HIL soak in Task 12.

---

## File Structure (locked)

```
sketch/
  README.md  arduino-cli.yaml  .gitignore
  docs/SETUP.md  PORTING_NOTES.md  TEST_CHECKLIST.md  FACTORY_TEST_JIG.md
  tools/deps.txt  setup.sh  build.sh  upload.sh  monitor.sh  find_port.py  hil.py
        backup_flash.sh  restore_flash.sh  gen_compile_commands.sh  png2rgb565.py
        test_host.sh  tests/test_png2rgb565.py  tests/test_find_port.py
  .vscode/tasks.json  settings.json  extensions.json  c_cpp_properties.json
  backup/ (git-ignored)
  libraries/M5DialDemo/
    library.properties
    src/M5DialDemo.h  config.h  log.h
    src/hal/hal.h  hal.cpp  encoder_facade.h/.cpp  button_facade.h/.cpp
            touch_facade.h  rtc_facade.h  buzzer_facade.h  detent_quantizer.h
    src/mooncake/app.h  gui_base.h  gui_base.cpp
    src/smooth_menu/...           (ported from main/apps/utilities/smooth_menu/src + LICENSE)
    src/assets/launcher_icons/*.h  temp_demo_icons/*.h  png/*.png
    src/apps/launcher/  lcd_test/  rtc_test/  rfid_test/  set_brightness/  temp_demo/
             wifi_scan/  ble_server/  more_menu/  factory_test/   (each: app_*.h/.cpp + gui/)
    test/host/  (host unit tests, not compiled by Arduino)
  UserDemo/UserDemo.ino  secrets.example.h
  examples/LcdTest RtcTest RfidTest Brightness TempDemo WifiScan BleServer LvglDemo/
```

---

### Task 1: Toolchain, skeleton, flash backup  *(issue 1)*

**Files:**
- Create: `sketch/arduino-cli.yaml`, `sketch/.gitignore`, `sketch/README.md`, `sketch/tools/{deps.txt,setup.sh,build.sh,upload.sh,monitor.sh,find_port.py,hil.py,backup_flash.sh,restore_flash.sh,gen_compile_commands.sh}`, `sketch/tools/tests/test_find_port.py`, `sketch/.vscode/*`, `sketch/docs/SETUP.md` (draft), `sketch/examples/Blink/Blink.ino` (temporary smoke sketch, deleted in Task 4)

**Interfaces — Produces:**
- `tools/find_port.py [--vidpid 303A:1001]` → prints port path, exit 0; exit 2 + message `no M5Dial found (VID:PID 303A:1001). Plug in, or hold the knob button while plugging in for download mode.`
- `tools/build.sh <sketch-dir>` → `arduino-cli compile --config-file sketch/arduino-cli.yaml --fqbn "$M5DIAL_FQBN" --warnings default --build-path sketch/.build/<name> <dir>`; FQBN from `deps.txt` (`m5stack:esp32:m5stack_dial` — confirm with `arduino-cli board listall dial`).
- `tools/upload.sh <sketch-dir> [port]`, `tools/monitor.sh [port]` (115200).
- `tools/hil.py <sketch-dir> --expect <regex>... [--timeout 20] [--no-upload]` → uploads, opens serial, resets via RTS, passes when every regex matched in order, prints captured log; exit 1 on timeout listing unmatched regexes.
- VSCode tasks: `Build`, `Upload`, `Monitor`, `Build+Upload+Monitor`, `HIL test`, `Host tests`, each with `${input:sketch}` pick-list of sketch dirs.

- [ ] **Step 1:** Install toolchain: `brew install arduino-cli`; `python3 -m venv sketch/tools/.venv && sketch/tools/.venv/bin/pip install pyserial pillow esptool`. Write `setup.sh` to do exactly this plus core + libs install from `deps.txt`. Run it. Expected: `arduino-cli core list` shows the m5stack core; `lib list` shows M5Dial 1.0.3.
- [ ] **Step 2: Write failing test** `tests/test_find_port.py`: `test_picks_303a_1001` (fake `serial.tools.list_ports` entries incl. one `303A:1001` → returns its device), `test_none_found_exits_2_with_message` (asserts exit code 2 and message text above). Run `sketch/tools/.venv/bin/python -m unittest discover sketch/tools/tests` → FAIL (module missing).
- [ ] **Step 3:** Implement `find_port.py` (`find_port(vidpid: str) -> str | None`, `main()`). Re-run → PASS. Run for real → prints `/dev/cu.usbmodem…`.
- [ ] **Step 4: Backup + baseline (before any upload!):** `backup_flash.sh` → `esptool --chip esp32s3 -p $(find_port) read_flash 0 0x800000 sketch/backup/m5dial-factory-$(date +%F).bin` + `sha256sum` file. Run it; expected 8 388 608-byte file. Write `restore_flash.sh` (`write_flash 0 <file>`), do not run it. Record in `PORTING_NOTES.md`: on the original firmware, ask owner which way a clockwise turn moves the launcher (needed by Task 2).
- [ ] **Step 5:** Blink smoke sketch (no library yet): `M5Dial.begin(); Serial.println("[M5DD] boot: smoke");` and fill the screen green. `tools/hil.py sketch/examples/Blink --expect 'boot: smoke'` → PASS. Ask owner: screen green?
- [ ] **Step 6:** `gen_compile_commands.sh` (`arduino-cli compile --only-compilation-database`, copy to `sketch/.build/compile_commands.json`), `.vscode/settings.json` points C/C++ `compileCommands` there. Open `Blink.ino` in VSCode → no red squiggles on `M5Dial`.
- [ ] **Step 7:** SETUP.md draft covering spec §5.6 points 1–9 as actually executed above (macOS verified; Linux/Windows/WSL from `hotplugSetup.sh` notes marked "untested").
- [ ] **Step 8: Commit** `chore(sketch): toolchain, VSCode tasks, HIL harness, flash backup tooling` (backup binary is git-ignored).

### Task 2: Shared library core — HAL facade, logging, Mooncake base  *(issue 2)*

**Files:** Create `libraries/M5DialDemo/library.properties` (`name=M5DialDemo`, `depends=M5Dial,M5Unified,M5GFX`, `architectures=esp32`), `src/M5DialDemo.h`, `src/config.h`, `src/log.h`, `src/hal/*`, `src/mooncake/*`, `test/host/{test_detent_quantizer.cpp,test_button_facade.cpp,run.sh}`, `sketch/tools/test_host.sh`, `examples/HalProbe/HalProbe.ino` (kept: diagnostic sketch).

**Interfaces — Produces** (namespace `HAL`, mirrors `main/hal/*` public API used by apps):
- `class DetentQuantizer { explicit DetentQuantizer(int countsPerStep); bool update(int32_t rawCount); int direction() const; void reset(int32_t rawCount); }` — header-only, no Arduino deps; step index = floor(raw / countsPerStep); `update` returns true when step index changed; `direction()` = `+1` when step index *decreased*, `-1` when increased (original sign).
- `class Encoder { bool wasMoved(bool updateDirection = false); int getDirection(); Button btn; void begin(); }` — wraps `M5Dial.Encoder.read()` through `DetentQuantizer(kEncoderCountsPerStep)`, `constexpr int kEncoderCountsPerStep = 4;` in `config.h`.
- `class Button { bool read(); bool pressed(); bool released(); void begin(); void setPin(int); }` — `read()` returns `true` when **released** (debounced, 25 ms, matching `main/hal/utils/Button`), driven by an injectable `std::function<bool()> rawPressed` (default: `M5Dial.BtnA.isPressed()` after `M5Dial.update()`), `millis` injectable for host tests.
- `struct TouchPoint_t { uint8_t touch_num = 0; int x = -1; int y = -1; };` `class Touch { bool isTouched(); void update(); TouchPoint_t getTouchPointBuffer(); const TouchPoint_t& readPos(); }` over `M5Dial.Touch.getDetail()`.
- `class Rtc { int getTime(tm& t); int setAlarmIRQ(int afterSeconds); void clearIRQ(); void disableIRQ(); }` over `M5Dial.Rtc`.
- `class Buzzer { void tone(unsigned freq, unsigned long durationMs = 0); void noTone(); }` over `M5Dial.Speaker`.
- `class HAL { M5GFX& display; LGFX_Sprite* canvas; Buzzer buzz; Encoder encoder; Touch tp; Rtc rtc; void init(); void powerOn(); void powerOff(); }` — `init()` = `M5Dial.begin(cfg, true, true)`, canvas `createSprite(240,240)` 16 bpp. If M5GFX lacks the `LGFX_Sprite` name, `using LGFX_Sprite = M5Canvas;` in `hal.h`.
- `M5DD_LOG(tag, fmt, ...)` in `log.h`.
- `mooncake/app.h` = `MOONCAKE::APP_BASE` verbatim from `main/apps/app.h`; `gui_base.*` from `main/apps/utilities/gui_base/` with `<LovyanGFX.hpp>` → `<M5GFX.h>`.

- [ ] **Step 1: Failing host tests.** `test_detent_quantizer.cpp`: `counts 0..3 → no move`, `4 → moved, direction -1`, `back to 0 → moved, direction +1`, `-1 → moved (floor), -4 → no further move`, `+8 in one jump → one move`. `test_button_facade.cpp`: raw not pressed → `read()==true`; raw pressed for 30 ms → `read()==false`, `pressed()==true` once; bounce 5 ms → still `true`. `tools/test_host.sh` = `c++ -std=c++17 -I src test/host/*.cpp -o /tmp/... && run`. Run → FAIL (headers missing).
- [ ] **Step 2:** Implement `detent_quantizer.h`, `button_facade.h/.cpp` (core logic Arduino-free). Run `test_host.sh` → all PASS.
- [ ] **Step 3:** Implement remaining facades, `HAL`, `log.h`, `config.h`, mooncake. `build.sh examples/HalProbe` → compiles.
- [ ] **Step 4: HalProbe** logs `init: display WxH`, `init: rtc ok`, `init: rfid ok|missing`, then events `enc: dir=±1`, `btn: down|up`, `touch: x=.. y=..`, `rtc: YYYY-MM-DD HH:MM:SS` each second. HIL: `hil.py examples/HalProbe --expect 'init: display 240x240' 'init: rtc ok' 'rtc: 20'` → PASS.
- [ ] **Step 5: Owner checks (prompt):** turn exactly one detent CW → exactly one `enc:` line with the sign matching the Task 1 observation; press/release → one `btn: down`, one `btn: up`; touch centre → x,y ≈ 120,120. Adjust `kEncoderCountsPerStep`/sign if not; re-run host tests.
- [ ] **Step 6: Commit** `feat(lib): M5DialDemo HAL facade over M5Dial, logging, mooncake base`.

### Task 3: Assets + png2rgb565  *(issue 3)*

**Files:** Create `tools/png2rgb565.py`, `tools/tests/test_png2rgb565.py`, `libraries/M5DialDemo/src/assets/{launcher_icons,temp_demo_icons}/*.h` + `assets/png/*.png` (copied from `main/apps/launcher/launcher_icons`, `main/apps/app_temp_demo/gui/temp_demo_icons`).

**Interfaces — Produces:** `png2rgb565.py <in.png> <out.h> --name <ident> [--bg 0x000000]` emitting the same layout as the existing headers (`static const uint16_t image_data_<name>[W*H]`, header comment noting tool). Python API `convert(png_path, bg=0x000000) -> (w, h, list[int])`, `parse_header(h_path) -> list[int]`.

- [ ] **Step 1: Failing test** `test_regenerates_all_existing_headers`: for each of the 18 PNG/H pairs, `convert(png) == parse_header(h)` element-wise. Run → FAIL.
- [ ] **Step 2:** Implement; alpha handling (composite on `--bg` vs ignore alpha) chosen by whichever makes all 18 match; `// why:` comment records it. Run → PASS.
- [ ] **Step 3:** Copy headers into library unchanged; one umbrella `assets/launcher_icons.h` like the original. `build.sh examples/HalProbe` still compiles with assets included.
- [ ] **Step 4: Commit** `feat(lib): image assets and png2rgb565 regeneration tool`.

### Task 4: smooth_menu + Launcher + UserDemo.ino  *(issue 4)*

**Files:** Create `src/smooth_menu/**` (from `main/apps/utilities/smooth_menu/src` + LICENSE), `src/apps/launcher/**` (from `main/apps/launcher`), `sketch/UserDemo/UserDemo.ino`, `secrets.example.h`. Delete `examples/Blink`.

**Interfaces:** Consumes Task 2 `HAL`, Task 3 icons. Produces `MOONCAKE::USER_APP::Launcher` (unchanged API). Launcher's app factory switch (original `launcher.cpp:276-297`) creates apps from later tasks; entries not yet ported open a placeholder that logs `app: <name> not ported` and closes. Launcher logs `app: open <name>` / `app: close <name>`.
`UserDemo.ino`: `setup()` = `hal.init()` + factory-test check (button held >1000 ms at boot → double 4 kHz/30 ms beep → FactoryTest; guarded by `#ifdef ENABLE_FACTORY_TEST`, FactoryTest itself arrives in Task 13 — until then log `factory: not ported`); `loop()` = `launcher.onRunning()`.

- [ ] **Step 1:** Port smooth_menu (include paths only). Port launcher (HAL type changes only). `build.sh UserDemo` → compiles, zero new warnings in our files.
- [ ] **Step 2: HIL:** `hil.py UserDemo --expect 'boot: UserDemo' 'launcher: ready'` → PASS.
- [ ] **Step 3: Owner checks:** icons ring renders like original, knob scrolls smoothly, press opens placeholder and returns. Add Launcher section to `TEST_CHECKLIST.md`.
- [ ] **Step 4: Commit** `feat(sketch): launcher and smooth_menu on M5Dial library`.

### Tasks 5–8: Simple apps — LCD test, RTC test, Brightness, Temp demo  *(issues 5–8, one each)*

Same shape for each; one task, branch and PR per app.

| Task | App class (keep name) | Source | Example sketch | HIL expects | Owner checks |
|---|---|---|---|---|---|
| 5 | `LCD_Test` | `main/apps/app_lcd_test` | `examples/LcdTest` | `app: open LCD_Test`, `lcd: pattern 0` | colour patterns cycle with knob, quit works |
| 6 | `RTC_Test` | `main/apps/app_rtc_test` | `examples/RtcTest` | `rtc: 20\d\d-` twice, seconds increasing | time shown and ticking |
| 7 | `Set_Brightness` | `main/apps/app_set_brightness` | `examples/Brightness` | `brightness: \d+` after encoder move | backlight visibly changes |
| 8 | `VideoShit` (display name unchanged) | `main/apps/app_temp_demo` | `examples/TempDemo` | `app: open Video Shit`, `temp: frame` | animation + button images as original |

- [ ] **Step 1:** Port `src/apps/<app>/` (app + gui, HAL types/includes only; add `M5DD_LOG` lines listed above).
- [ ] **Step 2:** Example sketch: `HAL::HAL hal; <App> app;` — `setup()`: `hal.init(); app.setUserData(&hal); app.onSetup(); app.onCreate();` `loop()`: `app.onRunning(); if (app.isGoingClose()||app.isGoingDestroy()) { app.onDestroy(); M5DD_LOG("app","restart"); ESP.restart(); }` — put this in one shared helper `runStandalone(APP_BASE&, HAL&)` in `M5DialDemo.h` (created in Task 5, reused after).
- [ ] **Step 3:** Wire into launcher factory switch. `build.sh` both sketches → compile.
- [ ] **Step 4:** HIL both sketches with the expects above → PASS. Prompt owner checks; append to `TEST_CHECKLIST.md`.
- [ ] **Step 5: Commit** `feat(apps): port <App>`.

### Task 9: RFID test  *(issue 9)*

**Files:** `src/apps/rfid_test/**`, `examples/RfidTest`.
**Interfaces:** Replace `esp-idf-rc522` event callback with polling in `onRunning()`: `if (M5Dial.Rfid.PICC_IsNewCardPresent() && M5Dial.Rfid.PICC_ReadCardSerial())` → format UID as uppercase hex `AA:BB:CC:DD`, hand to GUI the same way the callback did, beep 4 kHz/30 ms, log `rfid: uid=<hex>`; then `PICC_HaltA()`.
- [ ] **Step 1:** Host test `test/host/test_uid_format.cpp` for `std::string formatUid(const uint8_t* b, uint8_t n)` (4- and 7-byte UIDs, leading zero bytes). FAIL → implement in `src/apps/rfid_test/uid_format.h` → PASS.
- [ ] **Step 2:** Port app/gui, sketch, launcher wiring; compile both.
- [ ] **Step 3:** HIL `--expect 'app: open RFID_Test' 'rfid: ready'`; then prompt owner to present a card, `hil.py --no-upload --expect 'rfid: uid=[0-9A-F:]+' --timeout 60` → PASS.
- [ ] **Step 4: Commit** `feat(apps): port RFID test to MFRC522 API`.

### Task 10: Wi-Fi scan  *(issue 10)*

**Files:** `src/apps/wifi_scan/**`, `src/apps/common/wifi_common.{h,cpp}` (Arduino replacement for `wifi_common_test.cpp` APIs the apps call), `examples/WifiScan`.
**Interfaces:** `WiFi.mode(WIFI_STA); WiFi.scanNetworks(/*async*/true)`; poll `WiFi.scanComplete()` in `onRunning()`; read count first, then records (`SSID(i)`, `RSSI(i)`) — the d49e20e fix intent; `WiFi.scanDelete()`; on close `WiFi.mode(WIFI_OFF)`. Log `wifi: ap_count=<n>`, then one `wifi: <rssi> <ssid>` per AP.
- [ ] **Step 1:** Port, compile.
- [ ] **Step 2:** HIL `--expect 'wifi: ap_count=[1-9]'` (owner's environment has APs) → PASS. Repeat scan via knob/press → second `ap_count` line, no crash.
- [ ] **Step 3: Commit** `feat(apps): port Wi-Fi scan to WiFi.h`.

### Task 11: BLE server  *(issue 11)*

**Files:** `src/apps/ble_server/**`, `src/apps/common/ble_demo.{h,cpp}` (NimBLE-Arduino rewrite of `ble_demo.c` + `gatt_svr.c`), `examples/BleServer`.
**Interfaces:** Keep `void ble_demo_start(); BleDemoInfos_t* ble_demo_get_infos();` with `const char* device_name; int* sending_data; bool* is_connected; bool* is_subscribed;` (same fields the GUI reads). Services: Heart Rate `0x180D` (Measurement `0x2A37` notify, Body Sensor Location `0x2A38` read), Device Info `0x180A` (Manufacturer `0x2A29`, Model `0x2A24`) — copy string values and notify interval from `gatt_svr.c`/`ble_demo.c`. Device name as original. Log `ble: advertising <name>`, `ble: connected=0|1`, `ble: subscribed=0|1`.
- [ ] **Step 1:** Port, compile.
- [ ] **Step 2:** HIL `--expect 'ble: advertising'`. Owner connects with nRF Connect, subscribes to 0x2A37 → `hil.py --no-upload --expect 'ble: connected=1' 'ble: subscribed=1'` → PASS; on screen counter increments.
- [ ] **Step 3: Commit** `feat(apps): port BLE heart-rate server to NimBLE-Arduino`.

### Task 12: More menu + power handling + soak  *(issue 12)*

**Files:** `src/apps/more_menu/**` (drop the three LVGL entries and `lv_demos` include), `hal.cpp` `powerOff()` = `M5Dial.Power.powerOff()`.
- [ ] **Step 1:** Port, compile UserDemo.
- [ ] **Step 2:** HIL soak (Review Focus 5): add `M5DD_LOG("heap", "free=%u min=%u", ...)` on every `app: close`. Owner opens Wi-Fi scan → back → BLE → back → Wi-Fi → back. `hil.py --no-upload --timeout 180 --expect 'app: close WiFi_Scan' 'app: close BLE_Server' 'app: close WiFi_Scan'` → PASS, no reboot (`boot:` must not reappear — add `--forbid 'boot:'` option to `hil.py`). Record min heap in PORTING_NOTES.md.
- [ ] **Step 3:** Owner: Power Off powers down; button press wakes.
- [ ] **Step 4: Partition check:** UserDemo (now containing Wi-Fi + BLE) `build.sh` reports sketch size vs maximum. If usage > 90 %, add `sketch/UserDemo/partitions.csv` (factory app 4 MB as original, no FAT `storage`) and note it in PORTING_NOTES; otherwise record the percentage and keep the default scheme.
- [ ] **Step 5: Commit** `feat(apps): port More menu and power-off`.

### Task 13: Factory test with jig, verdicts, summary  *(issue 13)*

**Files:** `src/apps/factory_test/**` (from `main/apps/app_factory_test`), `src/apps/factory_test/report.h` (Arduino-free), `test/host/test_report.cpp`, `sketch/docs/FACTORY_TEST_JIG.md`; deps add Adafruit NeoPixel.
**Interfaces:**
- `enum class Step { Lcd, Rtc, Rfid, Io }; enum class Verdict { Pending, Pass, Fail, Skip };`
- `class FactoryReport { void set(Step, Verdict); Verdict get(Step) const; bool allPassedOrSkipped() const; std::string line(Step) const; /* e.g. "IO: SKIP" */ std::string serial() const; /* "factory: LCD=PASS RTC=PASS RFID=FAIL IO=SKIP" */ };`
- `Verdict promptVerdict(HAL::HAL&, const char* stepName)` — round screen with three options PASS/FAIL/SKIP, knob selects, press confirms, default highlight PASS.
- IO step: NeoPixel 14 px on GPIO2; `analogReadMilliVolts(1)`; LED count = original mapping (`3000 - mv`, clamp 2900→3000, `/500`); Port A scan via `M5Dial.Ex_I2C` every 800 ms, list addresses. Each step logs `factory: step <name>`.
- Flow: LCD → RTC → RFID → IO (each followed by `promptVerdict`) → summary screen + `M5DD_LOG(report.serial())` → press → RTC wake-up test (`setAlarmIRQ(4)`, power off, RTC wakes device).
- [ ] **Step 1: Failing host test** `test_report.cpp`: fresh report all `Pending`; set Io=Skip → `line(Io)=="IO: SKIP"`; `serial()` exact string above; `allPassedOrSkipped()` false with any Fail/Pending. FAIL → implement → PASS.
- [ ] **Step 2:** Port steps + `promptVerdict` + summary, wire into `UserDemo.ino` boot check. Compile.
- [ ] **Step 3:** HIL: owner holds button while `hil.py UserDemo` resets → expect `factory: step LCD` … `factory: LCD=` → owner chooses verdicts, skips IO on bare device → expect `IO=SKIP`; after wake-up `boot: UserDemo` reappears (RTC wake works).
- [ ] **Step 4:** FACTORY_TEST_JIG.md: wiring (Port B GPIO2 → WS2812 DIN, 14 LEDs, 5V/GND; GPIO1 ← pot wiper 0–3.3 V; Port A any I2C device), and the owner runs once with the jig when built (checklist item, may stay open).
- [ ] **Step 5: Commit** `feat(apps): port factory test with operator verdicts and summary`.

### Task 14: LvglDemo  *(issue 14)*

**Files:** `examples/LvglDemo/LvglDemo.ino`, `examples/LvglDemo/lv_conf.h`; deps add `lvgl` 9.x.
**Interfaces:** Independent of M5DialDemo except `log.h`. Build flags only for this sketch: compile via `build.sh` with `--build-property "compiler.cpp.extra_flags=-DLV_CONF_INCLUDE_SIMPLE -I<sketch dir>"` (wired in `build.sh` when `lv_conf.h` exists in the sketch dir). Display: `lv_display_create(240,240)`, partial buffer 240×24×2 B, flush → `M5Dial.Display.pushImage`. Input: encoder indev (`LV_INDEV_TYPE_ENCODER`, diff from `M5Dial.Encoder`/4, press from BtnA) + pointer indev from Touch. UI: `lv_arc` 0–100 bound to knob via `lv_group`, value label centre, one button that toggles arc colour. Log `lvgl: value=<n>`.
- [ ] **Step 1:** Implement, compile; confirm other sketches still compile (no `lv_conf.h` leakage).
- [ ] **Step 2:** HIL `--expect 'lvgl: ready'`; owner turns knob → `lvgl: value=` lines; arc renders round and smooth.
- [ ] **Step 3: Commit** `feat(examples): LVGL 9 starter sketch`.

### Task 15: CI compile  *(issue 15)*

**Files:** `.github/workflows/sketch-build.yml`.
- [ ] **Step 1:** Workflow on push/PR touching `sketch/**`: setup arduino-cli, cache `~/.arduino15`, run `sketch/tools/setup.sh --ci` (no venv pillow needed), `test_host.sh`, python unit tests, `build.sh` for every dir containing a same-named `.ino`. Secrets: CI copies `secrets.example.h` → `secrets.h`.
- [ ] **Step 2:** Push branch → workflow green. Break a sketch locally on a scratch commit → red (then drop the commit).
- [ ] **Step 3: Commit** `ci: compile all sketches and run host tests`.

### Task 16: Documentation finalisation  *(issue 16)*

**Files:** `sketch/docs/{SETUP.md,PORTING_NOTES.md,TEST_CHECKLIST.md}`, `sketch/README.md`, root `README.md` (one link line to `sketch/README.md` only).
- [ ] **Step 1:** Fresh-machine check: new macOS user account (or `ARDUINO_DIRECTORIES_DATA` temp dir) following SETUP.md verbatim → `Build+Upload+Monitor` of UserDemo works. Fix every deviation in the doc.
- [ ] **Step 2:** PORTING_NOTES: spec §5.2 table as-built, encoder/button semantics, heap numbers, known differences from original.
- [ ] **Step 3:** TEST_CHECKLIST complete for every sketch; owner runs full list once.
- [ ] **Step 4: Commit** `docs(sketch): finalise setup, porting notes, test checklist`.

---

## GitHub tracking (after plan approval, owner confirms list first)

- Epic: "Port UserDemo to Arduino sketches (M5Dial library)" linking spec + this plan, task list of the 16 issues.
- Issues 1–16 = Tasks 1–16 (5–8 split, one each). Labels: `arduino-port`, plus `infra` (1,2,3,15), `app` (4–14), `docs` (16). Each issue body = task text + its owner checklist.
- Repo: `beyonddemise/M5Dial-UserDemo`. Dependencies: 2←1, 3←1, 4←2,3, 5–12←4, 13←4,9, 14←1, 15←1, 16←all.

## Review (to be filled in after execution)
