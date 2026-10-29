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
multiplies by it, so clockwise logs e.g. `enc: steps=1 total=1 raw=-1` (EncoderDetent floors: the step
comes on the first edge of a clockwise detent). The encoder steps of `probe.test.yaml` are the check in the simulator; which
physical direction is "A leads" on the real knob is confirmed in Task 19's physical check.

### DialProbe

`examples/DialProbe` replaces Task 1/2's Smoke sketch; Smoke's `board: 12` check became `init: board 12`.
It logs `init: board <n>` (12 = M5Dial, i.e. the panel was autodetected), `init: display WxH`, `init: rtc|touch|rfid ok|fail` (RTC: `Rtc.isEnabled()`; touch: VENDID
`A8h` non-zero; RFID: VersionReg not 00h/FFh), `probe: ready`, then `enc:`, `btn:`, `touch:`,
`rtc:` (1 Hz) and `rfid: uid=` (card halted after each read, so it is logged once per presentation).
Since Task 4 it starts with `m5dd::begin("DialProbe", canvas)` (logs `boot: DialProbe`), counts the
encoder with the interrupt-driven `dialEncoder` + `EncoderDetent`, logs `enc: steps=<n> total=<sum> raw=<n>`
and pushes its full canvas every loop as deliberate load. The "encoder fast" steps of `probe.test.yaml`
turn 20 detents at 0.5 ms per edge under that load and check the totals (`total=20`, then
`total=-20 raw=80`). The slow encoder steps keep `edgeIntervalUs = 40000` (direction/detent check only).

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
