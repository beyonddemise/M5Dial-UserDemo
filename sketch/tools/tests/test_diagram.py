"""Checks on sim/diagram.json, the virtual M5Dial.

why: Wokwi's ESP32 boards default to flashSize "4" (MB); the firmware is built with the
StampS3's 8 MB partition table (app1 at 0x340000), so a 4 MB part rejects it at boot
("partition 3 invalid ... exceeds flash chip size 0x400000"), seen on the first real CI run.
"""
import json
import os
import unittest

DIAGRAM = os.path.join(os.path.dirname(__file__), "..", "..", "sim", "diagram.json")


class DiagramTest(unittest.TestCase):
    def test_esp_flash_is_stamps3_8mb(self):
        with open(DIAGRAM) as f:
            esp = next(p for p in json.load(f)["parts"] if p["id"] == "esp")
        self.assertEqual("8", esp.get("attrs", {}).get("flashSize"))


if __name__ == "__main__":
    unittest.main()
