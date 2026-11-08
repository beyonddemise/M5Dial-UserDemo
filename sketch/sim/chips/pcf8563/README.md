# PCF8563 - Wokwi custom chip

A real-time clock / calendar (NXP PCF8563) for the [Wokwi](https://wokwi.com) simulator.
It implements the part's I2C register file: BCD time and date with the century flag,
the alarm, the countdown timer and the open-drain `INT` output. The clock runs on
simulation time and starts from the host's UTC wall clock, so firmware that reads the RTC
at boot sees a plausible "now".

## Pins

| Pin      | Dir    | Function                                                        |
|----------|--------|-----------------------------------------------------------------|
| `SCL`    | in     | I2C clock (fixed address `0x51`, as on the part)                |
| `SDA`    | in/out | I2C data                                                        |
| `INT`    | out    | interrupt, open drain, active low (needs a pull-up)             |
| `CLKOUT` | out    | accepted for wiring, not modelled                               |
| `VDD`    | power  | not modelled                                                    |
| `VSS`    | power  | not modelled                                                    |

## Controls

None.

## Attributes

| Attribute   | Default | Meaning |
|-------------|---------|---------|
| `startTime` | empty   | Initial clock value, `"YYYY-MM-DD HH:MM:SS"` (UTC). Empty: the host's current UTC time. Useful for deterministic tests. |
| `debug`     | `0`     | Print time writes, alarms and timer events. |

## Registers

| Reg | Name | Model |
|-----|------|-------|
| `00h` | Control_status_1 | `STOP` freezes the clock (resumes from the frozen value); `TEST1`/`TESTC` stored only |
| `01h` | Control_status_2 | `AIE`, `TIE`, `TI_TP` stored; `AF`/`TF` set by the alarm/timer and cleared only by writing 0 |
| `02h` | VL_seconds | seconds (BCD) + `VL` flag (0 at power-on; stored as written) |
| `03h`-`08h` | minutes .. years | BCD time/date; `06h` weekday is counted, not computed (the part does not validate it); `07h` bit 7 = century (set = 19xx, clear = 20xx), toggles when the years wrap 99 -> 00 |
| `09h`-`0Ch` | alarms | minute/hour/day/weekday; bit 7 `AE` = 1 disables the field. `AF` is set at second 00 of a minute when every enabled field matches |
| `0Dh` | CLKOUT_control | stored only |
| `0Eh` | Timer_control | `TE` + `TD[1:0]` (4096 Hz, 64 Hz, 1 Hz, 1/60 Hz) |
| `0Fh` | Timer | countdown value; on expiry `TF` is set and the countdown reloads |

## Interface behaviour

- The register pointer auto-increments and wraps from `0Fh` to `00h`.
- Time registers are latched at the start of each read transaction, so a burst read is consistent.
- Written time registers take effect at the I2C STOP (or a repeated START). Writing them
  resets the sub-second prescaler. A partial write (e.g. date only) keeps the other fields.
- `INT` is low while (`AF` and `AIE`) or (`TF` and `TIE`), released otherwise.

## Limitations

- `TI_TP` pulse mode is not modelled: `INT` always follows the flags (level mode).
- `CLKOUT` output and the `TEST1`/`TESTC` test modes are not modelled.
- Century bit: the model keeps the full year and shows it as `C = 1` for 1900-1999, `C = 0` for
  2000-2099 (the convention of M5Unified and most drivers). The datasheet leaves the meaning to
  software and only toggles `C` at each 99 -> 00 wrap; here it toggles at 1999 -> 2000, and a clock
  running past 2099 shows years 2100+ as `C = 0`, `00h`.. instead of toggling again.
- Invalid BCD values or dates are not rejected; they are normalised through the calendar arithmetic.
- The clock follows simulation time, which may run slower or faster than wall time.

## Use it in Wokwi

Each release of this chip (a tag `vX.Y.Z` with the asset `chip.zip`, holding exactly
`chip.json` and `chip.wasm`) can be loaded by any Wokwi project. Add the dependency and a part to
the project's `diagram.json`:

```json
"dependencies": {
  "chip-pcf8563": "github:beyonddemise/chip-pcf8563@0.1.0"
}
```

```json
{ "type": "chip-pcf8563", "id": "rtc", "top": 0, "left": 200, "attrs": {} }
```

[`example/diagram.json`](example/diagram.json) is a complete minimal project: a stock
ESP32-S3-DevKitC-1 and this chip, nothing else.
The example wires the clock to the ESP32-S3's default I2C pins (SDA 8, SCL 9) and `INT` to
GPIO10; `INT` is open drain, so enable the MCU's pull-up (`INPUT_PULLUP`).

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

To try a local build, point `wokwi.toml` at it and add a part of type `chip-pcf8563` to
`diagram.json`:

```toml
[[chip]]
name = 'pcf8563'
binary = 'path/to/dist/chip.wasm'
```

`wokwi-api.h` is the unmodified Wokwi Chips API header (from wokwi.com/api/chips/wokwi-api.h).

## License

MIT. See [LICENSE](LICENSE).
