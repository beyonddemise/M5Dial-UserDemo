# Porting notes

How the ESP-IDF M5Dial UserDemo (`../main`) became the Arduino sketches in this folder: what
replaced what, why, and where the port behaves differently from the original.

Code comments point to sections here by title (for example "RFID presence probe"). Keep those
titles when you edit this file.

Contents:
1. [Component mapping (spec §5.2, as built)](#component-mapping-spec-52-as-built)
2. [Idioms: original → M5Dial](#idioms-original--m5dial)
3. [Shared code](#shared-code): app helpers, RFID presence probe, encoder
4. [Build, flash and partitions](#build-flash-and-partitions)
5. [Memory](#memory)
6. [Apps: known differences](#apps-known-differences)
7. [Virtual M5Dial (Wokwi)](#virtual-m5dial-wokwi)
8. [CI](#ci)
9. [Open items](#open-items)

The on-device checks that are still open are in [TEST_CHECKLIST.md](TEST_CHECKLIST.md).

---

## Component mapping (spec §5.2, as built)

No compatibility layer (decision D7): ported code calls the M5Dial API directly. There is no
`HAL` class, no `hal->`, no `LGFX_Sprite`, no `btn.read()`.

| Original | As built | Notes |
|---|---|---|
| `LGFX_StampRing` + vendored LovyanGFX | `M5Dial.Display` (M5GFX autodetects the Dial); `LGFX_Sprite` → `M5Canvas` | LovyanGFX is not installed. One shared 240x240 16 bpp canvas, created by `m5dd::begin(name, canvas)`. |
| ESP32Encoder `wasMoved()/getDirection()` | `dialEncoder.read()` (x4 counts, interrupt-driven, D12) → `EncoderDetent` | Not `M5Dial.Encoder`: its interrupts don't attach on GPIO40/41 (m5stack/M5Dial#8). Clockwise = +1 = next item. See [Encoder](#encoder). |
| `Button` on GPIO42, `!btn.read()` loops | `M5Dial.BtnA.isPressed()/wasPressed()/wasReleased()/pressedFor(ms)` | Every loop, blocking waits included, calls `M5Dial.update()`. Apps quit on the press edge (`m5dd::buttonClicked()`). |
| FT3267 driver (`hal_tp.hpp`) | `M5Dial.Touch.getDetail()` (`x`, `y`, `isPressed()`, `wasPressed()`) | Panel range 0-239. |
| PCF8563 driver (`hal_rtc.hpp`) | `M5Dial.Rtc` (`getDateTime()`, `setAlarmIRQ(sec)`, `clearIRQ()`, `disableIRQ()`) | |
| `hal/arduino/Tone.cpp`, LEDC copies | `M5Dial.Speaker.tone(freq, ms)` | `hal/arduino/` dropped. Tone values in `ui_tones.h`. |
| GPIO46 power hold / power off | `M5Dial.Power.powerOff()` | `powerOff()` ends in deep sleep and never returns. See [More menu / power-off](#more-menu--power-off). The factory test's RTC wake-up uses `Rtc.setAlarmIRQ(4)` + `powerOff()`. |
| `HAL::HAL` object as user data | `AppContext*` (shared `M5Canvas*`) | `app->setUserData(ctx)`. |
| `printf` / `_log` | `M5_LOGI/W/E`, body `<tag>: <message>` | `printf` to the console (UART0, which Wokwi reads; the device's USB port shows it too), 115200. Tests match on the body. |
| `esp-idf-rc522` (event callbacks) | `M5Dial.Rfid` (MFRC522), polled in `onRunning()` | Initialised once by `m5dd::begin`, only if the reader answers. See [RFID presence probe](#rfid-presence-probe). |
| `espressif/led_strip` (14 LEDs, GPIO2) | Adafruit NeoPixel | Factory test IO step. |
| ADC oneshot + calibration (ADC1_CH0) | `analogReadMilliVolts(1)` | Same pin, calibrated mV, 11 dB default. |
| NimBLE C heart-rate server (`ble_demo.c`, `gatt_svr.c`) | NimBLE-Arduino, same services and UUIDs | HRS 0x180D, Device Info 0x180A. |
| esp_wifi scan (`wifi_common_test.cpp`) | `WiFi.scanNetworks(true, true)` (async, hidden APs listed) | Count before records, as fix d49e20e intended. |
| efontCN fonts via CMake defines | `fonts::efontCN_24` / `fonts::efontCN_16_b` | `GUI_FONT_CN_BIG` / `GUI_FONT_CN_SMALL` in `config.h`. |
| `wear_levelling` FAT mount | dropped | Its init was commented out in the original. |
| `partitions.csv`, sdkconfig | `FlashSize=8M`, `huge_app` for the examples; `UserDemo/partitions.csv` (4 MB app) for UserDemo | See [Partition table (Task 14)](#partition-table-task-14). |
| `flash.sh`, `hotplugSetup.sh` | `tools/*.sh` + `.vscode/tasks.json` | See [SETUP.md](SETUP.md). |
| LVGL demos in the More menu | standalone `examples/LvglDemo` (LVGL 9.6) | D3. See [LvglDemo](#lvgldemo). |

## Idioms: original → M5Dial

The same rewrite applies in every app. The per-app tables below list only what is special.

| Original idiom | M5Dial idiom | Why |
|---|---|---|
| `_data.hal->...` / `HAL::HAL*` from `getUserData()` | `AppContext*` from `getUserData()`, then `M5Dial.*` | No HAL (D7). |
| `LGFX_Sprite* canvas`, `canvas->pushSprite(0, 0)` | `M5Canvas*` (`ctx->canvas`), same call | M5GFX keeps the LovyanGFX API. |
| `encoder.wasMoved(true)` + `getDirection() < 1` → next | `EncoderDetent::update(dialEncoder.read())`, or `m5dd::readDetentsWithClick(detent)` in apps | One step per detent, signed, clockwise positive. Several detents per frame each count. |
| `delay(100)` after an encoder move | nothing | The detent tracker gives one step per click and the ISR counter loses nothing while the app draws. |
| `!encoder.btn.read()` (level) | `M5Dial.BtnA.wasPressed()` (edge) | A button still held when an app or the launcher starts must not act again. |
| `while (!btn.read()) delay(5);` | `while (M5Dial.BtnA.isPressed()) { M5Dial.update(); delay(5); }` | Without `update()` the state never changes and the loop hangs (Review Focus 1). |
| `!btn.read()` + hold loop to quit | `m5dd::buttonClicked()` | Shared quit gesture with the 2 kHz press tone. |
| HAL encoder callback `buzz.tone(6000/7000, 20)` | `M5Dial.Speaker.tone(6000, 20)` next / `(7000, 20)` previous | Played by `readDetentsWithClick()`. |
| HAL button callback `buzz.tone(2000, 20)` | `M5Dial.Speaker.tone(2000, 20)` | |
| `tp.isTouched()` + `tp.update()` + `getTouchPointBuffer()` | `M5Dial.Touch.getDetail()`: `isPressed()`, `x`, `y` | |
| `while (tp.isTouched()) delay(5);` | `while (M5Dial.Touch.getDetail().isPressed()) { M5Dial.update(); delay(5); }` | Same reason as the button loop. |
| `rtc.getTime(tm&)` | `M5Dial.Rtc.getDateTime(&dt)` (`m5::rtc_datetime_t`) | |
| `display.getBrightness()/setBrightness()` | `M5Dial.Display.getBrightness()/setBrightness()` | |
| `hal->powerOff()` + `delay(4000); hal->powerOn()` | `M5Dial.Power.powerOff()` | Never returns; no fallback needed. |
| `_log`, `printf`, `_log_mem()` (`heap_caps_get_free_size`) | `M5_LOGI/W/E("<tag>: ...")`, same heap call | One log format for tests. |
| `delay` / `millis` macros (FreeRTOS / esp_timer) | Arduino `delay()` / `millis()` | |
| `app->setUserData(hal)` | `app->setUserData(ctx)` | |

Every app logs `app: open <Class>` in `onCreate()` and `app: close <Class>` in `onDestroy()`,
with `<Class>` = `setAppName()` = the C++ class name. Standalone sketches log `boot: <Sketch>`
and `init: rfid ok|missing` first (`m5dd::begin`), and `app: restart` when the app closes
(`m5dd::runStandalone` restarts the chip).

---

## Shared code

### Shared app helpers (`m5dd_begin.h`)

Every ported app reads input the same way, so three helpers live next to `runStandalone`. They
call the M5Dial API directly; they are not a HAL.

- **`m5dd::initGui(app, ctx, icon, color, pixels)`:** what the launcher does before it opens an
  app, for standalone sketches: canvas font `GUI_FONT_CN_BIG` (the original `HAL::init()` left
  the canvas on efontCN_24 and every app's text relies on it), the 42x42 launcher icon in a
  sprite, theme colour `launcher_layout::appColor(slot)`, then `getGui()->init()`. The colour
  table moved from the launcher's render callback to `launcher_layout.h`, so both use one table.
- **`m5dd::readDetentsWithClick(detent)`:** detent steps (clockwise positive) plus the original
  HAL's knob click (6 kHz next / 7 kHz previous, 20 ms). Upstream played the click from the
  HAL's encoder-moved callback, which fired from `wasMoved()`, so it sounded only in apps that
  polled the encoder (not in RTC_Test). The port matches that: only apps that read the knob
  call this helper. The tone values live in `ui_tones.h`, shared with the launcher (which keeps
  its own press/hold loop for the selector animation).
- **`m5dd::buttonClicked()`:** the quit gesture. True once per press: on `BtnA.wasPressed()` it
  plays the HAL's 2 kHz press tone, then waits for the release, calling `M5Dial.update()`
  (Review Focus 1). Upstream checked the level (`!btn.read()`); the edge matches the launcher
  and cannot quit an app at once if the button is still held when it opens.
  `wasPressed()` is true for one `M5Dial.update()` only, so a press that starts and ends inside
  another blocking wait of the app (e.g. the touch-release loops of LCD_Test and TempDemo) is
  consumed there and does not quit.
- **`m5dd::begin(name)`:** a canvas-less overload of `m5dd::begin(name, canvas)` for LvglDemo.
  Same bring-up (sim shim, RFID probe, log lines), no 115 KB canvas.

### RFID presence probe

The plan has `m5dd::begin` call `M5Dial.begin(cfg, false, true)` (deviation). `MFRC522::PCD_Reset()`
busy-waits on `CommandReg` (`while (PCD_ReadRegister(CommandReg) & (1<<4))`, M5Dial
`utility/MFRC522.cpp`), and a failed I2C read leaves the value uninitialised, so a missing or dead
reader would hang every sketch at boot.

`m5dd::begin` therefore:
1. calls `M5Dial.begin(cfg, false, false)`;
2. reads VersionReg `37h` at `RC522_I2C_ADDRESS` over `M5.In_I2C` (100 kHz);
3. only if the reader ACKs with a value other than 00h/FFh, initialises it like `PCD_Init()`, but
   through `M5.In_I2C` with every transfer checked and a bounded soft reset (`rfidInit()` in
   `rfid_reset.h`, 200 ms).

A reader that answers VersionReg yet never leaves power-down, or fails any setup transfer, is
reported missing instead of hanging in `PCD_Reset()` or being used with garbage reads
(`PCD_ReadRegister()` ignores the I2C status). It logs `init: rfid ok|missing`. Sketches check
`m5dd::rfidPresent()` before they use `M5Dial.Rfid`.

### Encoder

Spec §5.2 maps the encoder to `DialEncoder.read()`. It is built as the global `dialEncoder`
(`dial_encoder.{h,cpp}`): an interrupt-driven x4 counter with M5Dial's `Encoder` API, started by
`dialEncoder.begin()` in `m5dd::begin`. `M5Dial.begin()` is called with `enableEncoder=false`
because M5Dial's `ENCODER` (PJRC Encoder) has no interrupt entry for GPIO40/41 and would poll, so a
20 ms canvas push could drop edges (D12, Review Focus 2). The quadrature table is the pure
`quadrature_decoder.h`, host-tested.

#### Encoder direction (CW sign): `M5DD_ENCODER_CW_SIGN = -1`

With A on GPIO41 (`ENCODER` pin1) leading B on GPIO40, PJRC's state table counts down: from rest
(both high) A falls → state `1011b` → `position--` (Encoder.h `update()`), so one clockwise detent
gives raw `-4`. `M5DD_ENCODER_CW_SIGN` lives in `libraries/M5DialDemo/src/config.h`; `EncoderDetent`
multiplies by it, so clockwise logs e.g. `enc: steps=1 total=1 raw=-3`.

The sign is derived from library code and checked by the simulator's encoder chip (A leads B =
clockwise, like the KY-040). Which physical direction is "A leads" on the real knob is a
[TEST_CHECKLIST](TEST_CHECKLIST.md) item.

#### Encoder detents: centred window

One detent = 4 counts (x4 decoding). `EncoderDetent` index = `floor((raw + 2) / 4)`, so detent 0
covers raw -2..+1. The knob rests at raw = multiple of 4, i.e. in the middle of a window: ±1
contact chatter at rest never steps, and a step fires at the half-detent point (third edge
clockwise, second edge counter-clockwise; with 4 integer counts per detent the window cannot be
exactly symmetric). The boundary itself (raw 2 mod 4) is mid-detent, where the knob does not
rest, so moving back across it undoes the step. Floor (not truncating) division keeps detent 0 as
wide as every other (Review Focus 2).

After an app returns to the launcher, the launcher re-bases its detent, so detents the app
consumed do not move the launcher.

---

## Build, flash and partitions

### Board options: 8 MB flash, `huge_app`

`tools/deps.txt` sets `M5DIAL_FQBN_OPTIONS=CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=huge_app`.

- `CDCOnBoot=cdc`: the board default leaves `Serial`/`M5_LOG` off the native USB port.
- `FlashSize=8M,PartitionScheme=huge_app`: the M5Dial has 8 MB flash. The board default is a
  4 MB layout with a 1.25 MB app partition, and UserDemo already used 88% of it
  (`lgfx_efont_cn_24` alone is 550 KB) before the Wi-Fi and BLE apps were added. `huge_app` gives
  a 3 MB app and no OTA; the original firmware also used a 4 MB factory app without OTA.

The flash backup and restore tools always handle the whole 8 MB chip, whatever the partition
table.

### Partition table (Task 14)

With the More menu, UserDemo measured 2,833,983 B, which is 90.1% of the 3 MB `huge_app` app
partition (the limit is 90%). So `UserDemo/partitions.csv` gives UserDemo one 4 MB app, as the
original firmware (`/partitions.csv`: `factory` 4M). There is no OTA and no FAT/SPIFFS, because
the port stores no files.

| Name     | Type | SubType  | Offset   | Size     |
|----------|------|----------|----------|----------|
| nvs      | data | nvs      | 0x9000   | 0x5000   |
| otadata  | data | ota      | 0xe000   | 0x2000   |
| app0     | app  | ota_0    | 0x10000  | 0x400000 |
| coredump | data | coredump | 0x410000 | 0x10000  |

- **Why not the original table as-is:** the original table has `nvs` at 0x9000 with 0x6000 bytes
  and `phy_init` at 0xf000. Every Arduino upload writes `boot_app0.bin` to 0xe000, which would
  overwrite both. So the layout follows the core's `huge_app.csv` (otadata at 0xe000, `app0` as
  `ota_0`, coredump), with `app0` grown to 4 MB. 0x420000-0x800000 stays unused.
- **How it is picked up:** the core's prebuild hook copies `<sketch>/partitions.csv` over the
  menu's table. The examples have none and keep `huge_app`. arduino-cli still checks the size
  against the menu's `upload.maximum_size` (3 MB), so `tools/build.sh` reads the first app
  partition of a sketch's `partitions.csv` and passes it as
  `--build-property upload.maximum_size=<bytes>` (`max_size` in `build.sh`; tested in
  `tools/tests/test_build_partitions.py`).
- **Result:** UserDemo was 2,833,983 B (67% of 4,194,304 B) in Task 14, and 2,869,723 B (68%)
  with the factory test in Task 17's build table ([Flash use](#flash-use)). The flash settings
  stay 8 MB (`FlashSize=8M`, `--flash-size 8MB` in the build's `flash_args`), and
  `gen_esp32part.py` decodes the built `UserDemo.ino.partitions.bin` to the table above.
- **Re-flashing over the old layout:** a Dial flashed with `huge_app` gets the new table, so the
  old SPIFFS area at 0x310000 becomes part of app0. Nothing in the port uses SPIFFS.

### Library linkage: `dot_a_linkage=true`

`library.properties` sets `dot_a_linkage=true`, so the library is linked as an archive and a sketch
pulls in only the objects it references. Without it, every standalone sketch linked `launcher.cpp`
and ran its static constructors at boot: 8 icon `M5Canvas` objects and the icon and tag tables
(16 `std::string` tags). DialProbe measured 540,283 B flash / 31,612 B globals with it off, and
537,283 B / 27,964 B with it on.

### Explicit prototypes in `.ino` files

Every `.ino` declares `void setup(); void loop();` itself. Why: arduino-cli's prototype
generator was unreliable on Apple Silicon. Raw universal-ctags emits `typeref:` instead
of `returntype:` and ignores `#line`, so arduino-cli inserted `setup(); loop();` *inside*
`setup()` in DialProbe, which recursed at boot (Double exception). On Apple Silicon the builds
now use the `tools/ctags/ctags` wrapper ([SETUP.md](SETUP.md), step 3); the explicit prototypes
stay as a second guard.

### Optional secrets.h

`config.h` includes `secrets.h` only `#if __has_include("secrets.h")`, and otherwise defines
placeholder `FACTORY_TEST_WIFI_SSID` / `FACTORY_TEST_WIFI_PASSWORD`. `M5DD_HAVE_SECRETS` is 1 or 0
to say which case applied. A fresh clone and CI therefore build UserDemo without the file.

The lookup works from a library header because arduino-cli passes `-I<sketch dir>` first, for
the sketch and for every library source alike (checked in a `--verbose` build). So every
translation unit of one build sees the same answer. Sketches without a `secrets.h` (the
examples) and the host tests (`-Isrc` only) get the placeholders.

### Extra compiler flags (`M5DIAL_EXTRA_FLAGS`)

`build.sh` owns `compiler.c.extra_flags` and `compiler.cpp.extra_flags`. Callers add flags through
`M5DIAL_EXTRA_FLAGS` (e.g. `sim.sh` sets `-DM5DD_SIM`), and `build.sh` merges them with a sketch's
`lv_conf.h` flags. Why: a second `--build-property` for the same key replaces the first, so the
sim build of LvglDemo would have lost either `-DM5DD_SIM` or the LVGL flags. `-DM5DD_SIM` now
reaches C files too; no C file uses it. `gen_compile_commands.sh` calls
`build.sh --only-compilation-database`, so the IntelliSense database has the same flags.

---

## Memory

The M5Dial's StampS3 has no PSRAM, so everything shares the internal heap (Review Focus 5).

### Heap numbers

| Item | Size | Status |
|---|---|---|
| Shared `M5Canvas` (240x240, 16 bpp) | 115,200 B, heap, allocated at boot | by design |
| LvglDemo: LVGL heap (`LV_MEM_SIZE`) | 64 KB static `.bss` (TLSF), not the ESP heap | real use not measured (`lv_mem_monitor`) |
| LvglDemo: draw buffer (240x24 px) | 11.25 KB static `.bss` | by design |
| LvglDemo globals | 114 KB of 320 KB (DialProbe: 37 KB, plus its 115 KB canvas on the heap) | build report |
| Wi-Fi scan, BLE server | `wifi: heap=<n>` after `WIFI_OFF`, `ble: heap=<n>` after `NimBLEDevice::deinit(true)` | logged; no device value yet |
| MemSoak min heap (`heap: free=<n> min=<n>`) | **pending the MemSoak device run** | see [Memory soak](#memory-soak-examplesmemsoak) |

The launcher logs `launcher: free mem <n>` after each app and `launcher: memory leak <n>` if the
free heap shrank, as upstream (`_log_mem()`). All heap figures use
`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`, so they compare directly.

### Flash use

From `tools/build_all.sh` (Task 17 dry run, core 3.3.10):

| Sketch | App size | Partition |
|---|---|---|
| UserDemo | 2,869,723 B (68%) | 4,194,304 B (`UserDemo/partitions.csv`) |
| MemSoak | 2,223,643 B (70%; 2,222,499 B in Task 14) | 3,145,728 B (`huge_app`) |
| WifiScan | 2,048,799 B (65%) | 3 MB |
| TempDemo | 1,791,639 B (56%) | 3 MB |
| BleServer | 1,780,871 B (56%) | 3 MB |
| RfidTest | 1,567,423 B (49%) | 3 MB |
| LcdTest | 1,245,855 B (39%) | 3 MB |
| Brightness | 1,244,735 B (39%) | 3 MB |
| RtcTest | 1,244,195 B (39%) | 3 MB |
| LvglDemo | 967,691 B (30%) | 3 MB |
| DialProbe | 663,903 B (21%) | 3 MB |

NimBLE adds about 206 KB and cannot be trimmed to the peripheral role from the build. See
[BLE server](#ble-server).

---

## Apps: known differences

Each section has the app's special mappings and every behaviour change from upstream. The common
rewrites are in [Idioms](#idioms-original--m5dial).

### Launcher

Class `Launcher`, in UserDemo. Ring math in `launcher_layout.h` (host-tested).

| Original (`_data.hal->...`) | Port |
|---|---|
| `canvas` (`LGFX_Sprite*`) | `AppContext::canvas` (`M5Canvas*`, from `getUserData()`) |
| `_canvas_update()` = `canvas->pushSprite(0, 0)` | `Launcher::_canvas_update()`, same call |
| `encoder.wasMoved(true)` + `getDirection() < 1` → next | `EncoderDetent::update(dialEncoder.read())`: each step > 0 → `goNext()`, < 0 → `goLast()` |
| HAL encoder-moved callback `buzz.tone(6000/7000, 20)` | `M5Dial.Speaker.tone(6000, 20)` on next, `(7000, 20)` on previous |
| `!encoder.btn.read()` (pressed, level) | `M5Dial.BtnA.wasPressed()` (edge) |
| HAL button callback / `_encoder_button_pressed_callback` `buzz.tone(2000, 20)` | `M5Dial.Speaker.tone(2000, 20)` |
| `while (!encoder.btn.read())` hold loop | `while (M5Dial.BtnA.isPressed()) { M5Dial.update(); ... }` |
| `tp.isTouched()` + `tp.update()` + `getTouchPointBuffer()` | `M5Dial.Touch.getDetail()`: `isPressed()`, `x`, `y` |
| `while (tp.isTouched())` release loop | `while (M5Dial.Touch.getDetail().isPressed()) { M5Dial.update(); ... }` |
| `_log` / `_log_e` / `_log_mem()` | `M5_LOGI` / `M5_LOGE` with the `launcher:` tag, same heap call |

Slots (`launcher_layout::appName`): 0 LcdTest, 1 RtcTest, 2 RfidTest, 3 Brightness, 4 WifiScan,
5 BleServer, 6 TempDemo, 7 MoreMenu. Log lines: `launcher: ready`, `launcher: select <i> <tag>`,
`launcher: free mem <n>`.

- **Ten ring slots for eight icons:** the upstream menu has 10 selector stops (`n = 10`) and loop
  mode on, so turning counter-clockwise from slot 0 lands on empty slot 9 (tag `-. -`, log
  `launcher: select 9 -`), not on slot 7 as the plan's scenario text assumed. The port keeps the
  original layout. Opening an empty slot does nothing.
- **Button edge, not level:** the upstream loop opened the selected app whenever the button
  *was down*. The port opens on `wasPressed()`, so a button still held when the launcher starts
  (after the factory-test hold, or after an app that quits on press) does not open an app again.
- **Several detents per frame:** each detent moves the selector once. Upstream moved one item per
  `wasMoved()`, whatever the count change.
- **Touch on an icon (new):** a fresh tap (`wasPressed()`) on an icon (`launcher_layout::iconAt`,
  radius 22 = drawn icon) selects it and logs `launcher: select`. A touch in the centre circle
  (r 50) still opens the selected app once the selector animation has finished, as upstream.
- **App manager:** runs `M5Dial.update()` before every `onRunning()`, as `m5dd::runStandalone`
  does, and accepts `closeApp()` as well as `destroyApp()`. After the app returns, the encoder
  detent is re-based.
- **Fallback for a slot without an app:** `app: <name> not ported`, the open/close animation, then
  `launcher: ready` (used while apps were being ported; all eight slots now have an app).
- **No `app: open Launcher`:** the launcher logs `launcher: ready` instead, and never closes.
- **Icon byte order:** the icon headers are byte-swapped RGB565, as upstream. M5GFX, like the
  upstream LovyanGFX, reads `pushImage(..., const uint16_t*)` as `swap565_t` while `swapBytes` is
  false (the default; `LGFXBase::create_pc_fast`). The port calls `pushImage` the same way and
  never calls `setSwapBytes`, so the colours match the original.
- **MORE slot animation:** slot 7 keeps the black (not icon-coloured) open/close animation, as
  upstream.

#### smooth_menu

`src/smooth_menu/` is upstream `main/apps/utilities/smooth_menu/src` with its MIT `LICENSE` and
`README.md`. It has no hardware dependency. The only edits are `-Wall -Wextra` fixes: unused
parameter names commented out, and signed/unsigned comparisons (`size_t` loop index, an `int`
cast in `Selector_t::goToItem`). Each edited file's header says so. Because it is Arduino-free,
`tools/test_host.sh` compiles it into the host tests (`test_smooth_menu.cpp`).

### UserDemo factory-test entry

As upstream `main.cpp`: if `ENABLE_FACTORY_TEST` (config.h, default 1) is set and the button is
held for more than 1000 ms at boot, polled every 10 ms with `M5Dial.update()`, the sketch plays
4 kHz/30 ms twice (50 ms apart), then runs `FactoryTest` ([Factory test](#factory-test)), which
ends with a power-off, so the launcher is not reached in that boot. The original launcher had no
factory-test entry, so the launcher is unchanged.

### LCD test

Class `LCD_Test`, sketch `examples/LcdTest`, Task 7.

| Original (`_data.hal->...`) | Port |
|---|---|
| `encoder.wasMoved(true)` + `getDirection() < 1` → next, `delay(100)` | `m5dd::readDetentsWithClick(_detent)` → `goPages(steps)` (`lcd_test_pages::step`), no delay |
| `!encoder.btn.read()` + `while (!btn.read()) delay(5)` | `m5dd::buttonClicked()` |
| `while (tp.isTouched()) delay(5)` in `onCreate` | same loop on `getDetail().isPressed()` with `M5Dial.update()` |
| `_log("onCreate")`, `printf("render page: %d")` | `app: open LCD_Test`, `lcd: pattern <n>` (on open and every change) |

- `goNextPage`/`goLastPage` became `goPages(detents)`; the wrap logic is in `lcd_test_pages.h`
  (host-tested). Several detents in one frame each move one page.
- The original `delay(100)` after a page change throttled the per-count `wasMoved()`. It is gone
  (see [Idioms](#idioms-original--m5dial)).

### RTC test

Class `RTC_Test`, sketch `examples/RtcTest`, Task 8.

| Original (`_data.hal->...`) | Port |
|---|---|
| `tm rtc_time`, `rtc.getTime(tm&)` | `m5::rtc_datetime_t`, `M5Dial.Rtc.getDateTime(&dt)` (`rtc: read failed` warning if it fails) |
| `(millis() - time_count) > 1000` | `rtc_format::due(millis(), time_count, 1000)` (host-tested) |
| `_log("%d:%d:%d")` | `rtc: YYYY-MM-DD HH:MM:SS` (`rtc_format::formatDateTime`, DialProbe's form) |
| `snprintf("%02d:%02d:%02d")` in the GUI | `rtc_format::formatClock`, same format |
| `!encoder.btn.read()` + hold loop | `m5dd::buttonClicked()` |

- The refresh time is stored before the read (upstream: after drawing), so a failing RTC logs one
  warning per second, not one per frame.
- No knob click: upstream's RTC_Test never polled the encoder, so it never clicked.

### Brightness

Class `Set_Brightness`, sketch `examples/Brightness`, Task 9.

| Original (`_data.hal->...`) | Port |
|---|---|
| `display.getBrightness()` / `setBrightness()` | `M5Dial.Display.getBrightness()` / `setBrightness()` |
| `encoder.wasMoved(true)` + `getDirection() < 1` → brighter | `m5dd::readDetentsWithClick(_detent)`: steps > 0 brighter |
| `increment = 10 - delta_time / 8` (min 1), clamp 0..0xFF | `brightness_step::apply(value, steps, ms)` (host-tested) |
| `LVGL::Anim_Path` ease-out 600 ms | same (`smooth_menu/lv_anim`) |
| `!encoder.btn.read()` + hold loop | `m5dd::buttonClicked()` |
| `_log("get brightness")`, `_log("set to: %d")` | `brightness: <n>` on open and every change |

- Several detents in one frame each count (`steps * increment`); upstream moved once per
  `wasMoved()`.
- Upstream wrote the backlight only while the 600 ms animation ran, so its last write could stop
  short of the target. The port writes whenever the animated value changes; `getValue()` returns
  the end value once the animation is over, so the backlight lands exactly on the target.
- As upstream, the value is kept when the app quits (not restored).
- During a fast spin it logs one `brightness:` line per changed frame (bounded by the frame rate).

### Temp demo

Class `TempDemo` (upstream `VideoShit`), sketch `examples/TempDemo`, Task 10.

| Original (`_data.hal->...`) | Port |
|---|---|
| `VideoShit`, `VIDEO_SHIT::Data_t`, `GUI_VideoShit`, name "Video Shit" | `TempDemo`, `TEMP_DEMO::Data_t`, `GUI_TempDemo`, name "TempDemo" |
| `encoder.wasMoved(true)` + `getDirection() < 1` → `tempUp()`, `delay(100)` | `m5dd::readDetentsWithClick(_detent)` → `stepTarget(steps)` (`temp_demo_target::step`, clamped), no delay |
| `!encoder.btn.read()` + hold loop | `m5dd::buttonClicked()` |
| `tp.isTouched()` → `buttonPressed()`, `while (tp.isTouched()) delay(5)`, `buttonReleased()` | same on `M5Dial.Touch.getDetail().isPressed()`, `M5Dial.update()` in the wait |
| `printf("[%s] onCreate")`, `printf("%d", _temp)` | `app: open TempDemo`, `temp: target=<C>`, `temp: frame` per redraw |
| `gui/temp_demo_icons/*.h` (own copy) | the library's `assets/temp_demo_icons/*.h`; the image tables live in `gui_temp_demo.cpp` only |

- `temp: target=<n>` is the temperature the image shows: index 0..8 = temp_demo_1..9 =
  19..27 C, start index 3 = 22 C. It is logged on open and when the value changes; a clamped
  step logs only `temp: frame`.
- `temp: frame` is logged after every redraw. The app redraws only on input (knob, touch press
  and release), so the rate stays at a few lines per second. `onCreate()` redraws once so the
  first frame is logged after `app: open`.
- The "SET" button shows `temp_demo_button_released` while pressed and `..._pressed` otherwise,
  as upstream (the two images differ only slightly).

### RFID test

Class `RFID_Test`, sketch `examples/RfidTest`, Task 11.

| Original (`_data.hal->...`, esp-idf-rc522) | Port |
|---|---|
| `rc522_handle_t*` | no driver handle |
| `rc522_create` + `rc522_register_events` + `rc522_start` in `onCreate` | none: `m5dd::begin()` initialised the reader ([RFID presence probe](#rfid-presence-probe)); `onCreate` reads `m5dd::rfidPresent()` |
| `RC522_EVENT_TAG_SCANNED` callback (driver task) | poll in `onRunning()` every 100 ms: `M5Dial.Rfid.PICC_IsNewCardPresent() && PICC_ReadCardSerial()`, then `PICC_HaltA()` |
| `tag->serial_number` (uint64), `SN: %llu` on screen | `M5Dial.Rfid.uid.uidByte/.size` → `uid_format::formatUid` (host-tested) `04:A1:B2:C3`, shown under "UID" in efontCN_16_b |
| `buzz.tone(4000, 100)` | `M5Dial.Speaker.tone(4000, 30)` (plan value) |
| `!encoder.btn.read()` + hold loop | `m5dd::buttonClicked()` |
| `rc522_unregister_events` + `rc522_destroy` in `onDestroy` | nothing: the reader stays initialised for the next open |
| `_log("init rc522")`, `_log("Tag scanned (sn: ...)")` | `app: open RFID_Test`, `rfid: ready` or `rfid: missing` (warning), `rfid: uid=<HEX:..>`, `app: close RFID_Test` |

- **Reader missing:** `rfid: missing` is logged, the page shows "Reader Not Found", and the app
  never calls `M5Dial.Rfid` (its register access ignores I2C errors, and `PCD_Init()` could hang).
  The button still quits.
- **One line per presentation:** the card is halted after each read, so it ignores REQA until it
  leaves the field; presenting it again gives a new `rfid: uid=` line (scenario `card.test.yaml`).
- **Poll period 100 ms:** with no card, each `PICC_IsNewCardPresent()` waits for the reader's
  25 ms receive timeout over I2C; polling every frame would spend most of the loop there.
  DialProbe polls every 250 ms.
- The UID line uses `GUI_FONT_CN_SMALL`: a 7-byte UID is 20 characters, too wide for the round
  panel in efontCN_24. `uid.size` is clamped to `sizeof(uidByte)` before formatting.

### Wi-Fi scan

Class `WiFi_Scan`, sketch `examples/WifiScan`, Task 12.

| Original (`_data.hal->...`, ESP-IDF) | Port |
|---|---|
| `WIFI_COMMON_TEST::SCAN::scan()` in `onCreate`: NVS + netif + event loop + `esp_wifi_init`, blocking `esp_wifi_scan_start(NULL, ...)` (hidden APs off), then everything torn down | `WiFi.mode(WIFI_STA)` in `onCreate`, `WiFi.scanNetworks(true, true)` (hidden APs listed); `WiFi.scanComplete()` polled in `onRunning()` |
| `esp_wifi_scan_get_ap_num` then `..._get_ap_records` (fix d49e20e) | count first: `scanComplete()` returns it, then `SSID(i)` / `RSSI(i)` for `i < min(count, 10)`; the core's scan-done handler itself reads the count before the records |
| list text `i. (rssi) ssid\n`, 10 entries (`DEFAULT_SCAN_LIST_SIZE`) | same, in `wifi_list.h` (host-tested); hidden SSIDs drawn as `(hidden)`, SSIDs over 20 bytes cut on a UTF-8 boundary + `...`; control bytes in an SSID become `?` in the list and in the `wifi:` log line (`wifi_list::sanitizeSsid`) |
| knob: `25 - delta_time / 8` px per move, at least 1, overshoot anim 600 ms | same (`wifi_list::scrollIncrement/scrollOffset`), clockwise = later entries |
| `!encoder.btn.read()` + hold loop → quit | `m5dd::buttonClicked()` → quit (unchanged behaviour) |
| no rescan | **new:** a touch on the screen rescans once the finger is lifted |
| `onDestroy`: log only (Wi-Fi was already torn down per scan) | `WiFi.scanDelete()` + `WiFi.mode(WIFI_OFF)` (stops and deinits the driver), then `wifi: heap=<free internal>` |
| `_log("get wifi num: %d")`, `printf(result)` | `app: open WiFi_Scan`, `wifi: scan`, `wifi: ap_count=<n>`, `wifi: <rssi> <ssid>` per listed AP, `wifi: scan failed` (warning), `wifi: heap=<n>`, `app: close WiFi_Scan` |

- **Rescan on touch, not on the button:** upstream the button click quits and nothing rescans.
  The plan asked for "a press triggers a rescan"; giving the button press to the rescan would
  have needed a long press to quit, unlike every other app. So the button keeps quitting and a
  touch rescans (scenario `scan.test.yaml`). Touches during a scan are ignored.
- **Asynchronous scan:** the original blocked `onCreate()` for the whole scan, so the app could
  not quit until it finished. The port shows "Scanning..." and keeps the button responsive;
  knob turns during a scan are dropped.
- **Heap:** the scan records are freed with `WiFi.scanDelete()` as soon as the list text is
  built; `WIFI_OFF` on close returns the driver's memory, so repeated open/close does not leak.
  `wifi: heap=` is logged after `WIFI_OFF` for MemSoak. The list itself is at most 10 lines.
- **Scroll offset unbounded,** as upstream: you can scroll the list off screen.
- No credentials: a scan never connects, so the app does not include `secrets.h`.
- Wokwi always reports one AP, `Wokwi-GUEST`; the scenario waits for it to prove `ap_count >= 1`
  (`wait-serial` matches text, not numbers).

### BLE server

Class `BLE_Server`, sketch `examples/BleServer`, Task 13.

| Original (`ble_demo.c`, `gatt_svr.c`, raw NimBLE host) | Port |
|---|---|
| `ble_demo_start()`: `nvs_flash_init`, `nimble_port_init`, `ble_gatts_add_svcs`, host task | `BleHeartRateServer::begin(name)`: `NimBLEDevice::init` (does the NVS init), `NimBLEServer` + services, `start()` |
| HRS `0x180D`: `0x2A37` notify, `0x2A38` read = `0x01` (Chest); DIS `0x180A`: `0x2A29` "Apache Mynewt ESP32 devkitC", `0x2A24` "Mynewt HR Sensor demo" | same UUIDs and values (`heart_rate.h`) |
| name `M5-%lX` of the MAC's upper 32 bits of a little-endian uint64 | same: `M5-` + `mac[5] << 8 \| mac[4]`, unpadded hex (`heart_rate::deviceName`, host-tested) |
| advertising: flags GEN \| BREDR_UNSUP, TX power, complete name; restart on disconnect | same (`NimBLEAdvertisementData`), `advertiseOnDisconnect(true)` |
| FreeRTOS timer, 1 s: notify `{0x06, hr}`, hr 90..159 then 90; started on subscribe, stopped + reset to 90 after unsubscribe | `heart_rate::Notifier` polled from `onRunning()` via `loop()`; same payload, period and counter (host-tested) |
| page redrawn every 1000 ms; `send <hr>` / `no subscribe` | same; the number is the last *sent* value (upstream showed the next one), `subscribed` before the first |
| `!encoder.btn.read()` + hold loop → quit | `m5dd::buttonClicked()` → quit |
| `ble_demo_stop()`: `nimble_port_deinit`, `nvs_flash_deinit` | `end()`: stop advertising, disconnect peers, `NimBLEDevice::deinit(true)` (also deletes the server objects); NVS stays up (the Arduino core owns it) |
| `printf` / `MODLOG_DFLT` | `app: open BLE_Server`, `ble: advertising <name>`, `ble: connected=0\|1`, `ble: subscribed=0\|1`, `ble: heap=<free internal>` (after `end()`), `app: close BLE_Server` |

- **Threading:** NimBLE runs its callbacks on its host task; they only set atomics. Logging, the
  schedule and `notify()` happen on the app task in `loop()`, so a connect or subscribe shorter
  than one frame is not logged. NimBLE no longer finds the connection when it reports the
  unsubscribe of a disconnect, so `onDisconnect` clears the subscription itself.
- **Small additions:** the page is drawn on open, not 1 s later; `end()` logs
  `ble: connected=0` / `ble: subscribed=0` if those were still set.
- **Device-only:** Wokwi has no BLE. There is no BleServer scenario; the launcher scenario only
  checks `app: open BLE_Server` / `app: close BLE_Server`. Check on the device with
  `tools/.venv/bin/python tools/ble_check.py --name M5-<hex>` (scans, connects, reads 0x2A38,
  subscribes to 0x2A37, passes after 3 increasing values; exit codes in the script header).
  Whether `NimBLEDevice::init()` returns cleanly in Wokwi is unverified ([Open items](#open-items)).
- **Flash:** NimBLE adds ~206 KB. NimBLE cannot be trimmed to the peripheral role from the build:
  on arduino-esp32 3.3.x for the ESP32-S3 the NimBLE host comes precompiled with the core, and the
  core's `sdkconfig.h` sets all four `CONFIG_BT_NIMBLE_ROLE_*` to 1 before NimBLE-Arduino's
  `CONFIG_BT_NIMBLE_ROLE_*_DISABLED` options are checked (tried: same binary size to the byte).

### More menu / power-off

Class `MoreMenu`, launcher slot 7, Task 14.

| Original (`_data.hal->...`) | Port |
|---|---|
| 16 entries: Quit, Power Off, LVGL Widgets / Benchmark / Stress, 11 filler rows | 13 entries: the three LVGL rows and `lv_demos.h` / `_callback_lvgl` are dropped (LVGL lives only in LvglDemo, D3); list and row geometry in `more_menu_items.h` (host-tested) |
| `encoder.wasMoved(true)` + `getDirection() < 1` → `goNext()` | `m5dd::readDetentsWithClick()`, one `goNext()` / `goLast()` per detent, clockwise = next; logs `more: select <i> <tag>` |
| `!encoder.btn.read()` + hold loop, then wait for the selector anim | `M5Dial.BtnA.wasPressed()` (edge), hold loop with `M5Dial.update()`, then the same anim wait |
| `approxSin` row curve, `255 - abs(dy) * 2` text grey in a `uint8_t` | same maths (`more_menu_items::curveOffsetX / textShade`); the grey is clamped at 0 (upstream converted negative floats to `uint8_t`, undefined behaviour) |
| `fillSmoothRoundRectInDifference` (added to the original's vendored LovyanGFX) | `_invert_round_rect()` in the render callback: inside the rounded rect, black pixels become the selector orange and all others black, as upstream; exact mask (`more_menu_items::inRoundRect`), without upstream's anti-aliased corner edge |
| "Quit" → `destroyApp()` | same; logs `more: selected Quit`, then `app: close MoreMenu` |
| "Power Off": black screen, `rtc.clearIRQ()` + `disableIRQ()`, `delay(500)`, `hal->powerOff()`, `delay(4000)`, `hal->powerOn()` | logs `power: off` and drains the console first, then black screen, `M5Dial.Rtc.clearIRQ()` + `disableIRQ()`, `delay(500)`, a 4 s timer wake, `M5Dial.Power.powerOff()` |

- **Recovery on USB power:** M5Unified's `powerOff()` pulses the hold pin (GPIO46) low and then
  calls `esp_deep_sleep_start()`, so it never returns. Upstream's HAL only released the hold pin,
  so on USB power the chip kept running and re-took the hold pin after 4 s. The port arms a 4 s
  deep-sleep timer wake (`esp_sleep_enable_timer_wakeup`) before `powerOff()`. Without it nothing
  wakes the Dial on USB power: M5Unified arms no wake source for the M5Dial, and the front button
  (GPIO42) is not an RTC GPIO. On battery the supply is cut at the first hold-pin pulse, so the
  timer never fires. On USB the Dial reboots to the launcher after ~4 s (upstream returned to the
  menu without a reboot); the physical check is in TEST_CHECKLIST.
- **Button edge:** the click that opened the menu cannot also choose Quit.
- **Scenarios:** `sim/scenarios/UserDemo/more_menu_quit.test.yaml` (Quit → `app: close MoreMenu`
  → `launcher: ready`) and `power_off.test.yaml` (`power: off`; the sim ending there is accepted).
  Each is its own Wokwi session, because a power-off ends the boot. Both pass on Wokwi (PR #50,
  run 42501330101) with the 300/400/500 ms settle delays as written.

### Memory soak (`examples/MemSoak`)

Review Focus 5, Task 14. Not an upstream app.

- **Driver:** MemSoak has its own non-restarting loop instead of `m5dd::runStandalone()`, which
  restarts the chip when the app closes and so would reset the heap being measured. Per app it
  does `new (std::nothrow)`, `m5dd::initGui()`, `setUserData(&ctx)`, `onSetup()`, `onCreate()`,
  then `M5Dial.update(); onRunning()` until the done condition, then `onDestroy()` and `delete`.
- **Sequence:** `mem_soak::Sequencer` (`src/mem_soak.h`, host-tested in `test_mem_soak.cpp`).
  Each cycle runs WiFi_Scan until its scan has completed (n = 0 APs counts) or 20 s pass, then
  BLE_Server for 5 s. The phase clock starts after `onCreate()`, so NimBLE start-up does not use
  up the 5 s. Three cycles in all, each followed by `heap: free=<n> min=<n>` (internal heap: now,
  and the lowest since boot), read 200 ms after the close so the idle task has freed the stacks
  of the tasks Wi-Fi off and NimBLE deinit deleted. It logs `soak: start cycles=3` first.
- **Leak check:** the first cycle's free heap is the baseline; the first Wi-Fi and BLE init
  allocate once (NVS, PHY calibration, NimBLE pools), which is not a leak. A later cycle more
  than `LEAK_TOLERANCE` (4 KB) below the baseline logs `heap: baseline=<n>` and fails as
  `heap leak` (`mem_soak::Sequencer::cycleHeap()`, host-tested). Without it the soak only printed
  the numbers, and an 8 KB loss per cycle would still have ended in `soak: done`.
- **Verdict:** `soak: done` is logged only after three good cycles that passed the leak check.
  Any failure logs `soak: FAIL <reason>` and stops, and the sketch then idles without
  restarting. The reasons are `wifi timeout`, `wifi quit` / `ble quit` (the app closed itself,
  e.g. after a button click), `heap leak`, and `alloc wifi` / `alloc ble`. Failures are sticky,
  and a scan that completes in the same frame as the timeout or a click still counts as done.
- **Done seam:** `WiFi_Scan::scanDone()`. It returns true from the frame that logged
  `wifi: ap_count=<n>`, and is the only addition to the Wi-Fi app. A failed scan never sets it,
  so it shows up as `wifi timeout` after 20 s.
- **Device-only:** Wokwi has no BLE, so there is no scenario. The check is in TEST_CHECKLIST.
- **Min heap:** *pending the MemSoak device run*. Record the three `heap:` lines and the final
  `min=` here once it has passed.

### Factory test

Class `FactoryTest`, in UserDemo (boot hold), Task 15. Wiring, controls and the summary:
[FACTORY_TEST_JIG.md](FACTORY_TEST_JIG.md).

| Original (`main/apps/app_factory_test`) | Port (`src/apps/factory_test`) |
|---|---|
| `onCreate()`: LCD, RTC, RFID, IO, (Wi-Fi, BLE commented out), RTC wake-up | the same order, without Wi-Fi/BLE; each step is followed by `promptVerdict()`, then a summary before the wake-up |
| no verdicts: the operator judged by eye | `factory_test::FactoryReport` (`report.h`, host-tested): PASS/FAIL/SKIP per step, `factory: LCD=.. RTC=.. RFID=.. IO=..` |
| embedded `LCD_Test`/`RTC_Test`/`RFID_Test`, theme black, empty icon sprite | the ported apps embedded the same way, theme black, with their launcher icon (`m5dd::initGui`), `M5Dial.update()` each frame |
| per-frame Chinese overlay on the RTC/RFID apps ("[RTC测试] ...") | dropped: it was printed over the ported GUIs; the step name is on the verdict prompt |
| `_io_test_rgb_init()`: esp-idf `led_strip`, RMT, SK6812, GPIO2 | `Adafruit_NeoPixel(14, 2, NEO_GRB + NEO_KHZ800)` (`LIB_NEOPIXEL`) |
| `adc_read_get_value()`: adc_oneshot ch0 + calibration, 11 dB | `analogReadMilliVolts(1)` (same pin, calibrated mV, 11 dB default) |
| `3000 - mv`, `>2900` → 3000, `/500`; lit if `i <= n` or `13-i <= n` | `io_led_level::level()` / `lit()` (host-tested); level clamped at 0 (see below) |
| own `I2C_NUM_1` driver on G13/G15 + address probe, every 800 ms | `M5Dial.Ex_I2C.begin()` + `scanID()` (0x08-0x77), every 800 ms |
| IO screen: `[电池]` (the mapped value / 1000), `[PORT-B]`, `[PORT-A]` | `[PORT-B] <mV> <level>`, `[PORT-A] <addresses>` or `无` |
| `!btn.read()` + hold loops | `m5dd::buttonClicked()` / `isPressed()` waits with `M5Dial.update()` |
| wake-up: `clearIRQ`, `setAlarmIRQ(4)`, black, `delay(500)`, `powerOff()` | the same, after a click on the summary; `factory: rtc wakeup in 4 s` logged and flushed first |

- **Only four steps:** the plan's flow is LCD → RTC → RFID → IO. The original's `_wifi_test()` and
  `_ble_test()` were already commented out in `onCreate()`; `wifi_test.cpp`/`ble_test.cpp` are not
  ported (the launcher's Wi-Fi scan and BLE server apps cover that hardware).
- **Verdict prompt:** `promptVerdict(canvas, step, default = Pass)`: PASS/FAIL/SKIP top to bottom,
  the knob moves the highlight (clockwise = next, clamped, not wrapping: an overshoot past SKIP must
  not land on PASS), a click confirms (`m5dd::buttonClicked()`, so the next step starts with the
  button released). Highlight logic in `verdict_choice.h` (host-tested). It logs
  `factory: prompt <step>` (scenarios wait for it) and `factory: verdict <step>=<v>`.
- **RFID reader missing:** the step logs `factory: rfid missing`, never runs `RFID_Test` or touches
  `M5Dial.Rfid`, and the prompt starts on FAIL.
- **LED level clamp:** C++ `/` truncates, so up to 3499 mV the original also gave 0 (3.3 V → 0).
  Above that it gave -1 (ring dark); the port clamps to 0, so the level stays 0..6 and two end
  pixels stay lit. The log's `leds=` value is the level, not the number of lit pixels.
- **Ring off after the step:** the original left the ring lit until the power-off; the port
  clears it when the IO step ends.
- **IO log line:** `factory: io adc=<mV> leds=<level> i2c=[<0xAA 0xBB>]` after every Port A scan
  (800 ms), not every frame.
- **Icons:** embedded apps get their launcher icon instead of upstream's empty sprite, which
  avoids `pushRotateZoom` on a 0x0 sprite. Cost: the three icon arrays are linked twice (~10 KB).
- **Kept from upstream:** the LCD intro ("[M5Dial测试] V1.0 <date> 松开按键 进入测试模式") and its
  release wait, the wake-up hint ("请移除电源 / 按下按键[关机] / 数秒后将自动启动"), ALL OK /
  CHECK FAIL, the 500 ms delay before `powerOff()`.
- **Scenario:** the `launcher.test.yaml` scenario no longer holds the button at boot (that would
  now enter the factory test). `enter`, `skip_io` and `io_leds` cover the test.

### LvglDemo

Sketch `examples/LvglDemo`, Task 16. A new sketch, not a port. Upstream ran LVGL 8
(`main/hal/lvgl/porting/`) only for the More menu's widgets, benchmark and stress demos, which
the port drops (D3); its flush pushed native RGB565 (`pushPixels(..., true)`), the byte order
used here too. LVGL 9.6 (`LIB_LVGL`), an arc 0..100 the knob drives, a centred value label, a
button that toggles the arc colour. Logs `lvgl: ready`, `lvgl: value=<n>` on every change and
`lvgl: colour=blue|orange` on every click.

- **`lv_conf.h` flags (`tools/build.sh`):** without a define, LVGL 9 includes
  `../../../../lv_conf.h` relative to `include/lvgl/config/`, i.e. `sketch/libraries/lv_conf.h`,
  one file shared by every sketch. Instead, `build.sh` checks for `<sketch dir>/lv_conf.h` and,
  only if it exists, adds `-DLV_CONF_INCLUDE_SIMPLE "-I<sketch dir>"` to both
  `compiler.c.extra_flags` (LVGL is C) and `compiler.cpp.extra_flags`. Both are empty in the
  core's `platform.txt` (3.3.10) and unset in `boards.txt`, so nothing is clobbered. Sketches
  without `lv_conf.h` build with exactly the same command line as before
  (`tools/tests/test_build_partitions.py`), and they never include `lvgl.h`, so arduino-cli does
  not compile LVGL for them at all. Only C/C++ flags are set: LVGL's only `.S` file (Helium) is
  wrapped in `#if __ARM_FEATURE_MVE`, so it is empty on Xtensa. Merging with `-DM5DD_SIM`:
  [Extra compiler flags](#extra-compiler-flags-m5dial_extra_flags).
- **Short `lv_conf.h`:** only the options this sketch sets (16 bpp, heap, no OS, no LVGL log,
  Montserrat 28, dark theme); `lv_conf_internal.h` supplies every other default.
- **Byte order:** LVGL renders RGB565 as native little-endian `uint16_t`. M5GFX's
  `pushImage(const uint16_t*)` treats the data as byte-swapped (`swap565_t`) unless
  `setSwapBytes(true)`; the flush passes `reinterpret_cast<const lgfx::rgb565_t*>(px_map)`,
  which means native RGB565 whatever the swap flag, and M5GFX converts to the panel's order while
  it copies. No `LV_COLOR_FORMAT_RGB565_SWAPPED`, no swap pass in LVGL.
- **Memory:** no 240x240 canvas (`m5dd::begin(name)`). LVGL renders into one static 240x24 px
  partial buffer (11.25 KB, `.bss`, aligned to `LV_DRAW_BUF_ALIGN`), flushed synchronously, so no
  second buffer. LVGL's heap is its builtin TLSF allocator over a static 64 KB array
  (`LV_MEM_SIZE`, `.bss`). Numbers: [Heap numbers](#heap-numbers).
- **Input:** encoder indev `enc_diff` = `EncoderDetent::update(dialEncoder.read())` (clockwise
  +1 = `LV_KEY_RIGHT` = value up), pressed = `M5Dial.BtnA.isPressed()`; pointer indev from
  `M5Dial.Touch.getDetail()`. `loop()` calls `M5Dial.update()` once, then `lv_timer_handler()`,
  then waits at most 5 ms. Tick: `lv_tick_set_cb(millis)` (LVGL 9's documented time source).
  The arc starts focused in edit mode; a knob press leaves edit mode, so the knob can then focus
  the button.
- **Not verified yet:** `sim/scenarios/LvglDemo/knob.test.yaml` (boot, `lvgl: ready`, CW x5 →
  `value=5`, CCW → `value=4`) passes on Wokwi (PR #50); the sketch has not run on the device. Colours, RGB565 byte order and the touch mapping are unchecked on a real panel.

### DialProbe

Sketch `examples/DialProbe`, the hardware probe (replaces Task 1/2's Smoke sketch; Smoke's
`board: 12` check became `init: board 12`). Not an upstream app.

- Starts with `m5dd::begin("DialProbe", canvas)` (logs `boot: DialProbe`, then `init: rfid ok|missing`).
- Then logs `init: board <n>` (12 = M5Dial, i.e. the panel was autodetected), `init: display WxH`,
  `init: rtc|touch ok|fail` (RTC: `Rtc.isEnabled()`; touch: VENDID `A8h` non-zero), `probe: ready`.
- Then `enc:`, `btn:`, `touch:`, `rtc:` (1 Hz) and `rfid: uid=` (card halted after each read, so
  logged once per presentation).
- Encoder: `enc: steps=<n> total=<sum> raw=<n>` per step and `enc: idle raw=<n> total=<sum>` once
  the knob has rested 100 ms. It pushes its full canvas every loop as deliberate load.
- The "encoder fast" steps of `probe.test.yaml` turn 20 detents at 0.5 ms per edge under that load
  and check the exact rest counts (`enc: idle raw=-80 total=20`, then after 40 detents back
  `enc: idle raw=80 total=-20`). The slow encoder steps keep `edgeIntervalUs = 40000`
  (direction/detent check only).

---

## Virtual M5Dial (Wokwi)

The device and the sim run the same sources; the sim build differs only by one flag.

### Sim fallback (`-DM5DD_SIM`)

`tools/sim.sh` always builds with `M5DIAL_EXTRA_FLAGS=-DM5DD_SIM` into `.build/sim/<Name>/`. The
flag enables two things (items 1 and 2); this list is everything the simulation does differently:

1. **Panel MISO route (code behind `M5DD_SIM`).** `m5dd::begin()`
   (`libraries/M5DialDemo/src/m5dd_begin.cpp`, `sim_route_panel_miso()`) runs it before
   `M5Dial.begin()`, so every sketch that uses `m5dd::begin` gets it (since Task 4; before, it was
   `examples/DialProbe/sim_shims.h`). It calls
   `esp_rom_gpio_connect_in_signal(GPIO_NUM_5, FSPIQ_IN_IDX, false)`.
   - Why: in 3-wire (SIO) mode, Wokwi's ESP32-S3 SPI samples the FSPIQ (MISO) input, not FSPID.
     M5GFX leaves FSPIQ unrouted, so the RDDID read returns `00000000` and autodetect fails.
   - On real hardware, SIO mode reads FSPID and ignores FSPIQ. The route is compiled only into the
     sim build anyway.
   - `cfg.fallback_board = board_M5Dial` (the plan's fallback) was tried first and dropped. It
     never takes effect: when the display probe fails, M5Unified's GPIO-pattern probe
     (M5Unified.inl:1895-1898) still reports a board (`board_M5StampS3`, 136), so `board_detected`
     is true; and M5GFX would have no panel either.
2. **No internal speaker (code behind `M5DD_SIM`, #39).** `m5dd::begin()` calls
   `sim_disable_speaker()` (`cfg.internal_spk = false`) before `M5Dial.begin()`, so
   `M5Dial.Speaker.tone()` is silent in the sim.
   - Why: M5Unified plays the Dial's buzzer (GPIO3) through I2S DMA from its `spk_task`, which the
     first tone starts. In Wokwi's ESP32-S3 the sketch then stops at the first click tone, with no
     log line, reset or panic, until `wokwi-cli` times out. That was the #39 hang: every sketch
     on `m5dd::begin()` went silent at its first knob turn or button press. DialProbe, which plays
     no tone, passed.
   - Evidence (local `wokwi-cli`, RtcTest scenario): with the speaker, silence right after the
     button press (exit 42); with this, the scenario passes.
   - No scenario checks sound, so nothing is lost. The device build keeps the tones.
3. **Chip attribute `readDummyBits: "0"` in the reference `sim/diagram.json`** (not code).
   - M5GFX makes its single dummy read clock on the S3 by flipping `SPI_CK_IDLE_EDGE`. Wokwi
     produces no SCK edge for that.
   - The chip counted 32 clocks per `Bus_SPI` RDDID read, and 33 when the dummy clock was
     bit-banged.
   - The real panel needs 1, which is the chip default.
4. **Separate build output.** The sim build goes to `.build/sim/<Name>/` (`M5DIAL_BUILD_DIR`), so
   it never overwrites the device build in `.build/<Name>/` that `upload.sh` flashes. `upload.sh`
   ignores an inherited `M5DIAL_BUILD_DIR`.

Not different:
- **Serial:** `M5_LOGx` lines go through `printf` to the UART0 console, which Wokwi's serial
  monitor and scenarios read as is. In one diagnostic, plain `Serial.printf` output
  did not appear while `esp_rom_printf` did. The project logs via `M5_LOG` only.
- **Peripherals:** the chips model the parts; the diagram adds the button pull-up resistor (on the
  real board external) and ties the RFID/touch RST inputs high.
- **Wi-Fi:** Wokwi's ESP32-S3 always provides the open access point `Wokwi-GUEST` (no part in
  `diagram.json`). The scan sees it; nothing connects.
- **BLE:** not modelled. Whether `NimBLEDevice::init()` returns cleanly there is unverified; if it
  doesn't, BleServer and the launcher BLE leg need an `M5DD_SIM` guard ([Open items](#open-items)).

### Display wiring: source of truth

`sketch/sim/diagram.json` wires the `gc9a01` chip to the pins that M5GFX's M5Dial autodetect
uses. The pins come from `sketch/libraries/M5GFX/src/M5GFX.cpp` in M5GFX 0.2.35, in the
ESP32-S3 / `pkg_ver == 0` branch:

| Signal | GPIO | M5GFX.cpp line |
|--------|------|----------------|
| RST    | 8    | 1937 `_pin_reset(GPIO_NUM_8, ...)` |
| MOSI   | 5    | 1938 `bus_cfg.pin_mosi = GPIO_NUM_5` (3-wire, `pin_miso = -1`, `spi_3wire = true`) |
| SCLK   | 6    | 1940 |
| DC     | 4    | 1941 |
| CS     | 7    | 1946 `_read_panel_id(bus_spi, GPIO_NUM_7)` |
| BL     | 9    | 1969 `_set_pwm_backlight(GPIO_NUM_9, 7, 44100)` |

How the Dial is detected:
1. `_read_panel_id` (M5GFX.cpp:892-907) raises CS and sends command `00h`.
2. It lowers CS, sends `04h` (RDDID), calls `beginRead(1)` (one dummy bit) and reads 32 bits from
   the shared data line.
3. The board is an M5Dial if `(id & 0xFFFFFF) == 0x019a00` (M5GFX.cpp:1947).

M5Unified then takes the board from `Display.getBoard()`. `board_M5Dial == 12` (M5GFX
`src/lgfx/boards.hpp:23`).

### Internal I2C bus and peripherals: source of truth

All three I2C chips sit on M5Unified's internal bus `In_I2C`, I2C port 1
(`M5Unified.inl:2158` `in_port = I2C_NUM_1`), SDA GPIO11 / SCL GPIO12 (`M5Unified.inl:106`, table
columns "In SCL,SDA" = 12, 11). Versions: M5Unified/M5GFX/M5Dial as pinned in `tools/deps.txt`.

| Part (diagram id) | Address | Other pins | Evidence |
|-------------------|---------|------------|----------|
| FT3267 touch (`touch`) | `0x38` | INT GPIO14 | M5GFX.cpp:1982 (`pin_int = 14`), :1986-1989 (port 1, `0x38`, SDA 11, SCL 12) |
| PCF8563 RTC (`rtc`) | `0x51` | INT not wired to the MCU | PCF8563_Class.hpp:14; `M5.Rtc.begin(&M5.In_I2C, ...)` M5Unified.inl:3403; PCF8563 is the default instance, RTC_Class.inl:101 |
| WS1850S RFID (`rfid`) | `0x28` | RST tied high | M5Dial MFRC522.h:85, M5Dial.h:36; register access via `M5.In_I2C` at 100 kHz, MFRC522.cpp:47-81 |
| Encoder (`enc`) | - | A GPIO41, B GPIO40 | M5Dial.h:9-10, :35 (`ENCODER(PIN_A, PIN_B)`) |
| Front button (`btn`) | - | GPIO42, active low, 10k pull-up `btn_pu` | M5Unified.inl:3588-3591 (`!gpio_in(GPIO_NUM_42)`), :2471 (input, no internal pull-up) |
| Buzzer (`buzzer`) | - | GPIO3 | M5Unified.inl:3007-3012 (`pin_data_out = GPIO_NUM_3`, `buzzer = true`) |
| NeoPixel ring x14 (`ring`), potentiometer (`pot`) | - | Port B: GPIO2 / GPIO1 | M5Unified.inl:171 (Port B pins 1, 2) |

### What the drivers access (and the chips model)

- **Touch** (M5GFX `Touch_FT5x06.inl`): init check writes `00h = 00`, reads 6 bytes from `A3h`
  (CIPHER .. `A8h` VENDID, must be non-zero), writes `A4h = 00` (polling mode) (:58-61).
  `getTouchRaw` reads INT first and only polls I2C while INT is low (:145-156), then reads `02h`
  (TD_STATUS) and, in the same read, `points*6-2` more bytes (`03h`-`06h` for one point) (:113-131);
  it reads twice and compares, so reports must be stable between reads (:162-168). X/Y are the
  low 12 bits of `03h/04h`, `05h/06h`; id = `05h >> 4` (:175-177). Wake = INT pulled low, then
  `A5h = 01` (:91-101); sleep `A5h = 03`.
- **RTC** (M5Unified `PCF8563_Class.inl`): begin writes `00h = 00` twice and `0Eh = 03`; the
  ACKs decide `Rtc.isEnabled()` (:21-22). `getDateTime` reads `02h`-`08h` and rejects invalid BCD
  (:31-85); century bit set = 19xx (:76, :106). Alarm `09h`-`0Ch` + `01h` bit AIE (:146-194),
  timer `0Eh/0Fh` + TIE (:113-144), flags cleared via `bitOff(01h, 0Ch)` (:201-205),
  `disableIRQ` (:207-219) runs at boot (M5Unified.inl:3412-3413). `setSystemTimeFromRtc` then
  sets the ESP32 clock (RTC_Class.inl:204-234).
- **RFID** (M5Dial `MFRC522.cpp`): `PCD_Init` = SoftReset, spin while CommandReg bit 4 (:229-243 -
  no timeout, hence the [RFID presence probe](#rfid-presence-probe)), timer `2Ah-2Dh` = 25 ms
  TAuto, `TxASKReg`, `ModeReg = 3Dh` (CRC preset 6363h), antenna on (:197-252). Every exchange is
  `PCD_CommunicateWithPICC` (:388-485): Idle, clear ComIrq, flush FIFO, write FIFO, BitFraming,
  Transceive + StartSend, poll ComIrqReg for RxIRq/IdleIRq (success) or TimerIRq (timeout),
  ErrorReg, FIFOLevel, FIFO burst read (no address auto-increment), ControlReg RxLastBits. CRCs
  come from the CRC coprocessor (`PCD_CalculateCRC`, :159-187). `PICC_IsNewCardPresent` = REQA
  (7-bit frame) (:1748-1753), `PICC_ReadCardSerial` = CL1 ANTICOLLISION (NVB 20h) + SELECT
  (NVB 70h, CRC_A, SAK + CRC_A checked) (:550-760), `PICC_HaltA` succeeds only on timeout (:765-790).
- **Encoder** (M5Dial `utility/Encoder.h`, PJRC Encoder): `begin()` sets INPUT_PULLUP and reads the
  initial state; `attach_interrupt` has no entry for GPIO40/41 (`CORE_INT*_PIN` list), so `read()`
  polls the pins (`interrupts_in_use < 2`). x4 decoding: one detent = 4 counts. The port's
  `dialEncoder` counts in an ISR instead ([Encoder](#encoder)).

### Encoder: chip instead of the stock KY-040 (decision)

Wokwi's `set-control` supports pushbutton, potentiometer and a few sensors, but not the KY-040
(docs.wokwi.com/wokwi-ci/automation-scenarios; the KY-040 page lists no automation controls), so
scenarios cannot turn it. `sim/chips/encoder/` replaces it: control `steps` (n detents, n > 0 =
clockwise = A leads B, like the KY-040's "CLK goes low first"), control `edgeIntervalUs` for fast
turns. The diagram therefore has no KY-040 (both would drive GPIO40/41).

### Touch coordinates

The `ft3267` chip's `x`/`y` controls cover the controller's 12-bit range (0-4095). The M5Dial
panel reports 0-239 (M5GFX.cpp:1977-1980 `x_max = y_max = 239`), so scenarios use that range.

### Known sim limits

- **No screenshots of custom-chip displays.** wokwi-cli reports "Part does not have a valid
  framebuffer: lcd".
  - The chip (`frameLog: "1"`) prints `gc9a01: frame fnv=<hash> uniform=<RRGGBB|none>` after each
    RAMWR burst. Scenarios check it with `expect-chip-output`.
  - That step does not pause the simulation. It must be the last step of a scenario, or come after
    the serial lines it would otherwise race.
- Display rotation is checked for rotation 0 only. MADCTL MV/MX/MY mapping for other rotations is
  unverified.
- The backlight (GPIO9 PWM) is not modelled. The panel is lit whenever DISPON is set and the panel
  is out of sleep.
- No BLE (see [Sim fallback](#sim-fallback--dm5dd_sim)).
- Wokwi occasionally drops the connection (`API Error: Connection to transport closed
  unexpectedly: code 1006`). CI retries the scenario once. The first attempt's failure stays
  visible in the log.
- The free plan has a monthly CI-minute quota; every scenario run (local or CI) counts.

### Diagnostics

- `SIM_DEBUG_LEVEL=<0-5> tools/sim.sh ...` builds with `build.code_debug=<n>`.
  - It is for diagnostics only and never set in CI.
  - M5GFX's `ESP_LOGD` autodetect lines still did not show at level 5.
- Add `"debug": "1"` to the chip's attrs in `diagram.json` to trace every command and read.

### Upstream bug report draft (Wokwi, for the owner to decide)

**Title:** ESP32-S3: SPI half-duplex 3-wire (SIO) reads sample MISO instead of the MOSI/D line

**Summary:**
- With `SPI_USER_REG.SPI_SIO` set (3-wire, data in and out on the D line), an SPI2 read returns
  whatever FSPIQ is routed to, not the D line.
- With FSPIQ unrouted, reads return 0. Real ESP32-S3 hardware reads the D line (FSPID input) in
  this mode.
- This breaks M5GFX/LovyanGFX panel autodetection on 3-wire panels, e.g. the M5Stack Dial
  (GC9A01, RDDID 04h).

**Repro:**
- ESP32-S3 DevKitC-1 and a custom SPI chip that drives the shared data line (GPIO5) during a
  register read.
- SCK GPIO6, DC GPIO4, CS GPIO7.
- `lgfx::Bus_SPI` (M5GFX 0.2.35) with `spi_3wire = true`, `pin_miso = -1`, `pin_mosi = 5`.
- Sequence: `writeCommand(0x04)`, `beginRead()`, `readData(32)`.

**Observed (wokwi-cli 0.29.1, simulator 1.0.0-20261026):**
- bit-banged read of the same chip: `009a0100`
- `Bus_SPI` 3-wire read: `00000000`
- `Bus_SPI` 4-wire read with `pin_miso = 5` (the same pin): `00019a00`

The chip saw the full 32 clocks in every case.

**Expected:** the 3-wire read returns `00019a00`, the same as 4-wire.

**Secondary issue:** a one-shot `SPI_CK_IDLE_EDGE` flip, which M5GFX uses to make a single dummy
clock on the C3/S3, produces no SCK edge. On hardware it produces one clock.

Core of the probe (M5GFX `lgfx::Bus_SPI`, CS on GPIO7 driven by hand):

```cpp
lgfx::Bus_SPI bus; auto cfg = bus.config();
cfg.spi_host = SPI2_HOST; cfg.spi_mode = 0; cfg.freq_read = 16000000;
cfg.pin_sclk = 6; cfg.pin_mosi = 5; cfg.pin_dc = 4;
cfg.pin_miso = -1; cfg.spi_3wire = true;      // 4-wire variant: pin_miso = 5, spi_3wire = false
bus.config(cfg); bus.init();
bus.beginTransaction(); digitalWrite(7, LOW);
bus.writeCommand(0x04, 8); bus.beginRead(0);
uint32_t id = bus.readData(32);               // Wokwi: 00000000 (3-wire) vs 00019a00 (4-wire)
bus.endTransaction(); digitalWrite(7, HIGH);
```

---

## CI

- **Two workflows:** the free checks are kept apart from the one that spends Wokwi quota.

  | Workflow / job | What it runs | When | Wokwi quota |
  | --- | --- | --- | --- |
  | `sketch` / `chips` | chip host tests (`make test`, native cc) | every push to `main` and every PR touching `sketch/**`, plus `workflow_dispatch` (port branches build through their PR, so no double run) | none |
  | `sketch` / `host` | `tools/test_host.sh` (clang++) | same | none |
  | `sketch` / `build` | `setup.sh`, Python tool tests, chip wasm builds, `tools/build_all.sh` (UserDemo + every `examples/*`, flash use in the step summary) | same | none |
  | `sim` / `scenario` | quota gate (chip host tests + `test_sim_ci.py`), then every `sim/scenarios/**/*.test.yaml` via `tools/sim_ci.sh` | only `workflow_dispatch` and same-repo PRs touching `sketch/sim/**`, `sketch/examples/**`, `sketch/libraries/M5DialDemo/**`, `sketch/UserDemo/**`, `sketch/tools/**` or `sim.yml`, and on PRs only while the PR carries the `sim` label; never on push | yes |

  - Fork PRs never get `WOKWI_CLI_TOKEN`; their scenario job is skipped.
  - `scenario` is **opt-in** on pull requests: adding the `sim` label starts a run; without it the
    job is skipped, which passes branch protection. Since the first real run (PR #38) a full run
    costs ~20 CI minutes, too much for every push.
  - A used-up quota marks the remaining scenarios SKIPPED (warning + step summary) instead of
    failing.
  - `build_all.sh` builds every sketch even after a failure, and the step fails if any failed.
    A missing `UserDemo/UserDemo.ino` fails the step (exit 2); an `examples/<Name>/` without
    `<Name>.ino` is skipped with a warning.
    In the `build` job the Python tests, chip builds and sketch builds each run whenever setup
    succeeded, so one run reports every broken part.
  - No `secrets.h` copy: `config.h` includes it only via `__has_include`.
  - Both workflows share one toolchain cache entry, keyed on `deps.txt` + `setup.sh`.
  - `concurrency` per ref. On a PR a newer commit cancels the older run. `sketch` runs on `main`
    are never cancelled, so every merged commit keeps its own result.
  - Why two workflows: one merged `sketch/**` workflow would need a hand-rolled changed-files
    check, or a docs-only PR would spend Wokwi quota. A job can't `needs:` a job in another
    workflow, so the scenario job runs its own quota gate (seconds, no quota) before any scenario;
    build failures spend no quota either, because `sim.sh` builds before it runs `wokwi-cli`.
- **One Wokwi session per scenario file:** `sim/scenarios/<Sketch>/*.test.yaml` runs against
  `examples/<Sketch>` (or `<Sketch>`). DialProbe's checks (init, RTC, button, touch, encoder, RFID)
  are the steps of one scenario, `probe.test.yaml`. UserDemo has six files, because each power-off
  ends a boot.
- **Bounds:** 20 s of simulated time per scenario. It retries once, and only when the output
  contains Wokwi's transport-drop text ("Connection to transport closed unexpectedly", code
  1006). It aborts everything when Wokwi reports the CI-minute quota as used up.
- **History:** the first run of Task 3's CI (40954686971) used a 60 s limit and six separate
  scenarios. One scenario waited for a line that never came until Wokwi's 5-minute wall-clock
  session limit, and that used up the monthly free quota.
- **No display check in DialProbe:** no `frameLog` / `expect-chip-output` check is in the combined
  scenario. The step does not pause the simulation and must be last. DialProbe's last frame is
  not uniform (it shows text), and its only frames come before `probe: ready`. `init: board 12`
  covers panel autodetection instead.
- **First real run still to check:** job names, the 11-sketch Linux build table (the first Linux
  build of all sketches, with arduino-cli's bundled ctags), and `clang++` on ubuntu-24.04.

---

## Open items

Everything below is listed with its exact check in [TEST_CHECKLIST.md](TEST_CHECKLIST.md).

- **Device runs pending** (device offline during Tasks 7-17): every sketch on the real Dial,
  encoder direction on the real knob, MemSoak min heap, BLE with `ble_check.py`, More → Power Off
  on USB power, factory RTC wake-up, LvglDemo colours/byte order/touch, `Ex_I2C` scan time on a
  bare Port A.
- **Scenarios** pass on Wokwi since the #39 fix (PR #50: CI run 42501330101 passed 13 of 14; the
  14th, the one-session launcher run, outgrew the free plan's 5-minute session and was split into
  `launcher`, `launcher_apps` and `launcher_touch`, each passing on its own). The settle delays
  work as written; they still rely on `wait-serial` consuming the stream in order and on
  `steps: 0` stopping a running encoder burst.
- **Sim assumptions:** whether NimBLE init returns in Wokwi; `io_leds` assumes Wokwi's ADC reads
  < 100 mV at pot position 0 and ≥ 2600 mV at position 1; the RFID chip resets a card to IDLE when
  `cardPresent` goes to 0. (Settled since: the Wokwi ESP32-S3 boots the 8 MB merged image only
  with `"flashSize": "8"` on the board part, which `sim/diagram.json` sets, PR #38.)
- **Decisions open to the owner:** Wi-Fi rescan on touch (vs short press = rescan, long press =
  quit); launcher CCW from slot 0 lands on empty slot 9 (vs shrinking the ring to 8 slots).
