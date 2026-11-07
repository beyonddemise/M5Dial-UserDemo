# Test checklist (Task 19: acceptance on the real M5Dial)

One section per sketch. Tick each box when the device does what the "Expect" part says. Record
results and any defect in issue #19; open a new issue per defect.

Background for every item: [PORTING_NOTES.md](PORTING_NOTES.md). Jig wiring:
[FACTORY_TEST_JIG.md](FACTORY_TEST_JIG.md). Setup: [SETUP.md](SETUP.md).

## Before you start

Run every command from the `sketch/` folder.

- [ ] **This** Dial has a backup of its own factory flash. `tools/check_backup.sh` only proves
  that some intact backup exists, not that it came from the connected Dial. **Do:** for a Dial
  that has never been flashed, run `tools/backup_flash.sh` before the first upload, then
  `tools/check_backup.sh` (**Expect:** exit 0). Write the Dial's MAC
  (`tools/.venv/bin/python -m esptool read_mac`) next to the new `.bin` in `backup/`. Only an image
  read before any upload is a factory backup; name a later read differently.
- [ ] The Dial is found. **Do:** `tools/.venv/bin/python tools/find_port.py`. **Expect:** it prints
  one port (VID:PID `303A:1001`), exit 0. If none: hold **G0** (the small button on the StampS3) while you plug in
  USB, then release (download mode).

`device_test.py` uploads the sketch (`upload.sh`), resets the Dial and waits for every `--expect`
regex in the serial log (default timeout 20 s). Add `--no-upload` to check a sketch that is
already flashed. A serial monitor for the manual steps: `tools/monitor.sh` (115200).

## DialProbe (`examples/DialProbe`)

- [ ] **Do:**
  `tools/.venv/bin/python tools/device_test.py examples/DialProbe --expect 'boot: DialProbe' 'init: rfid ok' 'init: board 12' 'init: display 240x240' 'init: rtc ok' 'init: touch ok' 'probe: ready' 'rtc: 20'`
  **Expect:** PASS.
- [ ] **Do:** with `tools/monitor.sh` open, turn the knob one detent **clockwise**.
  **Expect:** `enc: steps=1 total=1 raw=-3` or `raw=-4` (one step per detent, positive).
  This confirms `M5DD_ENCODER_CW_SIGN = -1` on the real knob.
- [ ] **Do:** one detent counter-clockwise. **Expect:** `enc: steps=-1`.
- [ ] **Do:** rest the knob. **Expect:** `enc: idle raw=<multiple of 4>` and no further `enc: steps`
  lines while it rests (no chatter).
- [ ] **Do:** right after a reset, turn exactly 20 detents clockwise as fast as you can, then rest.
  **Expect:** `enc: idle raw=-80 total=20` (no lost edges under the canvas load).
- [ ] **Do:** press and release the button. **Expect:** `btn: pressed`, then `btn: released`.
- [ ] **Do:** touch the screen. **Expect:** `touch: x=<n> y=<n>` lines with x/y in 0-239.
- [ ] **Do:** hold a card (bank-card size) over the screen. **Expect:** one `rfid: uid=..` line per
  presentation.

## UserDemo / launcher (`UserDemo`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py UserDemo --expect 'boot: UserDemo' 'init: rfid ok' 'launcher: ready'`
  **Expect:** PASS.
- [ ] **Do:** look at the screen. **Expect:** the icon ring looks like the original firmware
  (8 icons, icon colours as upstream, not colour-swapped).
- [ ] **Do:** turn the knob slowly clockwise. **Expect:** the selector moves to the **next** icon,
  one icon per detent, smoothly; 6 kHz click; `launcher: select <i> <tag>` with `<i>` rising.
- [ ] **Do:** counter-clockwise from slot 0. **Expect:** 7 kHz click, `launcher: select 9 -` (empty
  slot, as upstream).
- [ ] **Do:** tap an icon. **Expect:** it is selected (`launcher: select <i>`).
- [ ] **Do:** tap the centre circle, or click the button. **Expect:** the selected app opens with
  the open animation, `app: open <Class>`; a click inside the app closes it and the launcher
  shows `launcher: ready` and `launcher: free mem <n>`, no `launcher: memory leak`.
