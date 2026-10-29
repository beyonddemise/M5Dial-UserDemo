# M5Dial UserDemo → Arduino Sketch Port — Design

Date: 2026-10-30 · Status: approved 2026-10-30 · Tracking: GitHub issues (to be created after approval)

## 1. Intent

The repo holds M5Stack's M5Dial "UserDemo" firmware, built with ESP-IDF v5.1.3 and
vendored LovyanGFX / LVGL / rc522 components. The owner wants **the samples working
again in Arduino `.ino` form**, built from VSCode, using the official
[M5Dial library](https://github.com/m5stack/M5Dial) instead of hand-rolled drivers.
The primary goal is *working samples*; whether the owner later builds their own UIs
(possibly with LVGL) is undecided.

## 2. Goals / Non-goals / Anti-goals

**Goals**
- A new `/sketch` tree containing all converted sources, assets and tools.
- Every original app works on a real M5Dial from Arduino builds:
  launcher, LCD test, RTC test, RFID test, brightness, Wi-Fi scan, BLE server,
  temp demo ("Video Shit"), More menu (Power Off / Quit), factory-test mode.
- Hardware access exclusively via the M5Dial library (M5Unified / M5GFX underneath).
- Comprehensive VSCode setup instructions (macOS primary, Linux/Windows notes).
- Work decomposes cleanly into GitHub issues.

**Non-goals**
- Redesigning the UI, the launcher look, or the app framework.
- Touching/removing the original ESP-IDF project (stays as reference, side by side).
- LVGL inside the launcher (see §5.4).
- PlatformIO or Arduino IDE 2 as primary workflow (Arduino IDE should *work*, not be documented in depth).

**Anti-goals (failure even if it compiles)**
- Vendoring copies of M5GFX/LovyanGFX, ESP32Encoder, FT3267/PCF8563/rc522 drivers into `/sketch`.
- Committing Wi-Fi credentials.
- Setup docs that only work on the author's machine (unpinned versions, absolute paths).
- Per-app sketches that duplicate the shared code by copy-paste.

## 3. Decisions made by the owner

| # | Decision |
|---|---|
| D1 | Structure: **shared local library + one standalone sketch per app + full launcher sketch**. |
| D2 | Toolchain: **arduino-cli driven by VSCode tasks**; IntelliSense via generated `compile_commands.json`. |
| D3 | LVGL: **not in launcher**; one optional standalone `LvglDemo` sketch on current LVGL 9 as a starter template. |
| D4 | Factory test: **ported**, behind `ENABLE_FACTORY_TEST`; **IO jig step included** (LED strip + ADC + Port A I2C scan) and **skippable** so a bare device can complete the run. The owner will build the jig. |
| D5 | Factory test results: **operator PASS/FAIL/SKIP per step + summary screen and serial log** before power-off; not persisted. |
| D6 | Tracking: **Claude creates epic + issues on `beyonddemise/M5Dial-UserDemo`** (after confirmation); **one PR per issue** against `main`. |
| D7 | **No compatibility layer.** Ported code calls the M5Dial library directly and uses its naming and idioms (`M5Dial.BtnA.wasPressed()`, `M5Dial.Encoder.read()`, `M5Dial.Touch.getDetail()`, `M5Dial.Rtc`, `M5Canvas`, `M5_LOGI`). Readers should not need to know the original firmware. Port behaviour, not names. |
| D8 | **Simulated hardware tests from phase 1:** a **virtual M5Dial in Wokwi** (custom chips for the parts Wokwi lacks), so unmodified sketches run under `wokwi-cli` scenario tests locally and in CI. QEMU rejected (no GPIO/I2C/SPI emulation on ESP32-S3). |
| D9 | **All human-only checks in one acceptance task near the end**; every earlier task is verified automatically (host tests, Wokwi scenarios, flash + serial on the attached device). |
| D10 | Execution: **subagent-driven** (fresh implementer + reviewer per task). |
| D11 | **Wokwi custom chips are kept and packaged for donation back to Wokwi** (self-contained, MIT, template layout, final contribution-package issue). |

## 4. Constraints

- **Hardware:** M5Dial = StampS3 (ESP32-S3, 8 MB flash, **no PSRAM**), GC9A01 240×240 round LCD,
  FT3267 touch, PCF8563 RTC, WS1850S/MFRC522 RFID on internal I2C, buzzer GPIO3,
  encoder GPIO40/41, button/wake GPIO42, power-hold GPIO46, Port A I2C (SDA13/SCL15),
  Port B (GPIO1 = ADC1_CH0, GPIO2 = LED data).
- **M5Dial library 1.0.3 API** (verified): `M5Dial.begin(cfg, enableEncoder, enableRFID)`,
  `M5Dial.update()`, members `Display`/`Lcd` (M5GFX&), `Touch`, `Power`, `Rtc` (RTC8563),
  `Speaker`, `BtnA`, `In_I2C`, `Ex_I2C`, `Encoder` (ENCODER on 41/40), `Rfid` (MFRC522 I2C).
  Depends on M5Unified + M5GFX.
- **Arduino sketch rules:** sketch folder name == `.ino` name; only the sketch root and
  `src/` (recursively) are compiled; shared code across sketches must be a library.
- **RAM:** 240×240×16 bpp canvas = 115 KB of internal RAM; Wi-Fi + BLE + canvas must coexist
  (the original already does this, so it fits — but nothing large may be added to the launcher).
- **Arduino-ESP32 core 3.x** (IDF 5.x underneath), so raw ESP-IDF APIs (FreeRTOS, esp_log, esp_wifi)
  remain callable where an Arduino equivalent would be a rewrite rather than a port.

## 5. Design

### 5.1 Folder layout

`/sketch` is an **Arduino sketchbook**: setting it as `directories.user` (or passing
`--libraries sketch/libraries`) makes the shared library resolve automatically for every sketch.

```
sketch/
  README.md                     # what's here, quick start, link to docs/SETUP.md
  docs/
    SETUP.md                    # comprehensive VSCode + arduino-cli setup
    PORTING_NOTES.md            # ESP-IDF → Arduino mapping, per-app deltas
    TEST_CHECKLIST.md           # on-device acceptance checks per sketch
    FACTORY_TEST_JIG.md         # jig wiring (LED strip, pot, Port A device)
  arduino-cli.yaml              # board-manager URL, sketchbook = this folder
  libraries/
    M5DialDemo/                 # shared library (library.properties, depends=M5Dial,...)
      library.properties
      src/
        M5DialDemo.h            # umbrella include
        app_context.h           # what apps receive via setUserData(): the shared M5Canvas
        encoder_detent.h        # raw M5Dial.Encoder counts → detent steps (only helper; no wrappers)
        mooncake/               # APP_BASE, GUI_Base (app framework, not hardware)
        smooth_menu/            # ported as-is (MIT, keep LICENSE)
        apps/<app>/             # each app's logic + gui, reused by launcher and standalone sketches
        assets/                 # icon/image RGB565 headers + source PNGs
        config.h                # feature flags (ENABLE_FACTORY_TEST, fonts), includes secrets.h if present
  UserDemo/
    UserDemo.ino                # launcher + factory-test entry (mirrors main.cpp)
    secrets.example.h           # FACTORY_TEST_WIFI_SSID / PASSWORD template
  examples/
    LcdTest/LcdTest.ino
    RtcTest/RtcTest.ino
    RfidTest/RfidTest.ino
    Brightness/Brightness.ino
    WifiScan/WifiScan.ino
    BleServer/BleServer.ino
    TempDemo/TempDemo.ino
    LvglDemo/LvglDemo.ino       # standalone, LVGL 9, own lv_conf handling
  tools/
    png2rgb565.py               # regenerate assets/*.h from PNGs
    build.sh / upload.sh / monitor.sh   # thin arduino-cli wrappers (replace flash.sh)
    gen-compile-commands.sh     # IntelliSense database for VSCode
  sim/                          # virtual M5Dial for Wokwi (§5.7)
    chips/<chip>/<chip>.chip.json + .chip.c   # custom chips, compiled to .chip.wasm
    diagram.json                # ESP32-S3 + custom chips + stock parts on real M5Dial pins
    scenarios/<sketch>/*.test.yaml            # wokwi-cli scenario tests per sketch
  .vscode/                      # tasks.json, settings.json, extensions.json, c_cpp_properties.json
```

Each standalone sketch is ~20 lines: `M5Dial.begin(...)`, construct the one app from the
library, run its lifecycle (`onSetup/onCreate/onRunning`) in `loop()`. The launcher sketch
is the port of `main/main.cpp` (hold-button-at-boot → factory test, else launcher).

### 5.2 Component mapping (called directly — no wrappers, D7)

| Original | Replacement (M5Dial naming) |
|---|---|
| `LGFX_StampRing` + vendored LovyanGFX | `M5Dial.Display` (M5GFX autodetects the Dial); `LGFX_Sprite` → `M5Canvas` |
| ESP32Encoder `wasMoved()/getDirection()` | `M5Dial.Encoder.read()` (x4 counts) → `encoder_detent.h` turns counts into detent steps; positive step = M5Dial's natural sign, defined as **clockwise = next item** |
| `Button` on GPIO42, `!btn.read()` loops | `M5Dial.BtnA.isPressed()/wasPressed()/wasReleased()/pressedFor(ms)`; every loop (incl. blocking waits) calls `M5Dial.update()` |
| FT3267 driver (`hal_tp.hpp`) | `M5Dial.Touch.getDetail()` (`m5::touch_detail_t`: `x`, `y`, `isPressed()`, `wasClicked()`) |
| PCF8563 driver (`hal_rtc.hpp`) | `M5Dial.Rtc` (`getDateTime()`, `setAlarmIRQ(sec)`, `clearIRQ()`, `disableIRQ()`) |
| `hal/arduino/Tone.cpp`, LEDC copies | `M5Dial.Speaker.tone(freq, ms)` — whole `hal/arduino/` dropped |
| GPIO46 power hold / power off | `M5Dial.Power` (`powerOff()`, `timerSleep()` for RTC wake-up) |
| `HAL::HAL` object passed as user data | gone; apps call `M5Dial.*` and get the shared `M5Canvas` via `AppContext` |
| `printf` / `_log` | `M5_LOGI/W/E` (M5Unified `M5.Log`) |
| `esp-idf-rc522` component (event callbacks) | `M5Dial.Rfid` (MFRC522: `PICC_IsNewCardPresent/ReadCardSerial`), polled in `onRunning` |
| `espressif/led_strip` (14 LEDs, GPIO2) | Adafruit_NeoPixel |
| ADC oneshot + calibration (ADC1_CH0) | `analogReadMilliVolts(1)` |
| NimBLE C heart-rate GATT server (`ble_demo.c`, `gatt_svr.c`) | NimBLE-Arduino, same services/UUIDs (HRS 0x180D, Device Info 0x180A), NimBLE-Arduino naming (`NimBLEServer`, callbacks) |
| esp_wifi scan (`wifi_common_test.cpp`) | `WiFi.scanNetworks()` (async); keep the "AP count before records" fix intent from d49e20e |
| efontCN fonts via CMake defines | M5GFX built-in `fonts::efontCN_24` / `fonts::efontCN_16_b` in `config.h` |
| `wear_levelling` FAT mount | dropped (its init is commented out in the original) |
| `partitions.csv`, sdkconfig | board options for 8 MB flash; custom partition only if the launcher binary exceeds the default app slot |
| `flash.sh`, `hotplugSetup.sh` | `tools/*.sh` + VSCode tasks; WSL/usbipd notes move into SETUP.md |

### 5.3 App framework

Mooncake `APP_BASE`, `GUI_Base`, launcher, `smooth_menu` are the app/UI framework (not hardware)
and are kept. Hardware calls inside them are rewritten to the M5Dial API (D7); app behaviour is
preserved. `std::string`/`std::vector` stay. The `VideoShit` class is renamed `TempDemo` in code
(display name may stay). Apps get the shared `M5Canvas*` through `AppContext` passed with
`setUserData()`.

### 5.4 LVGL demo (standalone)

`examples/LvglDemo` uses the Library-Manager LVGL 9.x, an M5GFX flush callback with a partial
(1/10-screen) buffer, and encoder + touch input devices (`lv_group` focus navigation).
Content: a small working template (e.g. arc/value bound to the knob, a button), not a copy of
the stock widgets/stress/benchmark demos. Its `lv_conf.h` handling must not affect other sketches.

### 5.5 Factory test

Same sequence as original `onCreate`: LCD → RTC → RFID → IO → RTC wake-up. Wi-Fi and BLE steps
stay present but disabled, as in the original. `ENABLE_FACTORY_TEST` is on by default (mirrors
the original global CMake define).

**Operator verdicts (owner decision, new vs. original).** The original has no pass/fail: each
step shows live output until the button is pressed. In the port, each step (LCD, RTC, RFID, IO)
ends with an **operator verdict prompt — PASS / FAIL / SKIP**, chosen with the knob and confirmed
with a press. No automatic thresholds. SKIP is how a bare device (no jig) gets through the IO step.

**Summary.** After the IO step and **before** the RTC wake-up/power-off step, a summary screen
lists each step's verdict and the same summary is printed to serial. A press continues to the
RTC wake-up test. Results are **not persisted** (no NVS); they are lost on power-off by design.

### 5.6 VSCode setup (content of `sketch/docs/SETUP.md`)

1. Install VSCode + extensions: C/C++ (or clangd), Serial Monitor, Wokwi Simulator; `extensions.json` recommends them.
2. Install arduino-cli (Homebrew / installer); `arduino-cli config init` or use repo `arduino-cli.yaml`.
3. Add M5Stack board-manager URL; install the M5Stack ESP32 board package (pinned version).
4. Install libraries (pinned): M5Dial, M5Unified, M5GFX, NimBLE-Arduino, Adafruit NeoPixel, lvgl (LvglDemo only).
5. USB: driver notes (ESP32-S3 native USB/CDC), finding the port on macOS/Linux/Windows, WSL usbipd.
6. VSCode tasks: Build / Upload / Monitor / Build+Upload+Monitor / Simulate / Sim test, with a sketch picker input.
7. IntelliSense: generate `compile_commands.json`, point C/C++ or clangd at it.
8. Secrets: copy `secrets.example.h` → `secrets.h`.
9. Simulation: Wokwi account, `WOKWI_CLI_TOKEN`, wasi-sdk for building custom chips, running a sketch in the VSCode Wokwi panel.
10. Troubleshooting: boot/download mode (hold button while plugging in), port busy, upload speed,
   "library not found" (sketchbook path), out-of-memory at link time, simulator autodetect failures.

### 5.7 Virtual M5Dial (Wokwi)

A Wokwi project in `sketch/sim/` that M5Unified/M5GFX accept as an M5Dial, so **the same compiled
firmware** that goes to the device runs in simulation.

- **Stock parts:** `board-esp32-s3-devkitc-1`, KY-040 encoder on GPIO40/41, pushbutton on GPIO42,
  buzzer on GPIO3, 14-pixel NeoPixel strip on GPIO2, potentiometer on GPIO1, Wokwi Wi-Fi (`Wokwi-GUEST`).
- **Custom chips** (Wokwi Chips API, C → WASM): `gc9a01` (SPI display with framebuffer, answers the
  ID reads M5GFX uses for autodetect), `ft3267` (I2C touch, touch point set via chip controls),
  `pcf8563` (I2C RTC incl. alarm/IRQ registers), `ws1850s` (I2C MFRC522-compatible register
  subset sufficient for REQA/anticollision/select; card UID + "card present" via controls).
  Pins and I2C addresses copied from M5GFX/M5Unified board definitions for M5Dial, not from docs.
- **Power hold (GPIO46)** is a no-op in sim; power-off/RTC wake-up ends the simulation (asserted).
- **Fallback:** if autodetect cannot be satisfied, use M5Unified `cfg.fallback_board = board_M5Dial`
  in a sim-only build flag. If neither works, stop and re-plan with the owner.
- **Not simulated:** BLE (verified on the device), real touch feel, audio.
- **Kept and donation-ready (owner decision D11):** the chips are first-class source, not test
  scaffolding. Each is self-contained, generic to the real part, MIT-licensed, documented, and
  follows the Wokwi chip template so it can be split into its own repo and contributed to Wokwi
  (together with an M5Dial board/assembly definition if Wokwi's format allows). Publishing or
  submitting anything externally happens only on the owner's go.
- **Scenarios:** `wokwi-cli` YAML per sketch: wait for serial lines, turn encoder / press button /
  set touch / present card via controls, assert serial output; screenshots compared against
  committed golden images where the scenario format supports it.

## 6. Verification

Three automated layers inside each task, one human layer at the end (D9).

1. **Host unit tests** for Arduino-free logic (encoder detents, UID formatting, factory report,
   png2rgb565, port detection).
2. **Wokwi scenarios** on the virtual M5Dial: interaction (encoder, button, touch, RFID card,
   Wi-Fi scan) and screenshots. Run locally and in CI (`WOKWI_CLI_TOKEN` repo secret).
3. **Device smoke tests**, no human needed: the attached M5Dial (ESP32-S3 USB-JTAG/serial,
   VID:PID `303A:1001`, port auto-detected) is flashed and its serial log checked for boot,
   peripheral init and idle behaviour; BLE is verified from the Mac (Python `bleak`:
   scan → connect → subscribe to 0x2A37 → receive notifications).
4. **Human acceptance task (last-but-one issue):** everything only a person can judge —
   screen vs original, knob/touch feel, clockwise = next, buzzer, real RFID card, factory test
   with the jig, power off / wake. Collected in `TEST_CHECKLIST.md`; nothing earlier blocks on it.

- Every sketch compiles via `tools/build.sh` with zero warnings introduced by our code.
- **First flash overwrites the factory firmware.** Before it, the builder backs up the full
  8 MB flash with `esptool read_flash` (to a git-ignored `sketch/backup/`), so the device can be restored.
- Behaviour reference = the original ESP-IDF firmware / the flash backup.

**Owner prerequisites before execution:** Wokwi account + `WOKWI_CLI_TOKEN` (local env and GitHub
repo secret); macOS Bluetooth permission for the terminal (for the BLE test).

## 7. Proposed issue breakdown (one epic, ordered)

**Workflow (owner decision):** after the plan is approved, Claude creates one epic plus the issues
below on the fork **`beyonddemise/M5Dial-UserDemo`** — only after the owner confirms the final
issue list. One branch + PR per issue against `main`; later issues build on merged earlier ones.
An issue closes when its automated checks pass; human checks live in the acceptance issue.

Phase 1 — foundation and simulation
1. Toolchain & skeleton, device tooling, flash backup.
2. Virtual M5Dial spike: GC9A01 chip + diagram; M5Dial.begin() autodetects in sim (go/no-go).
3. Virtual M5Dial completion: FT3267, PCF8563, WS1850S chips, encoder/button controls, scenario harness.
4. Shared library core: AppContext, encoder detents, logging, Mooncake base, GUI_Base, config/secrets.
5. Assets + `png2rgb565.py`.

Phase 2 — port
6. smooth_menu + Launcher + UserDemo sketch.
7. LCD test · 8. RTC test · 9. Brightness · 10. Temp demo.
11. RFID test. 12. Wi-Fi scan. 13. BLE server.
14. More menu, power handling, memory soak, partition check.
15. Factory test (jig step, verdicts, summary).
16. LvglDemo.

Phase 3 — wrap-up
17. CI: host tests, compile, Wokwi scenarios.
18. Docs: SETUP, PORTING_NOTES, TEST_CHECKLIST, FACTORY_TEST_JIG.
19. Human acceptance on the real device.
20. Wokwi contribution package (chip.zip builds, per-chip READMEs, split/submission guide; submission on owner's go).

## 8. Builder's calls (made in this design, not by the owner)

- NimBLE-Arduino over the core's Bluedroid BLE (closer to original, smaller RAM).
- Adafruit_NeoPixel for the LED strip.
- `/sketch` doubles as Arduino sketchbook; standalone sketches live under `sketch/examples/`.
- Original ESP-IDF tree left untouched.
- Exact pinned versions of board package and libraries chosen at implementation time (latest stable, recorded in SETUP.md).
- Factory test Wi-Fi/BLE steps stay disabled as in the original.
- `encoder_detent.h` is the only shared helper over the M5Dial API (pure count→step math; the
  M5Dial `ENCODER` exposes raw x4 counts only). Logging via M5Unified's `M5_LOGx`.
- BLE verified from the Mac with `bleak` instead of a phone.

## 9. Left to the builder

- Whether a custom partition table is needed (decide from binary size).
- Visual design of the PASS/FAIL/SKIP prompt and summary screen (must fit the round 240×240 display).
- Whether the stock KY-040 is scriptable enough in scenarios or the encoder also needs a custom chip.
