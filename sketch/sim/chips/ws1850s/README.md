# WS1850S - Wokwi custom chip

A 13.56 MHz RFID reader (WS1850S, register-compatible with the NXP MFRC522) on I2C, plus one
ISO/IEC 14443-3 type A card in its field, for the [Wokwi](https://wokwi.com) simulator.
MFRC522 drivers (e.g. the Arduino "MFRC522" libraries, M5Stack's I2C variant) can detect the
reader, poll for a card and read its UID. The card is placed, removed and given a UID with the
part's controls, so automation scenarios can present cards with `set-control`.

## Pins

| Pin   | Dir    | Function                                                           |
|-------|--------|--------------------------------------------------------------------|
| `SCL` | in     | I2C clock (attribute `address`, default `0x28`)                    |
| `SDA` | in/out | I2C data                                                           |
| `IRQ` | out    | interrupt request; polarity per `ComIEnReg.IRqInv`, open drain unless `DivIEnReg.IRQPushPull` |
| `RST` | in     | NRSTPD: low = power-down (I2C NACK), rising edge = hard reset (internal pull-up; tie high if unused) |
| `VCC` | power  | not modelled                                                       |
| `GND` | power  | not modelled                                                       |

## Controls

| Control        | Range | Meaning |
|----------------|-------|---------|
| `cardPresent`  | 0/1   | a card is in the field |
| `uid0`..`uid3` | 0-255 | the card's 4-byte UID (single size, cascade level 1) |

Removing the card (or changing its UID) resets it to IDLE, as when a real card leaves the field.

## Attributes

| Attribute | Default  | Meaning |
|-----------|----------|---------|
| `address` | `0x28`   | I2C address (set by the EA/ADR pins on the real part) |
| `version` | `0x15`   | `VersionReg` (37h) value. WS1850S modules have been reported to read `0x15`; use `0x91`/`0x92` to look like an NXP MFRC522 v1/v2 |
| `sak`     | `0x08`   | SAK returned by SELECT (08h = MIFARE Classic 1K) |
| `atqa`    | `0x0004` | ATQA returned by REQA/WUPA (sent low byte first) |
| `debug`   | `0`      | Print every frame sent to the card and card state changes |

## Supported reader commands (CommandReg)

- `Idle`, `NoCmdChange`, `SoftReset` (all registers to reset values), `Mem` (25-byte buffer), `GenerateRandomID` (no-op, completes)
- `CalcCRC`: CRC_A of the FIFO content with the `ModeReg.CRCPreset` value into `CRCResultReg`, sets `DivIrqReg.CRCIRq`
- `Transmit`, `Transceive` (+ `BitFramingReg.StartSend`, `TxLastBits`): the FIFO is sent to the card; its answer
  is placed in the FIFO with `ControlReg.RxLastBits`, `ComIrqReg.RxIRq` set. No answer: with `TModeReg.TAuto` the timer
  starts and `TimerIRq` is raised after `(TReload + 1) * (2 * TPrescaler + 1) / 13.56 MHz`
- `TxModeReg.TxCRCEn` / `RxModeReg.RxCRCEn` (hardware CRC append / check-and-strip)
- `ComIrqReg`/`DivIrqReg` Set1/Set2 semantics, `FIFOLevelReg.FlushBuffer`, `ControlReg.TStartNow/TStopNow`,
  `TModeReg.TAutoRestart`, `TxControlReg` antenna enable (no answers with the field off), live `Status1Reg`
- `MFAuthent`: not supported - completes with `ErrorReg.ProtocolErr`

## Card model (ISO/IEC 14443-3 type A)

| Frame | Accepted in | Answer | New state |
|-------|-------------|--------|-----------|
| REQA `26h` (7 bits) | IDLE | ATQA | READY |
| WUPA `52h` (7 bits) | IDLE, HALT | ATQA | READY |
| ANTICOLLISION `93h 20h` | READY | UID0-3 + BCC | READY |
| SELECT `93h 70h` UID BCC CRC_A | READY | SAK + CRC_A | ACTIVE |
| HLTA `50h 00h` CRC_A | ACTIVE | none | HALT |
| anything else | READY, ACTIVE | none | IDLE |

## Interface behaviour

- I2C: the first written byte selects the register; further bytes of the same transfer go to that
  same register (no auto-increment), so FIFO bursts work as on the real part.

## Limitations

- One card, 4-byte UID only (no cascade levels 2/3, no collisions).
- Anticollision with partially known UID bits (NVB other than `20h`) is not answered.
- No MIFARE Crypto1 authentication, no memory reads/writes, no ISO 14443-4.
- RF-level timing (frame delay times) is not modelled: answers are available immediately after `StartSend`.
- Analog/RF configuration registers (gain, modulation, thresholds) and the self-test are stored only.

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
name = 'ws1850s'
binary = 'path/to/dist/chip.wasm'
```

Then add a part of type `chip-ws1850s` in `diagram.json`.

`wokwi-api.h` is the unmodified Wokwi Chips API header (from wokwi.com/api/chips/wokwi-api.h).

## License

MIT. See [LICENSE](LICENSE).
