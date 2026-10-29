# Lessons

## 2026-10-30 — Don't assume there is no hardware; check first

- **Mistake:** the design said "the builder has no hardware", so all on-device verification was
  pushed onto the owner. An M5Dial was in fact plugged in via USB-C.
- **Rule:** for embedded/IoT work, before writing any verification plan, run a quick check
  (`ls /dev/cu.usb*`, `ioreg -p IOUSB`/`lsusb`) and ask if nothing shows up. Plan
  hardware-in-the-loop verification (flash + serial log) whenever a device is reachable,
  and leave only the truly human checks (visual, tactile, audio, external parts) to the owner.
- **Also:** flashing overwrites what's on the device, so back up the flash before the first write.

## 2026-10-30 — No compatibility shims that preserve legacy naming

- **Mistake:** the plan wrapped the M5Dial library in a facade that kept the original
  `hal->encoder.btn.read()` / `hal->tp` names to make porting easier.
- **Rule:** when porting to a target library, write code in that library's idioms and naming
  (`M5Dial.BtnA.wasPressed()`, `M5Dial.Touch.getDetail()`, `M5Canvas`). Readers won't know the
  legacy codebase; a shim optimises for the porter, not the maintainer. Port semantics, not names.

## 2026-10-30 — Batch human-only checks into one late task

- **Rule:** keep automated verification (host tests, simulation, flash + serial) inside each
  task; collect everything that needs a person (visual, tactile, cards, jig, phone) into a
  single acceptance task near the end, so execution isn't blocked waiting on the owner.

## 2026-10-30 — Verify the target library actually works before making it the contract

- **Mistake:** the design made `M5Dial.Encoder.read()` the encoder contract without checking it.
  M5Dial 1.0.3's `Encoder.h` has no interrupt pins above 39, so on GPIO40/41 it silently polls
  (m5stack/M5Dial#8). CodeRabbit caught it after the PR was merged.
- **Rule:** before a design depends on a third-party API for timing-critical I/O, read its source
  for the exact pins/board and search its open issues. Address review comments before merging.
