"""Tests for png2rgb565.py: the converter must reproduce the shipped lcd-image-converter headers."""
import glob
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
ASSETS = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "..", "libraries", "M5DialDemo", "src", "assets"))
HEADER_DIRS = [os.path.join(ASSETS, "launcher_icons"), os.path.join(ASSETS, "temp_demo_icons")]


def shipped_header(png):
    """Return the shipped header path matching a PNG in assets/png."""
    name = os.path.basename(png)[:-4] + ".h"
    return next(os.path.join(d, name) for d in HEADER_DIRS if os.path.exists(os.path.join(d, name)))


class Png2Rgb565Test(unittest.TestCase):
    def test_regenerates_all_existing_headers(self):
        import png2rgb565

        pairs = [(p, shipped_header(p)) for p in sorted(glob.glob(ASSETS + "/png/*.png"))]
        self.assertGreaterEqual(len(pairs), 18)
        for png, hdr in pairs:
            with self.subTest(png=os.path.basename(png)):
                w, h, data = png2rgb565.convert(png)
                self.assertEqual(len(data), w * h)
                self.assertEqual(data, png2rgb565.parse_header(hdr))

    def test_cli_output_roundtrips(self):
        import tempfile
        import png2rgb565

        png = os.path.join(ASSETS, "png", "icon_ble.png")
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "x.h")
            self.assertEqual(png2rgb565.main([png, out, "--name", "icon_ble"]), 0)
            self.assertIn("static const uint16_t image_data_icon_ble[1764]", open(out).read())
            self.assertEqual(png2rgb565.parse_header(out), png2rgb565.convert(png)[2])


if __name__ == "__main__":
    unittest.main()
