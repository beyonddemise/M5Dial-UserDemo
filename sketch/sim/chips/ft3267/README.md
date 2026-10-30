# FT3267 - Wokwi custom chip

A self-capacitive touch panel controller (FocalTech FT3267, register-compatible with the
FT6x36 family) for the [Wokwi](https://wokwi.com) simulator. It implements the I2C register
map that FocalTech drivers (e.g. LovyanGFX/M5GFX `Touch_FT5x06`, Adafruit FT6206) use: one touch
point, the identification registers and the `INT` output. The touch comes from the part's
controls, so automation scenarios can tap the panel with `set-control`.

## Pins

| Pin   | Dir    | Function                                                           |
|-------|--------|--------------------------------------------------------------------|
| `SCL` | in     | I2C clock (address `0x38`)                                         |
| `SDA` | in/out | I2C data                                                           |
| `INT` | out    | low while touched, released otherwise (host enables a pull-up); a host pulling it low wakes the part from hibernate |
| `RST` | in     | reset, active low (internal pull-up)                               |
| `VCC` | power  | not modelled                                                       |
| `GND` | power  | not modelled                                                       |

## Controls

| Control   | Range   | Meaning |
|-----------|---------|---------|
| `x`       | 0-4095  | touch X coordinate (raw 12-bit value, as reported in `P1_XH/XL`) |
| `y`       | 0-4095  | touch Y coordinate |
| `touched` | 0/1     | finger on the panel |

Use the panel's own resolution (e.g. 0-239 on a 240x240 panel such as the M5Stack Dial's).
Scenario example: set `x` and `y`, then `touched` to 1; set `touched` to 0 to lift.

## Attributes

| Attribute    | Default | Meaning |
|--------------|---------|---------|
| `chipId`     | `0x33`  | `CIPHER` (A3h) value |
| `vendorId`   | `0x11`  | `FOCALTECH_ID` (A8h) value (FocalTech); drivers treat 0 as "no panel" |
| `firmwareId` | `0x01`  | `FIRMID` (A6h) value |
| `debug`      | `0`     | Print touch / release / power-mode events |

## Registers

| Reg | Name | Model |
|-----|------|-------|
| `00h` | DEV_MODE | stored |
| `01h` | GEST_ID | always 0 (no gestures) |
| `02h` | TD_STATUS | number of points, 0 or 1 |
| `03h`-`08h` | P1_XH .. P1_MISC | event flag in `XH[7:6]`: press-down (00) on the first report of a touch, contact (10) after; lift-up (01) after release. Touch id 0 in `YH[7:4]`. 12-bit coordinates |
| `09h`-`0Eh` | P2 | always FFh (second point not reported) |
| `A1h`/`A2h` | LIB_VER | 3003h |
| `A3h` | CIPHER | attribute `chipId`, read-only |
| `A4h` | G_MODE | stored (reset 01h) |
| `A5h` | PWR_MODE | `03h` = hibernate: the part NACKs I2C until `RST` or a low pulse on `INT` |
| `A6h` | FIRMID | attribute `firmwareId`, read-only |
| `A8h` | FOCALTECH_ID | attribute `vendorId`, read-only |
| others | | stored, read back as written |

## Interface behaviour

- The register pointer auto-increments, so the usual "write 02h, read 1 + 6 x points bytes" works.
- The touch report is latched at the start of each read that begins at or below `08h`.
- The controls are polled every 5 ms (the report rate); `INT` follows `touched`.

## Limitations

- Single touch only; no gestures, no weight/area data beyond fixed values.
- `G_MODE` is stored, but `INT` behaves as in polling mode (low while touched) in both modes;
  the trigger-mode pulse train is not modelled.
- The default `CIPHER` value for the FT3267 is taken from community reports; set `chipId` if your driver checks it.
- Coordinates are passed through unchanged; panel resolution/rotation is the driver's business.

## Build

Needs [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) (tested with 34).

```sh
make WASI_SDK_PATH=/path/to/wasi-sdk        # dist/chip.wasm + dist/chip.json
make WASI_SDK_PATH=/path/to/wasi-sdk zip    # dist/chip.zip (release payload)
make test                                   # host unit test (native cc)
```

Use it in `wokwi.toml`:

```toml
[[chip]]
name = 'ft3267'
binary = 'path/to/dist/chip.wasm'
```

Then add a part of type `chip-ft3267` in `diagram.json`.

`wokwi-api.h` is the unmodified Wokwi Chips API header (from wokwi.com/api/chips/wokwi-api.h).

## License

MIT. See [LICENSE](LICENSE).
