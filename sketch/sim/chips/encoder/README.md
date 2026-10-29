# Rotary encoder (scriptable) - Wokwi custom chip

An incremental rotary encoder with detents, like the common mechanical EC11/KY-040 type, for the
[Wokwi](https://wokwi.com) simulator. Unlike the stock `wokwi-ky-040` part it can be turned from
automation scenarios (`set-control`), by any number of detents and at any speed, which makes it
suitable for testing encoder decoding (lost edges, direction, detent size) in CI.

## Pins

| Pin   | Dir   | Function                                                       |
|-------|-------|----------------------------------------------------------------|
| `A`   | out   | channel A (KY-040 `CLK`); high at rest                         |
| `B`   | out   | channel B (KY-040 `DT`); high at rest                          |
| `VCC` | power | not modelled                                                   |
| `GND` | power | not modelled                                                   |

One detent is one full quadrature cycle (4 edges, x4 decoding counts 4). Clockwise: A falls
first, then B falls, A rises, B rises (as the KY-040: "CLK goes low first"). Counter-clockwise: B leads A.

## Controls

| Control          | Range        | Meaning |
|------------------|--------------|---------|
| `steps`          | -20..20      | Each change to a non-zero value n turns the knob by n detents (n > 0 clockwise). Set it back to 0 to repeat the same value. |
| `edgeIntervalUs` | 1..100000    | Time between two edges in microseconds (default 1000). Small values simulate fast turns. |

Scenario example:

```yaml
- set-control: { part-id: enc, control: steps, value: 1 }   # one detent clockwise
- set-control: { part-id: enc, control: steps, value: 0 }   # re-arm
- set-control: { part-id: enc, control: steps, value: -3 }  # three detents counter-clockwise
```

## Attributes

| Attribute | Default | Meaning |
|-----------|---------|---------|
| `debug`   | `0`     | Print each queued turn and completed detent. |

The controls can also be given initial values as attributes.

## Interface behaviour

- Turns are queued: a new value while a turn is in progress adds to it. A detent in progress
  always completes in its own direction before a reversal is emitted.
- The `steps` control is polled every 1 ms.

## Limitations

- Outputs are push-pull (the MCU's pull-ups are not needed); contact bounce and the push switch are not modelled.
- No half-step / quarter-step detent variants (always one full cycle per detent).

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
name = 'encoder'
binary = 'path/to/dist/chip.wasm'
```

Then add a part of type `chip-encoder` in `diagram.json`.

`wokwi-api.h` is the unmodified Wokwi Chips API header (from wokwi.com/api/chips/wokwi-api.h).

## License

MIT. See [LICENSE](LICENSE).
