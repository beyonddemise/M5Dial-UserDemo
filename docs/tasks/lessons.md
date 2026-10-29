# Lessons

## 2026-10-30 — Don't assume there is no hardware; check first

- **Mistake:** the design said "the builder has no hardware", so all on-device verification was
  pushed onto the owner. An M5Dial was in fact plugged in via USB-C.
- **Rule:** for embedded/IoT work, before writing any verification plan, run a quick check
  (`ls /dev/cu.usb*`, `ioreg -p IOUSB`/`lsusb`) and ask if nothing shows up. Plan
  hardware-in-the-loop verification (flash + serial log) whenever a device is reachable,
  and leave only the truly human checks (visual, tactile, audio, external parts) to the owner.
- **Also:** flashing overwrites what's on the device, so back up the flash before the first write.
