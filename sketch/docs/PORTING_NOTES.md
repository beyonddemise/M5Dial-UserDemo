# Porting notes

## Virtual M5Dial (Wokwi), Task 2

### Display wiring: source of truth

`sketch/sim/diagram.json` wires the `gc9a01` chip to the pins that M5GFX's M5Dial
autodetect uses. The pins come from `sketch/libraries/M5GFX/src/M5GFX.cpp` in M5GFX 0.2.35,
in the ESP32-S3 / `pkg_ver == 0` branch:

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
2. It lowers CS, sends `04h` (RDDID), calls `beginRead(1)` (one dummy bit) and reads 32 bits from the shared data line.
3. The board is an M5Dial if `(id & 0xFFFFFF) == 0x019a00` (M5GFX.cpp:1947).

M5Unified then takes the board from `Display.getBoard()`. `board_M5Dial == 12` (M5GFX `src/lgfx/boards.hpp:23`).

### Sim-only differences

Everything the simulation does differently from the device build:

1. **Panel MISO route (`-DM5DD_SIM`, which `tools/sim.sh` always sets).**
   `m5dd::begin()` (`libraries/M5DialDemo/src/m5dd_begin.cpp`, `sim_route_panel_miso()`) runs it before `M5Dial.begin()`,
   so every sketch that uses `m5dd::begin` gets it (Task 4; it was `examples/DialProbe/sim_shims.h` before).
   It calls `esp_rom_gpio_connect_in_signal(GPIO_NUM_5, FSPIQ_IN_IDX, false)`.
   - Why: in 3-wire (SIO) mode, Wokwi's ESP32-S3 SPI samples the FSPIQ (MISO) input, not FSPID.
     M5GFX leaves FSPIQ unrouted, so the RDDID read returns `00000000` and autodetect fails.
   - On real hardware, SIO mode reads FSPID and ignores FSPIQ. The route is compiled only into the sim build anyway.
   - `cfg.fallback_board = board_M5Dial` was tried first and dropped. It never takes effect:
     - when the display probe fails, M5Unified's GPIO-pattern probe (M5Unified.inl:1895-1898) still reports a board (`board_M5StampS3`, 136), so `board_detected` is true;
     - M5GFX would have no panel either.
2. **Chip attribute `readDummyBits: "0"` in the reference `sim/diagram.json`.**
   - M5GFX makes its single dummy read clock on the S3 by flipping `SPI_CK_IDLE_EDGE`. Wokwi produces no SCK edge for that.
   - The chip counted 32 clocks per `Bus_SPI` RDDID read, and 33 when the dummy clock was bit-banged.
   - The real panel needs 1, which is the chip default.
3. **Separate build output.** The sim build goes to `.build/sim/<Name>/` (`M5DIAL_BUILD_DIR`), so it
   never overwrites the device build in `.build/<Name>/` that `upload.sh` flashes.

No sim-only change is needed for serial. USB-CDC on boot (`CDCOnBoot=cdc`, hardware
CDC/JTAG) output reaches Wokwi's serial monitor and scenarios as is (`M5_LOGx` lines).
In one diagnostic, plain `Serial.printf` output did not appear while `esp_rom_printf` did.
The project logs via `M5_LOG` only.

### Known sim limits

- **No screenshots of custom-chip displays.** wokwi-cli reports "Part does not have a valid framebuffer: lcd".
  - The chip (`frameLog: "1"`) prints `gc9a01: frame fnv=<hash> uniform=<RRGGBB|none>` after each RAMWR burst. Scenarios check it with `expect-chip-output`.
  - That step does not pause the simulation. It must be the last step of a scenario, or come after the serial lines it would otherwise race.
- Display rotation is checked for rotation 0 only. MADCTL MV/MX/MY mapping for other rotations is unverified.
- The backlight (GPIO9 PWM) is not modelled. The panel is lit whenever DISPON is set and the panel is out of sleep.
- Wokwi occasionally drops the connection (`API Error: Connection to transport closed unexpectedly: code 1006`).
  `sim.yml` retries the scenario once. The first attempt's failure stays visible in the log.

### Diagnostics

- `SIM_DEBUG_LEVEL=<0-5> tools/sim.sh ...` builds with `build.code_debug=<n>`.
  - It is for diagnostics only and never set in CI.
  - M5GFX's `ESP_LOGD` autodetect lines still did not show at level 5.