- [ ] **Do:** open and close each slot (0-6) once. **Expect:** each app behaves as in its own
  section below.
- [ ] **Do:** slot 7 (More) → turn to Quit → click. **Expect:** `more: selected Quit`,
  `app: close MoreMenu`, `launcher: ready`.
- [ ] **Do:** More → Power Off, **on battery** (USB unplugged). **Expect:** `power: off` (if a
  monitor is attached before unplugging, it may not show), the screen goes black and the Dial
  powers down. Press the WAKE button. **Expect:** it boots to the launcher.
- [ ] **Do:** More → Power Off, **on USB power.** **Expect:** `power: off`, the screen goes black,
  and after ~4 s the Dial reboots to the launcher (`boot: UserDemo`, a 4 s deep-sleep timer wake).
  The original kept running and returned to the menu (its HAL re-took the hold pin after 4 s).

## LcdTest (`examples/LcdTest`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/LcdTest --expect 'boot: LcdTest' 'app: open LCD_Test' 'lcd: pattern \d+'`
  **Expect:** PASS.
- [ ] **Do:** turn the knob through all colour pages. **Expect:** solid colours as the original, no
  dead pixels, one page per detent, `lcd: pattern <n>` per change, clockwise = next page.
- [ ] **Do:** touch the screen and drag. **Expect:** dots follow the finger.
- [ ] **Do:** click. **Expect:** `app: close LCD_Test`, `app: restart`, the sketch restarts.

## RtcTest (`examples/RtcTest`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/RtcTest --expect 'boot: RtcTest' 'app: open RTC_Test' 'rtc: 20\d\d-\d\d-\d\d \d\d:\d\d:\d\d'`
  **Expect:** PASS, and no `rtc: read failed`.
- [ ] **Do:** look at the clock. **Expect:** it shows the correct local time and counts up once a
  second, as the original.
- [ ] **Do:** click. **Expect:** `app: close RTC_Test`.

## RfidTest (`examples/RfidTest`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/RfidTest --expect 'boot: RfidTest' 'init: rfid ok' 'app: open RFID_Test' 'rfid: ready'`
  **Expect:** PASS.
- [ ] **Do:** hold a real card or tag over the screen. **Expect:** its UID under "UID"
  (`04:A1:B2:C3` style), a 4 kHz beep, one `rfid: uid=<HEX:..>` line.
- [ ] **Do:** remove the card and present it again. **Expect:** a second `rfid: uid=` line and beep.
- [ ] **Do:** click. **Expect:** `app: close RFID_Test`.

## Brightness (`examples/Brightness`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/Brightness --expect 'boot: Brightness' 'app: open Set_Brightness' 'brightness: \d+'`
  **Expect:** PASS.
- [ ] **Do:** turn clockwise, then counter-clockwise. **Expect:** the backlight visibly brightens,
  then dims; `brightness: <n>` follows (0..255); a fast spin takes bigger steps.
- [ ] **Deferred: quitting mid-animation.** **Do:** turn several detents and click at once, before the
  600 ms animation ends. **Expect:** the backlight keeps the selected (target) level after the
  app closes, not an intermediate one.

## TempDemo (`examples/TempDemo`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/TempDemo --expect 'boot: TempDemo' 'app: open TempDemo' 'temp: target=22' 'temp: frame'`
  **Expect:** PASS.
- [ ] **Do:** turn clockwise to the end, then counter-clockwise to the end. **Expect:** the image
  follows 19..27 C, `temp: target=<n>` per change, clamped at both ends.
- [ ] **Do:** touch and release the "SET" button. **Expect:** the pressed/released images swap, as
  the original.
- [ ] **Do:** click. **Expect:** `app: close TempDemo`.

## WifiScan (`examples/WifiScan`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/WifiScan --expect 'boot: WifiScan' 'app: open WiFi_Scan' 'wifi: scan' 'wifi: ap_count=[1-9]' --forbid 'wifi: scan failed' --timeout 30`
  **Expect:** PASS.
