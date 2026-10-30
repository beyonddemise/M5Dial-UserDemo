"""Tests for png2rgb565.py: the converter must reproduce the shipped lcd-image-converter headers."""
import glob
import os
import re
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


def all_headers():
    """Return every shipped per-image header path."""
    return sorted(h for d in HEADER_DIRS for h in glob.glob(d + "/*.h"))


class Png2Rgb565Test(unittest.TestCase):
    def test_png_and_header_sets_match(self):
        pngs = {os.path.basename(p)[:-4] for p in glob.glob(ASSETS + "/png/*.png")}
        hdrs = {os.path.basename(h)[:-2] for h in all_headers()}
        self.assertEqual(pngs - hdrs, set(), "PNGs without a header")
        self.assertEqual(hdrs - pngs, set(), "headers without a PNG")

    def test_headers_have_pragma_once_and_valid_regen_command(self):
        repo = os.path.abspath(os.path.join(ASSETS, "..", "..", "..", "..", ".."))
        for hdr in all_headers():
            with self.subTest(header=os.path.basename(hdr)):
                text = open(hdr, encoding="utf-8").read()
                self.assertEqual(text.count("#pragma once"), 1)
                name = os.path.basename(hdr)[:-2]
                m = re.search(r"^// Regenerable \(from repo root\): (.+)$", text, re.M)
                self.assertIsNotNone(m)
                parts = m.group(1).split()
                self.assertEqual(parts[0], "python3")
                self.assertTrue(os.path.isfile(os.path.join(repo, parts[1])))
                self.assertTrue(os.path.isfile(os.path.join(repo, parts[2])))
                self.assertEqual(os.path.join(repo, parts[3]), hdr)
                self.assertEqual(parts[4:], ["--name", name])

    def test_regenerates_all_existing_headers(self):
        import png2rgb565

        pairs = [(p, shipped_header(p)) for p in sorted(glob.glob(ASSETS + "/png/*.png"))]
        self.assertGreaterEqual(len(pairs), 18)
        for png, hdr in pairs:
            with self.subTest(png=os.path.basename(png)):
                w, h, data = png2rgb565.convert(png)
                self.assertEqual(len(data), w * h)
                self.assertEqual(data, png2rgb565.parse_header(hdr))

    def test_cli_regeneration_keeps_note_pragma_and_data(self):
        """Regenerating a committed header via its own note reproduces note, pragma and data.

        Only the lcd-image-converter banner (between the note and the pragma) differs.
        """
        import shutil
        import subprocess
        import png2rgb565

        hdr = os.path.join(HEADER_DIRS[0], "icon_ble.h")
        committed = open(hdr, encoding="utf-8").read()
        note = committed.splitlines()[0]
        repo = png2rgb565._REPO_ROOT
        backup = hdr + ".bak"
        shutil.copyfile(hdr, backup)
        try:
            args = note.split(": ", 1)[1].split()[1:]  # drop "python3"
            subprocess.run([sys.executable] + args, cwd=repo, check=True)
            regen = open(hdr, encoding="utf-8").read()
            regen_words = png2rgb565.parse_header(hdr)
        finally:
            shutil.move(backup, hdr)
        self.assertEqual(regen.splitlines()[0], note)
        self.assertEqual(regen.count("#pragma once"), 1)
        self.assertEqual(regen_words, png2rgb565.parse_header(hdr))

    def test_cli_output_roundtrips(self):
        import tempfile
        import png2rgb565

        png = os.path.join(ASSETS, "png", "icon_ble.png")
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "x.h")
            self.assertEqual(png2rgb565.main([png, out, "--name", "icon_ble"]), 0)
            self.assertIn("static const uint16_t image_data_icon_ble[1764]", open(out).read())
            self.assertIn("#pragma once", open(out).read())
            self.assertEqual(png2rgb565.parse_header(out), png2rgb565.convert(png)[2])


if __name__ == "__main__":
    unittest.main()