- Add `"debug": "1"` to the chip's attrs in `diagram.json` to trace every command and read.

### Upstream bug report draft (Wokwi, for the owner to decide)

**Title:** ESP32-S3: SPI half-duplex 3-wire (SIO) reads sample MISO instead of the MOSI/D line

**Summary:**
- With `SPI_USER_REG.SPI_SIO` set (3-wire, data in and out on the D line), an SPI2 read returns whatever FSPIQ is routed to, not the D line.
- With FSPIQ unrouted, reads return 0. Real ESP32-S3 hardware reads the D line (FSPID input) in this mode.
- This breaks M5GFX/LovyanGFX panel autodetection on 3-wire panels, e.g. the M5Stack Dial (GC9A01, RDDID 04h).

**Repro:**
- ESP32-S3 DevKitC-1 and a custom SPI chip that drives the shared data line (GPIO5) during a register read.
- SCK GPIO6, DC GPIO4, CS GPIO7.
- `lgfx::Bus_SPI` (M5GFX 0.2.35) with `spi_3wire = true`, `pin_miso = -1`, `pin_mosi = 5`.
- Sequence: `writeCommand(0x04)`, `beginRead()`, `readData(32)`.

**Observed (wokwi-cli 0.29.1, simulator 1.0.0-20261026):**
- bit-banged read of the same chip: `009a0100`
- `Bus_SPI` 3-wire read: `00000000`
- `Bus_SPI` 4-wire read with `pin_miso = 5` (the same pin): `00019a00`

The chip saw the full 32 clocks in every case.

**Expected:** the 3-wire read returns `00019a00`, the same as 4-wire.

**Secondary issue:** a one-shot `SPI_CK_IDLE_EDGE` flip, which M5GFX uses to make a single dummy clock on the C3/S3, produces no SCK edge. On hardware it produces one clock.

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

## Virtual M5Dial completion (Wokwi), Task 3

### Internal I2C bus and peripherals: source of truth

All three I2C chips sit on M5Unified's internal bus `In_I2C`, I2C port 1
(`M5Unified.inl:2158` `in_port = I2C_NUM_1`), SDA GPIO11 / SCL GPIO12
(`M5Unified.inl:106`, table columns "In SCL,SDA" = 12, 11). Versions: M5Unified/M5GFX/M5Dial as pinned in `tools/deps.txt`.

| Part (diagram id) | Address | Other pins | Evidence |
|-------------------|---------|------------|----------|
| FT3267 touch (`touch`) | `0x38` | INT GPIO14 | M5GFX.cpp:1982 (`pin_int = 14`), :1986-1989 (port 1, `0x38`, SDA 11, SCL 12) |
| PCF8563 RTC (`rtc`) | `0x51` | INT not wired to the MCU | PCF8563_Class.hpp:14; `M5.Rtc.begin(&M5.In_I2C, ...)` M5Unified.inl:3403; PCF8563 is the default instance, RTC_Class.inl:101 |
| WS1850S RFID (`rfid`) | `0x28` | RST tied high | M5Dial MFRC522.h:85, M5Dial.h:36; register access via `M5.In_I2C` at 100 kHz, MFRC522.cpp:47-81 |
| Encoder (`enc`) | - | A GPIO41, B GPIO40 | M5Dial.h:9-10, :35 (`ENCODER(PIN_A, PIN_B)`) |
| Front button (`btn`) | - | GPIO42, active low, 10k pull-up `btn_pu` | M5Unified.inl:3588-3591 (`!gpio_in(GPIO_NUM_42)`), :2471 (input, no internal pull-up) |
| Buzzer (`buzzer`) | - | GPIO3 | M5Unified.inl:3007-3012 (`pin_data_out = GPIO_NUM_3`, `buzzer = true`) |
| NeoPixel ring x14 (`ring`), potentiometer (`pot`) | - | Port B: GPIO2 / GPIO1 | M5Unified.inl:171 (Port B pins 1, 2) |

Wi-Fi: Wokwi's ESP32-S3 always provides the open access point `Wokwi-GUEST` (no part in
`diagram.json` is needed or exists). Sketches that use Wi-Fi under `M5DD_SIM` connect to it
(scenarios using it come with the Wi-Fi tasks).

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
  `disableIRQ` (:207-219) runs at boot (M5Unified.inl:3412-3413). `setSystemTimeFromRtc` then sets the ESP32 clock (RTC_Class.inl:204-234).
