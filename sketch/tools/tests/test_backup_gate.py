"""Tests for check_backup.sh, the 'no upload before a verified backup' gate."""
import hashlib
import os
import subprocess
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(__file__), "..", "check_backup.sh")
SIZE = 8388608


def _run(directory):
    return subprocess.run(["bash", SCRIPT, directory], capture_output=True, text=True)


def _make(directory, size=SIZE, good_hash=True, with_sha=True):
    """Create a fake backup of ``size`` bytes with (optionally wrong/missing) .sha256."""
    path = os.path.join(directory, "m5dial-factory-test.bin")
    data = b"\xa5" * size
    with open(path, "wb") as f:
        f.write(data)
    if with_sha:
        digest = hashlib.sha256(data).hexdigest() if good_hash else "0" * 64
        with open(path + ".sha256", "w") as f:
            f.write(f"{digest}  m5dial-factory-test.bin\n")


class BackupGateTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = self._tmp.name

    def tearDown(self):
        self._tmp.cleanup()

    def test_refuses_empty_dir(self):
        self.assertEqual(_run(self.dir).returncode, 1)

    def test_refuses_short_file(self):
        _make(self.dir, size=1024)
        self.assertEqual(_run(self.dir).returncode, 1)

    def test_refuses_missing_sha256(self):
        _make(self.dir, with_sha=False)
        self.assertEqual(_run(self.dir).returncode, 1)

    def test_refuses_wrong_hash(self):
        _make(self.dir, good_hash=False)
        self.assertEqual(_run(self.dir).returncode, 1)

    def test_accepts_valid_backup(self):
        _make(self.dir)
        self.assertEqual(_run(self.dir).returncode, 0)


if __name__ == "__main__":
    unittest.main()
