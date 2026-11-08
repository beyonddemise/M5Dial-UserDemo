# M5Dial UserDemo - Arduino port

Arduino-sketch port of the ESP-IDF M5Dial UserDemo (`../main`). Everything lives under this
folder; the ESP-IDF project is untouched. The port calls the M5Dial API directly
(`M5Dial.Display`, `M5Dial.BtnA`, `M5Dial.Touch`, `M5Dial.Rtc`, ...); shared code is the
`libraries/M5DialDemo` library. Most sketches also have Wokwi scenarios for a virtual M5Dial (not
BleServer and MemSoak: Wokwi has no BLE); they pass on Wokwi since the sim build silences the
buzzer ([#39](https://github.com/beyonddemise/M5Dial-UserDemo/issues/39), PR #50). See
[PORTING_NOTES](docs/PORTING_NOTES.md#sim-fallback--dm5dd_sim) for the simulator's limits.

## Docs

| Doc | What's in it |
|---|---|
| [docs/SETUP.md](docs/SETUP.md) | Toolchain, first-time setup, build, upload, VSCode, Wokwi |
| [docs/PORTING_NOTES.md](docs/PORTING_NOTES.md) | Original → M5Dial mapping, known differences per app, sim fallback, memory, partitions, CI |
| [docs/TEST_CHECKLIST.md](docs/TEST_CHECKLIST.md) | Acceptance checks on the real device, per sketch |
| [docs/FACTORY_TEST_JIG.md](docs/FACTORY_TEST_JIG.md) | Factory test and its IO jig wiring |
| [tools/deps.txt](tools/deps.txt) | Every pinned version |

## Sketches

| Sketch | What it shows | Wokwi scenarios (`sim/scenarios/`) |
|---|---|---|
| [UserDemo](UserDemo) | The full demo: launcher ring with 8 apps, More menu with power-off; factory test when the button is held at boot | `UserDemo/launcher`, `more_menu_quit`, `power_off`, `enter`, `skip_io`, `io_leds` |
| [examples/DialProbe](examples/DialProbe) | Hardware probe: logs board, display, RTC, touch, RFID, button and encoder | `DialProbe/probe` |
| [examples/LcdTest](examples/LcdTest) | LCD test app: colour pages, touch dots | `LcdTest/lcd_test` |
| [examples/RtcTest](examples/RtcTest) | RTC test app: live clock | `RtcTest/rtc_test` |
| [examples/RfidTest](examples/RfidTest) | RFID test app: card UID | `RfidTest/card` |
| [examples/Brightness](examples/Brightness) | Backlight brightness app | `Brightness/brightness` |
| [examples/TempDemo](examples/TempDemo) | Thermostat demo app (upstream "VideoShit") | `TempDemo/temp_demo` |
| [examples/WifiScan](examples/WifiScan) | Wi-Fi scan app: AP list, touch rescans | `WifiScan/scan` |
| [examples/BleServer](examples/BleServer) | BLE heart-rate server app | none (Wokwi has no BLE); `tools/ble_check.py` on the device |
| [examples/MemSoak](examples/MemSoak) | Heap soak: Wi-Fi scan + BLE, three cycles | none (device only) |
| [examples/LvglDemo](examples/LvglDemo) | LVGL 9 starter: arc on the knob, button, touch | `LvglDemo/knob` |

## Everyday commands

From this folder, after [first-time setup](docs/SETUP.md):

```bash
tools/build.sh UserDemo
tools/upload.sh UserDemo          # needs the flash backup first (tools/backup_flash.sh)
tools/monitor.sh
# device_test.py uploads the sketch it tests: this replaces UserDemo with DialProbe
tools/.venv/bin/python tools/device_test.py examples/DialProbe --expect 'boot: DialProbe' 'init: rfid ok' 'init: display 240x240' 'init: rtc ok' 'init: touch ok' 'rtc: 20'
tools/sim.sh examples/DialProbe sim/scenarios/DialProbe/probe.test.yaml   # needs WOKWI_CLI_TOKEN
tools/test_host.sh                                    # C++ host tests of the library
tools/.venv/bin/python -m unittest discover tools/tests   # Python tool tests
```

**Safety:** run `tools/backup_flash.sh` once before the first upload; `upload.sh` refuses to run
without a verified backup in `backup/`.