- **RFID** (M5Dial `MFRC522.cpp`): `PCD_Init` = SoftReset, spin while CommandReg bit 4 (:229-243 -
  spins forever on an unanswered read, so DialProbe probes VersionReg `37h` first and calls
  `PCD_Init` only if the reader answers), timer `2Ah-2Dh` = 25 ms TAuto, `TxASKReg`, `ModeReg = 3Dh`
  (CRC preset 6363h), antenna on (:197-252). Every exchange is `PCD_CommunicateWithPICC` (:388-485):
  Idle, clear ComIrq, flush FIFO, write FIFO, BitFraming, Transceive + StartSend, poll ComIrqReg for
  RxIRq/IdleIRq (success) or TimerIRq (timeout), ErrorReg, FIFOLevel, FIFO burst read (no address
  auto-increment), ControlReg RxLastBits. CRCs come from the CRC coprocessor (`PCD_CalculateCRC`,
  :159-187). `PICC_IsNewCardPresent` = REQA (7-bit frame) (:1748-1753), `PICC_ReadCardSerial` =
  CL1 ANTICOLLISION (NVB 20h) + SELECT (NVB 70h, CRC_A, SAK + CRC_A checked) (:550-760),
  `PICC_HaltA` succeeds only on timeout (:765-790).
- **Encoder** (M5Dial `utility/Encoder.h`, PJRC Encoder): `begin()` sets INPUT_PULLUP and reads the
  initial state; `attach_interrupt` has no entry for GPIO40/41 (`CORE_INT*_PIN` list), so `read()`
  polls the pins (`interrupts_in_use < 2`). x4 decoding: one detent = 4 counts.

### Encoder: chip instead of the stock KY-040 (decision)

