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
   `m5dd_sim_route_panel_miso()` in `examples/Smoke/sim_shims.h` runs before `M5Dial.begin()`.
   It calls `esp_rom_gpio_connect_in_signal(GPIO_NUM_5, FSPIQ_IN_IDX, false)`. Task 4 moves it into `m5dd::begin`.
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
