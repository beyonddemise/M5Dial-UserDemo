"""build.sh: a sketch with its own partitions.csv is size-checked against that table's app
partition (upload.maximum_size), not the board menu's PartitionScheme."""
import os
import shutil
import stat
import subprocess
import tempfile
import unittest

TOOLS = os.path.join(os.path.dirname(__file__), "..")


def _exe(path, text):
    with open(path, "w") as f:
        f.write(text)
    os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)


class BuildPartitionsTest(unittest.TestCase):
    def run_build(self, csv):
        """Run a copy of build.sh on sketch Foo (with `csv` as Foo/partitions.csv unless None)
        against a stub arduino-cli; return (exit code, stub's argv line, stderr)."""
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        tools = os.path.join(tmp.name, "tools")
        os.makedirs(os.path.join(tools, "bin"))
        for f in ("build.sh", "env.sh", "deps.txt"):
            shutil.copy(os.path.join(TOOLS, f), tools)
        sketch = os.path.join(tmp.name, "Foo")
        os.makedirs(sketch)
        if csv is not None:
            with open(os.path.join(sketch, "partitions.csv"), "w") as f:
                f.write(csv)
        log = os.path.join(tmp.name, "log")
        _exe(os.path.join(tools, "bin", "arduino-cli"), '#!/usr/bin/env bash\necho "cli:$*" >> "$LOG"\n')
        proc = subprocess.run([os.path.join(tools, "build.sh"), sketch],
                              env=dict(os.environ, LOG=log), capture_output=True, text=True)
        out = open(log).read() if os.path.exists(log) else ""
        return proc.returncode, out, proc.stderr

    def test_without_partitions_csv_the_board_maximum_applies(self):
        rc, out, err = self.run_build(None)
        self.assertEqual(rc, 0, err)
        self.assertNotIn("upload.maximum_size", out)

    def test_hex_app_size_becomes_the_maximum(self):
        rc, out, err = self.run_build(
            "# Name, Type, SubType, Offset, Size, Flags\n"
            "nvs,      data, nvs,      0x9000,   0x5000,\n"
            "otadata,  data, ota,      0xe000,   0x2000,\n"
            "app0,     app,  ota_0,    0x10000,  0x400000,\n"
            "coredump, data, coredump, 0x410000, 0x10000,\n")
        self.assertEqual(rc, 0, err)
        self.assertIn("--build-property upload.maximum_size=4194304", out)

    def test_suffixed_sizes_are_understood(self):
        rc, out, err = self.run_build("factory, app, factory, 0x10000, 4M,\n")
        self.assertEqual(rc, 0, err)
        self.assertIn("upload.maximum_size=4194304", out)
        rc, out, err = self.run_build("factory, app, factory, 0x10000, 1536K,\n")
        self.assertEqual(rc, 0, err)
        self.assertIn("upload.maximum_size=1572864", out)

    def test_table_without_an_app_partition_fails_clearly(self):
        rc, out, err = self.run_build("nvs, data, nvs, 0x9000, 0x5000,\n")
        self.assertNotEqual(rc, 0)
        self.assertIn("partitions.csv", err)
        self.assertEqual(out, "")


if __name__ == "__main__":
    unittest.main()
