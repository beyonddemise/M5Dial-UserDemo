# M5Stack Dial - Wokwi board definition (draft)

A [wokwi-boards](https://github.com/wokwi/wokwi-boards) definition of the M5Stack Dial: the
StampS3 (ESP32-S3, 8 MB flash) with its round display, touch panel, RTC, RFID reader and
rotary encoder built in, so a Wokwi project needs one part instead of the part-by-part
assembly in [`../diagram.json`](../diagram.json).

**Status: draft, not yet loaded in Wokwi.** It follows the format of merged boards
(`boards/st7789` for custom chips and displays, `boards/m5stack-core-s3` and
`boards/esp32-s3-box-3` for an ESP32-S3 with internal nets), but nobody has opened it with
"Load custom board file..." yet. It will be offered to wokwi-boards only after its chips are
released and a Wokwi scenario passes (see [`../CONTRIBUTING-WOKWI.md`](../CONTRIBUTING-WOKWI.md)).

## Files

| File | Content |
|------|---------|
| `board.json` | name, MCU, the five chips (`github:beyonddemise/chip-<part>@0.1.0`), pins, internal nets, display |
| `board.svg` | original, neutral drawing, 45 x 52 mm (1 user unit = 1 mm): body, knob ring, round display window, connector pads |

## Pin map

The wiring is the reference assembly [`../diagram.json`](../diagram.json);
`sketch/tools/tests/test_wokwi_contrib.py` fails if the two drift apart.

| Function | GPIO | In `board.json` |
|----------|------|-----------------|
| Display GC9A01 (SPI): MOSI/SDA, SCK, DC, CS, RST, BL | 5, 6, 4, 7, 8, 9 | internal, chip `lcd` |
| Internal I2C: SDA, SCL (touch `0x38`, RTC `0x51`, RFID `0x28`) | 11, 12 | internal, chips `touch`, `rtc`, `rfid` |
| Touch interrupt | 14 | internal, `touch:INT` |
| Encoder A, B | 41, 40 | internal, chip `enc` |
| BtnA (active low) | 42 | pin `G42` |
| Buzzer | 3 | pin `G3` |
| Port A (I2C): SCL, SDA | 15, 13 | pins `G15`, `G13` (+ `5V.1`, `GND.1`) |
| Port B: | 1, 2 | pins `G1`, `G2` (+ `5V.2`, `GND.2`) |

## Known gaps and guesses

- **Button and buzzer are not built in.** A board holds custom chips only, and there is no chip
  for a push button or a piezo here. Wire a `wokwi-pushbutton` (to `GND`, 10 kOhm pull-up to
  `3V3`) to `G42` and a `wokwi-buzzer` to `G3`, as `../diagram.json` does.
- **RTC interrupt and power hold are not modelled.** On the real Dial the PCF8563 `INT` drives
  the power-on circuit and GPIO46 holds the power; a simulated board is always on.
- **Square display.** Wokwi displays are rectangles; the 240 x 240 frame fills the square
  around the round window, so its corners show in the simulator.
- **Format guesses, to check when first loaded:**
  - per-chip `attrs` in `chips[]` (used for the GC9A01's `readDummyBits: "0"`, which M5GFX needs
    on Wokwi's ESP32-S3) - no merged board uses it;
  - several chip pins plus `power(3.3)` / `GND` in one internal net (`$3v3`, `$gnd`);
  - `fqbn` `esp32:esp32:m5stack_stamp_s3` (the Espressif core's StampS3 entry; the Dial
    sketches in this repo build with M5Stack's own core, `m5stack:esp32:m5stack_dial`).
- The touch panel is a custom chip, not a display `touch` entry, so a tap is a `set-control`
  on `touch` (`x`, `y`, `touched`), not a mouse click on the screen.

## Try it locally

As the wokwi-boards README describes: in a new wokwi.com project, click into the code editor,
press F1, choose "Load custom board file..." and select this folder. Then use
`{ "type": "wokwi-custom-board", "id": "board" }` as the only part in `diagram.json`. The
chips load from their GitHub releases, so they must be published first.

## License

MIT, like the chips. The drawing is original work; it copies no M5Stack artwork or logo.