- [ ] **Do:** look at the list. **Expect:** your home APs, `i. (rssi) ssid`, at most 10.
- [ ] **Do:** turn clockwise. **Expect:** the list scrolls to later entries.
- [ ] **Do:** touch the screen and lift. **Expect:** "Scanning...", then a new list (`wifi: scan` again).
- [ ] **Do:** click. **Expect:** `wifi: heap=<n>`, `app: close WiFi_Scan`.

## BleServer (`examples/BleServer`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/BleServer --expect 'boot: BleServer' 'app: open BLE_Server' 'ble: advertising M5-[0-9A-F]+'`
  **Expect:** PASS. Note the name `M5-<hex>`.
- [ ] **Do:** `tools/.venv/bin/python tools/ble_check.py --name M5-<hex>` (the name from above).
  **Expect:** `ble_check: body_sensor_location=1`, three `ble_check: hr=<n>` lines with a rising
  counter, `ble_check: PASS`, exit 0. On the Dial: `ble: connected=1`, `ble: subscribed=1`, the
  page shows `send <hr>`.
- [ ] Optional: the same with nRF Connect on a phone (Heart Rate service 0x180D).
- [ ] **Do:** click. **Expect:** `ble: heap=<n>`, `app: close BLE_Server`.

## MemSoak (`examples/MemSoak`)

- [ ] **Do:**
  `tools/.venv/bin/python tools/device_test.py examples/MemSoak --expect 'heap: free=\d+ min=\d+' 'heap: free=\d+ min=\d+' 'heap: free=\d+ min=\d+' 'soak: done' --forbid 'boot: MemSoak.*boot: MemSoak' 'soak: FAIL' --timeout 180`
  **Expect:** PASS (three cycles, no reboot, no `soak: FAIL`).
- [ ] **Deferred: record the heap.** Copy the three `heap: free=<n> min=<n>` lines and the final
  `min=` into PORTING_NOTES, "Memory soak" and "Heap numbers" (replace "pending").

## Factory test, bare device (UserDemo)

- [ ] **Do:** `tools/upload.sh UserDemo` (the steps above leave MemSoak flashed).
- [ ] **Do:** hold the front button while you reset or power the Dial; keep holding.
  **Expect:** after > 1 s a double 4 kHz beep and the "[M5Dial测试]" intro screen; `app: open FactoryTest`.
- [ ] **Do:** release. LCD step: turn the knob through the pages, touch the screen, click.
  **Expect:** `factory: step LCD`, then the verdict screen, `factory: prompt LCD`.
- [ ] **Do:** on the verdict screen turn clockwise twice, then counter-clockwise past PASS.
  **Expect:** the highlight goes PASS → FAIL → SKIP and stops at both ends (no wrap). Click PASS:
  `factory: verdict LCD=PASS`.
- [ ] **Do:** RTC step: watch the time, click, PASS. **Expect:** it counts up; `factory: verdict RTC=PASS`.
- [ ] **Do:** RFID step: present a card (beep, UID), click, PASS. **Expect:** `factory: verdict RFID=PASS`.
- [ ] **Do:** IO step with nothing connected: click, SKIP. **Expect:** `[PORT-A] 无`;
  `factory: io adc=<mV> leds=<0..6> i2c=[]` lines; `factory: verdict IO=SKIP`.
- [ ] **Deferred: Ex_I2C scan time on a bare Port A.** **Do:** in the IO step, note the time between
  two `factory: io` lines. **Expect:** about 800 ms. Record it; a much longer gap means the scan
  stalls without pull-ups on G13/G15.
- [ ] **Do:** summary. **Expect:** `factory: LCD=PASS RTC=PASS RFID=PASS IO=SKIP`, "ALL OK" in green.
- [ ] **Do:** unplug USB (battery), click. **Expect:** `factory: rtc wakeup in 4 s`, black screen,
  power-off, and the Dial **switches itself back on after about 4 s** (RTC wake-up).

## Factory test, with the jig (UserDemo)