Wokwi's `set-control` supports pushbutton, potentiometer and a few sensors, but not the KY-040
(docs.wokwi.com/wokwi-ci/automation-scenarios; the KY-040 page lists no automation controls), so
scenarios cannot turn it. `sim/chips/encoder/` replaces it: control `steps` (n detents, n > 0 =
clockwise = A leads B, like the KY-040's "CLK goes low first"), control `edgeIntervalUs` for fast
turns. The diagram therefore has no KY-040 (both would drive GPIO40/41).

### Encoder direction: `M5DD_ENCODER_CW_SIGN = -1`

With A on GPIO41 (`ENCODER` pin1) leading B on GPIO40, PJRC's state table counts down: from rest
(both high) A falls -> state `1011b` -> `position--` (Encoder.h `update()`), so one clockwise detent
gives raw `-4`. `M5DD_ENCODER_CW_SIGN` lives in `libraries/M5DialDemo/src/config.h`; `EncoderDetent`
multiplies by it, so clockwise logs e.g. `enc: steps=1 total=1 raw=-3`.

### Encoder detents: centred window

`EncoderDetent` index = `floor((raw + 2) / 4)`, so detent 0 covers raw -2..+1. The knob rests at
raw = multiple of 4, i.e. in the middle of a window: +-1 contact chatter at rest never steps, and a
step fires at the half-detent point (third edge clockwise, second edge counter-clockwise; with 4
integer counts per detent the window cannot be exactly symmetric). The boundary itself (raw 2 mod 4)
is mid-detent, where the knob does not rest, so moving back across it undoes the step. Floor (not
truncating) division keeps detent 0 as wide as every other.

### RFID presence probe (deviation from the plan)

The plan has `m5dd::begin` call `M5Dial.begin(cfg, false, true)`. `MFRC522::PCD_Reset()` busy-waits
on `CommandReg` (`while (PCD_ReadRegister(CommandReg) & (1<<4))`, M5Dial `utility/MFRC522.cpp`), and a
failed I2C read leaves the value uninitialised, so a missing or dead reader would hang every sketch at
boot. `m5dd::begin` therefore calls `M5Dial.begin(cfg, false, false)`, reads VersionReg `37h` at
`RC522_I2C_ADDRESS` over `M5.In_I2C` (100 kHz) and calls `M5Dial.Rfid.begin()` (= `PCD_Init()`) only if
the reader ACKs with a value other than 00h/FFh. It logs `init: rfid ok|missing`; sketches check
`m5dd::rfidPresent()` before using `M5Dial.Rfid`. The encoder steps of `probe.test.yaml` are the check in the simulator; which
physical direction is "A leads" on the real knob is confirmed in Task 19's physical check.

### DialProbe

`examples/DialProbe` replaces Task 1/2's Smoke sketch; Smoke's `board: 12` check became `init: board 12`.
It logs `init: board <n>` (12 = M5Dial, i.e. the panel was autodetected), `init: display WxH`, `init: rtc|touch ok|fail` (RTC: `Rtc.isEnabled()`; touch: VENDID
`A8h` non-zero; RFID: `init: rfid ok|missing` from `m5dd::begin`, VersionReg not 00h/FFh, logged before the others), `probe: ready`, then `enc:`, `btn:`, `touch:`,
`rtc:` (1 Hz) and `rfid: uid=` (card halted after each read, so it is logged once per presentation).
Since Task 4 it starts with `m5dd::begin("DialProbe", canvas)` (logs `boot: DialProbe` and
`init: rfid ok|missing`), counts the encoder with the interrupt-driven `dialEncoder` + `EncoderDetent`,
logs `enc: steps=<n> total=<sum> raw=<n>` per step and `enc: idle raw=<n> total=<sum>` once the knob has
rested 100 ms, and pushes its full canvas every loop as deliberate load. The "encoder fast" steps of
`probe.test.yaml` turn 20 detents at 0.5 ms per edge under that load and check the exact rest counts
(`enc: idle raw=-80 total=20`, then after 40 detents back `enc: idle raw=80 total=-20`). The slow encoder steps keep `edgeIntervalUs = 40000` (direction/detent check only).

### Sim-only differences (additions)

None. The chips model the parts; the diagram adds the button pull-up resistor (on the real board
external) and ties the RFID/touch RST inputs high.

### CI

- **Triggers (Wokwi minutes are a monthly quota):**
  - job `chips` (chip host tests, no Wokwi) runs on pushes to `port/**` that touch `sketch/sim/chips/**`;
  - job `scenario` runs only on `workflow_dispatch` and on pull requests (opened, synchronize,
    ready_for_review) that touch `sketch/sim/**`, `sketch/examples/**`, `sketch/libraries/M5DialDemo/**`,
    `sketch/UserDemo/**` or the workflow, and only for same-repo PRs, so fork PRs never get `WOKWI_CLI_TOKEN`;
  - `concurrency` per ref with cancel-in-progress, so a newer commit stops the older run.
- **One Wokwi session per sketch:** DialProbe's checks (init, RTC, button, touch, encoder, RFID) are
  the steps of one scenario, `sim/scenarios/DialProbe/probe.test.yaml`. The loop runs every
  `sim/scenarios/<Sketch>/*.test.yaml` against `examples/<Sketch>` (or `<Sketch>`).
- **Bounds:** 20 s of simulated time per scenario. It retries once, and only when the output contains
  Wokwi's transport-drop text ("Connection to transport closed unexpectedly", code 1006). It aborts
  everything when Wokwi reports the CI-minute quota as used up.
- **History:** the first run of this task (40954686971) used a 60 s limit and six separate scenarios.
  One scenario waited for a line that never came until Wokwi's 5-minute wall-clock session limit,
  and that used up the monthly free quota.
- **No display check:** no `frameLog` / `expect-chip-output` check is in the combined scenario.
  The step does not pause the simulation and must be last. DialProbe's last frame is not uniform (it
  shows text), and its only frames come before `probe: ready`. `init: board 12` covers panel
  autodetection instead.

### Touch coordinates

The `ft3267` chip's `x`/`y` controls cover the controller's 12-bit range (0-4095). The M5Dial
panel reports 0-239 (M5GFX.cpp:1977-1980 `x_max = y_max = 239`), so scenarios use that range.

## Launcher and smooth_menu, Task 6

### smooth_menu

`src/smooth_menu/` is upstream `main/apps/utilities/smooth_menu/src` with its MIT `LICENSE` and
`README.md`. It has no hardware dependency. The only edits are `-Wall -Wextra` fixes: unused
parameter names commented out, and signed/unsigned comparisons (`size_t` loop index, an
`int` cast in `Selector_t::goToItem`). Each edited file's header says so. Because it is
Arduino-free, `tools/test_host.sh` compiles it into the host tests (`test_smooth_menu.cpp`).

### Library linkage: `dot_a_linkage=true`

`library.properties` sets `dot_a_linkage=true`, so the library is linked as an archive and a sketch
pulls in only the objects it references. Without it, every standalone sketch linked `launcher.cpp`
and ran its static constructors at boot: 8 icon `M5Canvas` objects and the icon and tag tables
(16 `std::string` tags). DialProbe
measured 540,283 B flash / 31,612 B globals with it off, and 537,283 B / 27,964 B with it on.

