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

## Use it in Wokwi

Each release of this chip (a tag `vX.Y.Z` with the asset `chip.zip`, holding exactly
`chip.json` and `chip.wasm`) can be loaded by any Wokwi project. Add the dependency and a part to
the project's `diagram.json`:

```json
"dependencies": {
  "chip-encoder": "github:beyonddemise/chip-encoder@0.1.0"
}
```

```json
{ "type": "chip-encoder", "id": "enc", "top": 0, "left": 200, "attrs": {} }
```

[`example/diagram.json`](example/diagram.json) is a complete minimal project: a stock
ESP32-S3-DevKitC-1 and this chip, nothing else.
The example wires `A` to GPIO4 and `B` to GPIO5.

**Status:** `0.1.0` is a pre-release. The host unit test (`make test`) passes. The chip is part of the
wokwi-cli automation scenarios of the [M5Dial demo](https://github.com/beyonddemise/M5Dial-UserDemo),
and those scenarios pass on Wokwi
(since [PR #50](https://github.com/beyonddemise/M5Dial-UserDemo/pull/50)). Reports welcome.

## Build

Needs [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) (tested with 34).

```sh
make WASI_SDK_PATH=/path/to/wasi-sdk        # dist/chip.wasm + dist/chip.json
make WASI_SDK_PATH=/path/to/wasi-sdk zip    # dist/chip.zip (release payload)
make test                                   # host unit test (native cc)
```

To try a local build, point `wokwi.toml` at it and add a part of type `chip-encoder` to
`diagram.json`:

```toml
[[chip]]
name = 'encoder'
binary = 'path/to/dist/chip.wasm'
```

`wokwi-api.h` is the unmodified Wokwi Chips API header (from wokwi.com/api/chips/wokwi-api.h).

## License

MIT. See [LICENSE](LICENSE).
