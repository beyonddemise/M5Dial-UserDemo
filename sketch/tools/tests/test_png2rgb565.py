"""Tests for png2rgb565.py: the converter must reproduce the shipped lcd-image-converter headers."""
import glob
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
ORIGINALS = [
    os.path.join(REPO, "main/apps/launcher/launcher_icons"),
    os.path.join(REPO, "main/apps/app_temp_demo/gui/temp_demo_icons"),
]


class Png2Rgb565Test(unittest.TestCase):
    def test_regenerates_all_existing_headers(self):
        import png2rgb565  # noqa: E402  (imported late so RED shows a clear failure)

        pairs = [(p, p[:-4] + ".h") for d in ORIGINALS for p in sorted(glob.glob(d + "/*.png"))]
        self.assertGreaterEqual(len(pairs), 18)
        for png, hdr in pairs:
            with self.subTest(png=os.path.basename(png)):
                w, h, data = png2rgb565.convert(png)
                self.assertEqual(len(data), w * h)
                self.assertEqual(data, png2rgb565.parse_header(hdr))

    def test_cli_output_roundtrips(self):
        import tempfile
        import png2rgb565

        png = os.path.join(ORIGINALS[0], "icon_ble.png")
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "x.h")
            self.assertEqual(png2rgb565.main([png, out, "--name", "icon_ble"]), 0)
            self.assertIn("static const uint16_t image_data_icon_ble[1764]", open(out).read())
            self.assertEqual(png2rgb565.parse_header(out), png2rgb565.convert(png)[2])


if __name__ == "__main__":
    unittest.main()