### Launcher: HAL call mapping

| Original (`_data.hal->...`)                           | Port                                                                 |
|-------------------------------------------------------|----------------------------------------------------------------------|
| `canvas` (`LGFX_Sprite*`)                             | `AppContext::canvas` (`M5Canvas*`, from `getUserData()`)            |
| `_canvas_update()` = `canvas->pushSprite(0, 0)`       | `Launcher::_canvas_update()`, same call                             |
| `encoder.wasMoved(true)` + `getDirection() < 1` → next | `EncoderDetent::update(dialEncoder.read())`: each step > 0 → `goNext()`, < 0 → `goLast()` |
| HAL encoder-moved callback `buzz.tone(6000/7000, 20)` | `M5Dial.Speaker.tone(6000, 20)` on next, `(7000, 20)` on previous   |
| `!encoder.btn.read()` (pressed, level)                | `M5Dial.BtnA.wasPressed()` (edge, see below)                        |
| HAL button callback `buzz.tone(2000, 20)`             | `M5Dial.Speaker.tone(2000, 20)`                                     |
| `while (!encoder.btn.read())` hold loop               | `while (M5Dial.BtnA.isPressed()) { M5Dial.update(); ... }`          |
| `tp.isTouched()` + `tp.update()` + `getTouchPointBuffer()` | `M5Dial.Touch.getDetail()`: `isPressed()`, `x`, `y`            |
| `HAL::_encoder_button_pressed_callback(nullptr, hal)` | `M5Dial.Speaker.tone(2000, 20)`                                     |
| `while (tp.isTouched())` release loop                 | `while (M5Dial.Touch.getDetail().isPressed()) { M5Dial.update(); ... }` |
| `millis()` (esp_timer macro)                          | Arduino `millis()`                                                  |
| `_log` / `_log_e` / `_log_mem()` (`heap_caps_get_free_size`) | `M5_LOGI` / `M5_LOGE` with the `launcher:` tag, same heap call |
| `app->setUserData(hal)`                               | `app->setUserData(ctx)` (AppContext*)                               |

### Launcher: behaviour notes

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
  detent is re-based, so detents the app consumed do not move the launcher.
- **Not ported yet:** every factory entry logs `app: <name> not ported`, plays the open/close
  animation and returns (`launcher: ready`). Names: `launcher_layout::appName`.
- **Icon byte order:** the icon headers are byte-swapped RGB565, as upstream. M5GFX, like the
  upstream LovyanGFX, reads `pushImage(..., const uint16_t*)` as `swap565_t` while `swapBytes` is
  false (the default; `LGFXBase::create_pc_fast`). The port calls `pushImage` the same way and
  never calls `setSwapBytes`, so the colours match the original.

### UserDemo factory-test entry

As upstream `main.cpp`: if `ENABLE_FACTORY_TEST` (config.h, default 1) is set and the button is
held for more than 1000 ms at boot, polled every 10 ms with `M5Dial.update()`, the sketch plays
4 kHz/30 ms twice (50 ms apart). Until Task 15 it then logs `factory: not ported` and continues
with the launcher. Upstream ran the factory test forever instead.

### Board options: 8 MB flash, `huge_app` (Task 6 fix round 1)

`tools/deps.txt` sets `M5DIAL_FQBN_OPTIONS=CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=huge_app`.
The M5Dial has 8 MB flash. The board default is a 4 MB layout with a 1.25 MB app partition, and
UserDemo already used 88% of it (`lgfx_efont_cn_24` alone is 550 KB), before the Wi-Fi and BLE
apps are added. `huge_app` gives a 3 MB app and no OTA; the original firmware also used a 4 MB
factory app without OTA. The flash backup and restore tools always handle the whole 8 MB chip,
whatever the partition table.

### Optional secrets.h

`config.h` includes `secrets.h` only `#if __has_include("secrets.h")`, and otherwise defines
placeholder `FACTORY_TEST_WIFI_SSID` / `FACTORY_TEST_WIFI_PASSWORD`. `M5DD_HAVE_SECRETS` is 1 or 0
to say which case applied. A fresh clone and CI therefore build UserDemo without the file.