Wire the jig as in [FACTORY_TEST_JIG.md](FACTORY_TEST_JIG.md): 14-LED ring on Port B GPIO2,
pot wiper on GPIO1 (0-3.3 V), any I2C unit on Port A.

- [ ] **Do:** run the factory test to the IO step. Turn the pot from 0 V to 3.3 V.
  **Expect:** at 0 V the whole ring lights (`leds=6`); towards 3.3 V it empties to the two end LEDs
  (`leds=0`); `[PORT-B] <mV> <level>` follows.
- [ ] **Do:** look at `[PORT-A]`. **Expect:** the unit's I2C address, also in `i2c=[0x..]`.
- [ ] **Do:** click, PASS. **Expect:** the ring turns off; `factory: verdict IO=PASS`; summary
  `... IO=PASS`.

## LvglDemo (`examples/LvglDemo`)

- [ ] **Do:** `tools/.venv/bin/python tools/device_test.py examples/LvglDemo --expect 'boot: LvglDemo' 'lvgl: ready'`
  **Expect:** PASS.
- [ ] **Do:** turn clockwise 5 detents, then 1 counter-clockwise. **Expect:** the arc follows the
  knob; `lvgl: value=5`, then `lvgl: value=4`.
- [ ] **Deferred: RGB565 byte order and colours.** **Expect:** dark theme, blue arc, white
  Montserrat text; no swapped colours (blue must not look orange or green).
- [ ] **Do:** press the knob button (leaves edit mode), turn to focus the button, press.
  **Expect:** `lvgl: colour=orange`, the arc turns orange; again → `lvgl: colour=blue`.
- [ ] **Deferred: touch mapping.** **Do:** tap the on-screen button. **Expect:** it reacts where you
  tap (no mirrored or rotated hit area); `lvgl: colour=..`.
- [ ] **Deferred: LVGL heap.** Note whether the 64 KB `LV_MEM_SIZE` is enough (no crash, no
  missing widgets). Measuring the real use needs `lv_mem_monitor` (not built in).

## Optional: restore round-trip

Destructive: it overwrites the whole 8 MB flash.

- [ ] **Do:** `tools/restore_flash.sh backup/m5dial-factory-<date>.bin` (dry run). **Expect:** it
  names the port and the image, and asks for `--yes`.
- [ ] **Do:** `tools/restore_flash.sh backup/m5dial-factory-<date>.bin --yes`. **Expect:** size and
  sha256 verified, write OK; after a reset the **original** M5Stack firmware runs.
- [ ] **Do:** flash the port again: `tools/upload.sh UserDemo`. **Expect:** the launcher.

## Simulator items (first passing Wokwi run)

Not device checks, but open until the scenarios pass on Wokwi (the hang is #39).

- [ ] **LcdTest frame hash.** **Do:** `tools/sim.sh examples/LcdTest sim/scenarios/LcdTest/lcd_test.test.yaml`.
  **Expect:** record the `gc9a01: frame fnv=<hash>` of the touch step and add the
  `expect-chip-output` check the scenario comment describes.
- [ ] **Factory `io_leds`.** **Do:** `tools/sim.sh UserDemo sim/scenarios/UserDemo/io_leds.test.yaml`.
  **Expect:** PASS; this confirms Wokwi's ADC reads < 100 mV at pot position 0 (level 6).
- [ ] **NimBLE in Wokwi.** **Do:** `tools/sim.sh UserDemo sim/scenarios/UserDemo/launcher.test.yaml`.
  **Expect:** the BLE leg reaches `app: close BLE_Server`. If NimBLE init hangs or crashes,
  BleServer and the launcher BLE leg need an `M5DD_SIM` guard (owner decision).
- [ ] **Every other scenario** (`sim/scenarios/*/*.test.yaml`) passes at least once; settle delays
  tuned where needed.

## CI (first real run)

- [ ] The job names are `chips`, `host`, `build` (workflow `sketch`) and `scenario` (workflow `sim`);
  update required checks if needed.
- [ ] The `build` step summary shows the 11-sketch table, all `pass`, on Linux (bundled ctags).
- [ ] The `host` job finds `clang++` on ubuntu-24.04. If not, drop `CXX=clang++` from the job.
