# GC9A01 - Wokwi custom chip

A 240x240 round TFT controller (GalaxyCore GC9A01) for the [Wokwi](https://wokwi.com)
simulator. It accepts the serial command stream of the real controller, keeps a
240x240 RGB565 frame memory and shows it on the part's display. It also answers
the ID reads (`RDDID` etc.) that drivers such as M5GFX/LovyanGFX use to autodetect
the panel.

## Pins

| Pin   | Dir    | Function                                                   |
|-------|--------|------------------------------------------------------------|
| `VCC` | power  | not modelled                                               |
| `GND` | power  | not modelled                                               |
| `CS`  | in     | chip select, active low (internal pull-up)                 |
| `DC`  | in     | data/command: low = command byte, high = parameter / pixel |
| `SCL` | in     | serial clock, SPI mode 0                                   |
| `SDA` | in/out | serial data; the chip drives it only while answering a read |
| `RST` | in     | hardware reset, active low (internal pull-up)              |
| `BL`  | in     | backlight; accepted for wiring, not modelled               |

`SDA` is the controller's single bidirectional data line. Wire it to the MCU's MOSI.
If your MCU reads on a separate MISO pin, wire that to `SDA` as well.

## Controls

None. The chip has no interactive controls in the simulator UI. Configure it with the attributes below.

## Interface behaviour

- **CS** low starts reception and CS high ends any transfer or read. Command/parameter
  state persists across CS cycles, as on the real part.
- **DC** is sampled per byte: bytes received while DC is low are commands, while high are
  parameters/pixels. Parameters belong to the most recent command; any new command
  cancels a pending, not yet clocked-out read.
- **Reads:** after a read command (`04h`, `09h`-`0Ch`, `DAh`-`DCh`), the next DC edge with CS
  low turns `SDA` into an output. The chip then shifts `readDummyBits` dummy bits followed by the
  reply, MSB first, on the falling edges of `SCL`. Bits after the reply read as 0. The reply
  ends, and `SDA` is released, when CS goes high or DC goes low (the next command within the
  same CS assertion).
- **RST** low resets the registers to their defaults. Frame memory is kept, as on hardware,
  where it is undefined. RST high resumes reception if CS is still low.

## Attributes

| Attribute       | Default | Meaning |
|-----------------|---------|---------|
| `ips`           | `1`     | The glass inverts colours, as the common IPS GC9A01 modules do: true colours with `INVON`. Set `0` for true colours with `INVOFF`. |
| `bgr`           | `1`     | The glass is BGR-ordered: true colours with `MADCTL.BGR = 1`. |
| `readDummyBits` | `1`     | Dummy clocks before read data. The datasheet value is 1. Set `0` for MCU models that do not output the dummy clock (see Limitations). |
| `frameLog`      | `0`     | After each `RAMWR` burst, print `gc9a01: frame fnv=<hash> uniform=<RRGGBB or none>` as chip output. Automation scenarios can check it with `expect-chip-output`. |
| `debug`         | `0`     | Print every command byte and read transaction. |

## Supported commands

- `NOP` 00h, `SWRESET` 01h, `SLPIN` 10h, `SLPOUT` 11h, `INVOFF` 20h, `INVON` 21h,
  `DISPOFF` 28h, `DISPON` 29h
- `CASET` 2Ah, `RASET` 2Bh, `RAMWR` 2Ch, `RAMWRC` 3Ch (continue write)
- `MADCTL` 36h (MY, MX, MV, BGR), `COLMOD` 3Ah (stored; pixels are always RGB565)
- Reads: `RDDID` 04h (`00h 9Ah 01h`), `RDDST` 09h (approximate), `RDDPM` 0Ah,
  `RDDMADCTL` 0Bh, `RDDCOLMOD` 0Ch, `RDID1/2/3` DAh/DBh/DCh
- Any other command, such as the vendor init sequence or gamma and power settings,
  is accepted and ignored.

## Limitations

- Only 16-bit RGB565 pixels. 12-bit and 18-bit `COLMOD` modes are not modelled.
- No frame-memory reads (`RAMRD`), no scrolling, no partial or idle mode, no tearing-effect output.
- `MADCTL` rotation is applied as "exchange (MV), then mirror (MX/MY)". It has been
  checked for rotation 0 only.
- The backlight is not modelled. The panel is always lit while `DISPON` is set and the panel is out of sleep.
- Writes go through the Wokwi SPI device API. Reads are driven bit by bit on `SDA`:
  the chip shifts on the falling edge of `SCL` and the host samples on the rising edge.
- Wokwi's ESP32-S3 sends no clock edge for the "1 dummy bit" trick some drivers use,
  where M5GFX flips `SPI_CK_IDLE_EDGE`. Use `readDummyBits: 0` with such drivers on
  that MCU. The real panel needs 1.
- wokwi-cli's `take-screenshot` reports "Part does not have a valid framebuffer" for this
  chip's display, so use `frameLog` in automated checks.

## Use it in Wokwi

Each release of this chip (a tag `vX.Y.Z` with the asset `chip.zip`, holding exactly
`chip.json` and `chip.wasm`) can be loaded by any Wokwi project. Add the dependency and a part to
the project's `diagram.json`:

```json
"dependencies": {
  "chip-gc9a01": "github:beyonddemise/chip-gc9a01@0.1.0"
}
```

```json
{ "type": "chip-gc9a01", "id": "lcd", "top": 0, "left": 200, "attrs": {} }
```

[`example/diagram.json`](example/diagram.json) is a complete minimal project: a stock
ESP32-S3-DevKitC-1 and this chip, nothing else.
The example wires the panel to the ESP32-S3's default SPI pins (MOSI 11, SCK 12, CS 10)
plus DC 9, RST 8 and BL 7. With M5GFX/LovyanGFX on the ESP32-S3 add `"readDummyBits": "0"`
to the part's `attrs` (see Limitations).

**Status:** `0.1.0` is a pre-release. The host unit test (`make test`) passes. The chip is part of the
wokwi-cli automation scenarios of the [M5Dial demo](https://github.com/beyonddemise/M5Dial-UserDemo),
but it has not yet been seen passing a Wokwi simulation: those scenarios are held up by
[issue #39](https://github.com/beyonddemise/M5Dial-UserDemo/issues/39). Reports welcome.

## Build

Needs [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) (tested with 34).

```sh
make WASI_SDK_PATH=/path/to/wasi-sdk        # dist/chip.wasm + dist/chip.json
make WASI_SDK_PATH=/path/to/wasi-sdk zip    # dist/chip.zip (release payload)
make test                                   # host unit test (native cc)
```

To try a local build, point `wokwi.toml` at it and add a part of type `chip-gc9a01` to
`diagram.json`:

```toml
[[chip]]
name = 'gc9a01'
binary = 'path/to/dist/chip.wasm'
```

`wokwi-api.h` is the unmodified Wokwi Chips API header (from wokwi.com/api/chips/wokwi-api.h).

## License

MIT. See [LICENSE](LICENSE).