The lookup works from a library header because arduino-cli passes `-I<sketch dir>` first, for
the sketch and for every library source alike (checked in a `--verbose` build). So every
translation unit of one build sees the same answer. Sketches without a `secrets.h` (the
examples) and the host tests (`-Isrc` only) get the placeholders.

## Small apps, Tasks 7-10

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

Every app logs `app: open <Class>` in `onCreate()` and `app: close <Class>` in `onDestroy()`,
with `<Class>` = `setAppName()` = the C++ class name.

### LCD test (`LCD_Test`, Task 7)

| Original (`_data.hal->...`)                              | Port                                                          |
|----------------------------------------------------------|---------------------------------------------------------------|
| `HAL::HAL*` user data                                    | `AppContext*` (`getUserData()`)                               |
| `encoder.wasMoved(true)` + `getDirection() < 1` → next, `delay(100)` | `m5dd::readDetentsWithClick(_detent)` → `goPages(steps)` (`lcd_test_pages::step`), no delay |
| `!encoder.btn.read()` + `while (!btn.read()) delay(5)`   | `m5dd::buttonClicked()`                                       |
| `tp.isTouched()` + `tp.update()` + `getTouchPointBuffer()` | `M5Dial.Touch.getDetail()`: `isPressed()`, `x`, `y`        |
| `while (tp.isTouched()) delay(5)` in `onCreate`          | same loop on `getDetail().isPressed()` with `M5Dial.update()` |
| `_log("onCreate")`, `printf("render page: %d")`          | `app: open LCD_Test`, `lcd: pattern <n>` (on open and every change) |

- `goNextPage`/`goLastPage` became `goPages(detents)`; the wrap logic is in `lcd_test_pages.h`
  (host-tested). Several detents in one frame each move one page.
- The original `delay(100)` after a page change throttled the per-count `wasMoved()`. The
  detent tracker already gives one step per click, and the interrupt-driven counter loses
  nothing while the app draws, so the delay is gone.

### RTC test (`RTC_Test`, Task 8)

| Original (`_data.hal->...`)                              | Port                                                          |
|----------------------------------------------------------|---------------------------------------------------------------|
| `HAL::HAL*` user data, `tm rtc_time`                     | `AppContext*`; `m5::rtc_datetime_t` local                     |
| `rtc.getTime(tm&)`                                       | `M5Dial.Rtc.getDateTime(&dt)` (`rtc: read failed` warning if it fails) |
| `(millis() - time_count) > 1000`                         | `rtc_format::due(millis(), time_count, 1000)` (host-tested)   |
| `_log("%d:%d:%d")`                                       | `rtc: YYYY-MM-DD HH:MM:SS` (`rtc_format::formatDateTime`, DialProbe's form) |
| `snprintf("%02d:%02d:%02d")` in the GUI                  | `rtc_format::formatClock`, same format                        |
| `!encoder.btn.read()` + hold loop                        | `m5dd::buttonClicked()`                                       |

- The refresh time is stored before the read (upstream: after drawing), so a failing RTC logs one
  warning per second, not one per frame.

### Brightness (`Set_Brightness`, Task 9)

| Original (`_data.hal->...`)                              | Port                                                          |
|----------------------------------------------------------|---------------------------------------------------------------|
| `HAL::HAL*` user data                                    | `AppContext*` (`getUserData()`)                               |
| `display.getBrightness()` / `setBrightness()`            | `M5Dial.Display.getBrightness()` / `setBrightness()`          |
| `encoder.wasMoved(true)` + `getDirection() < 1` → brighter | `m5dd::readDetentsWithClick(_detent)`: steps > 0 brighter            |
| `increment = 10 - delta_time / 8` (min 1), clamp 0..0xFF | `brightness_step::apply(value, steps, ms)` (host-tested)      |
| `LVGL::Anim_Path` ease-out 600 ms                        | same (`smooth_menu/lv_anim`)                                  |
| `!encoder.btn.read()` + hold loop                        | `m5dd::buttonClicked()`                                       |
| `_log("get brightness")`, `_log("set to: %d")`           | `brightness: <n>` on open and every change                    |

- Several detents in one frame each count (`steps * increment`); upstream moved once per
  `wasMoved()`.
- Upstream wrote the backlight only while the 600 ms animation ran, so its last write could stop
  short of the target. The port writes whenever the animated value changes; `getValue()` returns
  the end value once the animation is over, so the backlight lands exactly on the target.
- As upstream, the value is kept when the app quits (not restored).
