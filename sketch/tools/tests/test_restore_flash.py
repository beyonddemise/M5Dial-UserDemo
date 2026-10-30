"""Tests for restore_flash.sh safety checks (never reaches esptool / a device)."""
import hashlib
import os
import subprocess
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(__file__), "..", "restore_flash.sh")
SIZE = 8388608


def _run(*args):
    return subprocess.run(["bash", SCRIPT, *args], capture_output=True, text=True)


def _image(directory, name, byte):
    path = os.path.join(directory, name)
    data = bytes([byte]) * SIZE
    with open(path, "wb") as f:
        f.write(data)
    return path, hashlib.sha256(data).hexdigest()


def _sidecar(path, digest, named):
    with open(path + ".sha256", "w") as f:
        f.write(f"{digest}  {named}\n")


class RestoreFlashTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = self._tmp.name

    def tearDown(self):
        self._tmp.cleanup()

    def test_refuses_image_not_matching_sidecar_that_names_another_file(self):
        good, good_hash = _image(self.dir, "good.bin", 0xA5)
        evil, _ = _image(self.dir, "evil.bin", 0x5A)
        _sidecar(evil, good_hash, "good.bin")  # sidecar copied from another backup
        r = _run(evil, "--yes", "/dev/null-test-port")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("checksum mismatch", r.stderr)

    def test_refuses_missing_sidecar(self):
        img, _ = _image(self.dir, "a.bin", 1)
        self.assertNotEqual(_run(img, "--yes", "/dev/x").returncode, 0)

    def test_refuses_wrong_size(self):
        path = os.path.join(self.dir, "s.bin")
        with open(path, "wb") as f:
            f.write(b"x" * 10)
        self.assertNotEqual(_run(path, "--yes", "/dev/x").returncode, 0)

    def test_without_yes_only_reports_and_exits_1(self):
        img, h = _image(self.dir, "ok.bin", 7)
        _sidecar(img, h, "ok.bin")
        r = _run(img, "--dry", "/dev/explicit-port")
        self.assertEqual(r.returncode, 1)
        self.assertIn("Would OVERWRITE", r.stdout + r.stderr)
        self.assertIn("/dev/explicit-port", r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main()
