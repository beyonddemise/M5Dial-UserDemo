# Factory test and IO jig

The UserDemo sketch has the original firmware's factory test (`src/apps/factory_test`, port of
`main/apps/app_factory_test`). It checks the LCD, touch and knob, the RTC, the RFID reader and the
two Grove ports. After each step the operator gives a verdict (PASS, FAIL or SKIP). At the end a
summary is shown and logged, and the RTC wake-up test powers the Dial off.
Deviations from the original are in `PORTING_NOTES.md`, "Factory test".

## Jig wiring

| M5Dial port      | Pin        | Connect to                                                   |
|------------------|------------|--------------------------------------------------------------|
| Port B (G1/G2)   | **GPIO2**  | WS2812 / SK6812 ring or strip **DIN**, 14 LEDs (GRB, 800 kHz) |
| Port B           | 5V, GND    | LED ring 5 V and GND                                          |
| Port B (G1/G2)   | **GPIO1**  | Potentiometer **wiper**, 0-3.3 V. Ends to 3V3 and GND, never 5 V |
| Port A (G13/G15) | SDA G13, SCL G15, 5V, GND | Any I2C device, e.g. an M5 Unit. It shows up by address. |

GPIO2 drives DIN at 3.3 V. A WS2812B powered at 5 V may need DIN ≥ 0.7 × VDD (3.5 V), so it
can stay dark or flicker. Use a ring rated for 3.3 V data input, or put a 3.3 V → 5 V level shifter (e.g. 74AHCT125) between GPIO2 and DIN.

The virtual M5Dial (`sim/diagram.json`) has the same jig: `ring` (wokwi-led-ring, 14 pixels) on
GPIO2 and `pot` (wokwi-potentiometer) on GPIO1. Port A is not connected, so it shows `无` (none).

## Entering the test

1. Hold the front button while powering on (or resetting) the Dial. Keep holding.
2. After more than 1 s you hear a double beep (4 kHz). The intro screen "[M5Dial测试]" shows the
   firmware version and build date.
3. Release the button. The test starts.

Without the hold, the normal launcher starts. The check is enabled by `ENABLE_FACTORY_TEST`
in `config.h` (on by default). Build with `-DENABLE_FACTORY_TEST=0` to remove it.

## Steps

Each step runs until you **click** the button. Then the verdict screen appears.

| Step | What to check                                                                 |
|------|-------------------------------------------------------------------------------|
| LCD  | Turn the knob through the colour pages: no dead pixels, and the knob steps once per detent. Touch the screen: dots follow your finger. |
| RTC  | The time read-out counts up every second.                                     |
| RFID | Hold a card above the screen: its UID is shown and you hear a beep. If no reader is found the step does not run (`factory: rfid missing`) and the verdict starts on FAIL. |
| IO   | Turn the pot. At 0 V the whole ring is lit (level 6). Towards 3.3 V it empties down to the two end LEDs (level 0). `[PORT-B]` shows the reading in mV and the level. `[PORT-A]` lists the I2C addresses found (rescanned every 800 ms). **Without the jig, choose SKIP:** the step cannot tell whether the jig is there, and an open GPIO1 reads noise, so a PASS would mean nothing. |

## Verdict controls

The screen shows `<STEP>?` and three options: **PASS** (green), **FAIL** (red), **SKIP** (orange).

- **Knob:** clockwise moves the highlight down (next), counter-clockwise moves it up. It stops at
  PASS and at SKIP; it does not wrap.
- **Button:** a click confirms the highlighted option.
- The highlight starts on PASS. For RFID without a reader it starts on FAIL.

## Summary and RTC wake-up

After the IO verdict, the summary lists every step (`LCD: PASS`, ..., `IO: SKIP`). Below the list
it shows **ALL OK** (green) when every step is PASS or SKIP, otherwise **CHECK FAIL** (red). The
serial log gets one line:

```
factory: LCD=PASS RTC=PASS RFID=FAIL IO=SKIP
```

Remove USB power (run on the battery), then click. The RTC alarm is set to 4 s and the Dial
powers off. A few seconds later the RTC switches it back on, which proves the wake-up path. On
USB power the Dial cannot cut its own supply; it stays in deep sleep until reset.

## Serial log (115200)

| Line                                              | When                               |
|---------------------------------------------------|------------------------------------|
| `app: open FactoryTest`                           | test entered                       |
| `factory: step LCD` / `RTC` / `RFID` / `IO`       | each step starts                   |
| `factory: rfid missing`                           | no reader (RFID step not run)      |
| `factory: io adc=<mV> leds=<0..6> i2c=[<0xAA ..>]` | IO step, after every Port A scan  |
| `factory: prompt <STEP>`                          | verdict screen shown               |
| `factory: verdict <STEP>=<PASS\|FAIL\|SKIP>`      | verdict confirmed                  |
| `factory: LCD=.. RTC=.. RFID=.. IO=..`            | summary                            |
| `factory: rtc wakeup in 4 s`                      | just before the power-off          |

The embedded apps log their own lines as well (`lcd: pattern <n>`, `rtc: <time>`, `rfid: uid=..`).
