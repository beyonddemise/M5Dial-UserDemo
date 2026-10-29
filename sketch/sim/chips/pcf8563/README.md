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
- Invalid BCD values or dates are not rejected; they are normalised through the calendar arithmetic.
- The clock follows simulation time, which may run slower or faster than wall time.

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
name = 'pcf8563'
binary = 'path/to/dist/chip.wasm'
```

Then add a part of type `chip-pcf8563` in `diagram.json`.

`wokwi-api.h` is the unmodified Wokwi Chips API header (from wokwi.com/api/chips/wokwi-api.h).

## License

MIT. See [LICENSE](LICENSE).
