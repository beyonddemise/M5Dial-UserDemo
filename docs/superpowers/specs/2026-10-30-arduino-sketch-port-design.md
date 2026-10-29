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
        hal/                    # thin HAL facade over M5Dial (keeps hal->display/encoder/... call shape)
        mooncake/               # APP_BASE, GUI_Base
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
  .vscode/                      # tasks.json, settings.json, extensions.json, c_cpp_properties.json
```

Each standalone sketch is ~20 lines: `M5Dial.begin(...)`, construct the one app from the
library, run its lifecycle (`onSetup/onCreate/onRunning`) in `loop()`. The launcher sketch
is the port of `main/main.cpp` (hold-button-at-boot → factory test, else launcher).

### 5.2 Component mapping

| Original | Replacement |
|---|---|
| `LGFX_StampRing` + vendored LovyanGFX | `M5Dial.Display` (M5GFX autodetects the Dial); `LGFX_Sprite` → `M5Canvas` |
| ESP32Encoder + `Button` on GPIO42 | `M5Dial.Encoder` + `M5Dial.BtnA` (wrapped to keep `encoder.getCount()` / `btn.read()` shape) |
| FT3267 driver (`hal_tp.hpp`) | `M5Dial.Touch` (`getDetail()`); facade keeps `isTouched()/update()/getTouchPointBuffer()` |
| PCF8563 driver (`hal_rtc.hpp`) | `M5Dial.Rtc` |
| `hal/arduino/Tone.cpp`, LEDC copies | `M5Dial.Speaker.tone()` — whole `hal/arduino/` dropped |
| GPIO46 power hold / power off | `M5Dial.Power` (`powerOff()`, `timerSleep()` for RTC wake-up) |
| `esp-idf-rc522` component (event callbacks) | `M5Dial.Rfid` (MFRC522: `PICC_IsNewCardPresent/ReadCardSerial`), polled in `onRunning` |
| `espressif/led_strip` (14 LEDs, GPIO2) | Adafruit_NeoPixel |
| ADC oneshot + calibration (ADC1_CH0) | `analogReadMilliVolts(1)` |
| NimBLE C heart-rate GATT server (`ble_demo.c`, `gatt_svr.c`) | NimBLE-Arduino, same services/UUIDs (HRS 0x180D, Device Info 0x180A) and same `ble_demo_get_infos()`-style status for the GUI |
| esp_wifi scan (`wifi_common_test.cpp`) | `WiFi.scanNetworks()` (async); keep the "AP count before records" fix intent from d49e20e |
| efontCN fonts via CMake defines | M5GFX built-in `fonts::efontCN_24` / `fonts::efontCN_16_b` in `config.h` |
| `wear_levelling` FAT mount | dropped (its init is commented out in the original) |
| `partitions.csv`, sdkconfig | board options for 8 MB flash; custom partition only if the launcher binary exceeds the default app slot |
| `flash.sh`, `hotplugSetup.sh` | `tools/*.sh` + VSCode tasks; WSL/usbipd notes move into SETUP.md |

### 5.3 App framework

Mooncake `APP_BASE`, `GUI_Base`, launcher, `smooth_menu` are plain C++ and are ported with
minimal edits: include paths, `delay` macro removal (Arduino provides `delay`), HAL type changes.
`std::string`/`std::vector` stay. The `VideoShit` class keeps its behaviour; renaming
it to `TempDemo` in code is acceptable (display name unchanged).

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

1. Install VSCode + extensions: C/C++ (or clangd), Serial Monitor; `extensions.json` recommends them.
2. Install arduino-cli (Homebrew / installer); `arduino-cli config init` or use repo `arduino-cli.yaml`.
3. Add M5Stack board-manager URL; install the M5Stack ESP32 board package (pinned version).
4. Install libraries (pinned): M5Dial, M5Unified, M5GFX, NimBLE-Arduino, Adafruit NeoPixel, lvgl (LvglDemo only).
5. USB: driver notes (ESP32-S3 native USB/CDC), finding the port on macOS/Linux/Windows, WSL usbipd.
6. VSCode tasks: Build / Upload / Monitor / Build+Upload+Monitor, with a sketch picker input.
7. IntelliSense: generate `compile_commands.json`, point C/C++ or clangd at it.
8. Secrets: copy `secrets.example.h` → `secrets.h`.
9. Troubleshooting: boot/download mode (hold button while plugging in), port busy, upload speed,
   "library not found" (sketchbook path), out-of-memory at link time.

## 6. Verification

- Every sketch compiles via `tools/build.sh` with zero warnings introduced by our code.
- A GitHub Actions workflow compiles all sketches on push/PR (arduino-cli, cached cores).
- **Hardware in the loop:** an M5Dial is attached to the dev machine (ESP32-S3 native
  USB-JTAG/serial, VID:PID `303A:1001`, currently `/dev/cu.usbmodem21101` — the port name can change
  between plug-ins, so scripts take it as a parameter / auto-detect). The builder **flashes every
  sketch and reads the serial log** as part of each issue's definition of done.
- **Serial self-report:** every sketch logs boot, peripheral init results and key events
  (encoder delta, button, touch x/y, RTC time, RFID UID, Wi-Fi AP count, BLE connect/subscribe)
  at 115200 baud, so the builder can verify behaviour without seeing the screen.
- **Owner-only checks** (things serial can't prove): what's on screen, knob/touch feel,
  buzzer sound, presenting an RFID card, the IO jig. Listed per sketch in `TEST_CHECKLIST.md`;
  the builder prompts the owner for these at the end of each issue.
- **First flash overwrites the factory firmware.** Before it, the builder backs up the full
  8 MB flash with `esptool read_flash` (to a git-ignored `sketch/backup/`), so the device can be restored.
- Behaviour reference = the original ESP-IDF firmware / the flash backup (side-by-side comparison).

## 7. Proposed issue breakdown (one epic, ordered)

**Workflow (owner decision):** after the plan is approved, Claude creates one epic plus the issues
below on the fork **`beyonddemise/M5Dial-UserDemo`** — only after the owner confirms the final
issue list. One branch + PR per issue against `main`; later issues build on merged earlier ones
(an issue may be merged once it compiles and closed after the owner's on-device check).

1. Toolchain & skeleton: `/sketch` layout, `arduino-cli.yaml`, tools scripts, VSCode tasks, SETUP.md draft, empty-sketch build.
2. Shared library core: HAL facade over M5Dial, Mooncake base, GUI_Base, config/secrets.
3. Assets + `png2rgb565.py` (byte-identical regeneration check vs existing headers).
4. smooth_menu + Launcher shell (icons, encoder/touch navigation, open/close apps).
5. LCD test · 6. RTC test · 7. Brightness · 8. Temp demo — each: library app + standalone sketch + launcher wiring.
9. RFID test (MFRC522 API adaptation).
10. Wi-Fi scan.
11. BLE server (NimBLE-Arduino).
12. More menu (Power Off / Quit) + power handling.
13. Factory test incl. IO jig step, PASS/FAIL/SKIP verdicts, summary screen + FACTORY_TEST_JIG.md.
14. LvglDemo standalone sketch.
15. CI compile workflow.
16. SETUP.md finalisation + TEST_CHECKLIST.md + PORTING_NOTES.md.

## 8. Builder's calls (made in this design, not by the owner)

- NimBLE-Arduino over the core's Bluedroid BLE (closer to original, smaller RAM).
- Adafruit_NeoPixel for the LED strip.
- `/sketch` doubles as Arduino sketchbook; standalone sketches live under `sketch/examples/`.
- Original ESP-IDF tree left untouched.
- Exact pinned versions of board package and libraries chosen at implementation time (latest stable, recorded in SETUP.md).
- Factory test Wi-Fi/BLE steps stay disabled as in the original.

## 9. Left to the builder

- Exact HAL facade API shape (as long as app code changes stay minimal).
- Whether a custom partition table is needed (decide from binary size).
- Visual design of the PASS/FAIL/SKIP prompt and summary screen (must fit the round 240×240 display).
