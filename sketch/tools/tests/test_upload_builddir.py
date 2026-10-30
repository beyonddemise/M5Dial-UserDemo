"""upload.sh must flash the binary it just built: never a sim build, whatever M5DIAL_BUILD_DIR says."""
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


class UploadBuildDirTest(unittest.TestCase):
    def run_upload(self, env_extra):
        """Run a copy of upload.sh with stubbed backup gate, build.sh and arduino-cli."""
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        tools = os.path.join(tmp.name, "tools")
        os.makedirs(os.path.join(tools, "bin"))
        for f in ("upload.sh", "env.sh", "deps.txt"):
            shutil.copy(os.path.join(TOOLS, f), tools)
        os.makedirs(os.path.join(tmp.name, "Foo"))
        _exe(os.path.join(tools, "check_backup.sh"), "#!/usr/bin/env bash\nexit 0\n")
        # why: record which build dir build.sh was told to use
        _exe(os.path.join(tools, "build.sh"),
             '#!/usr/bin/env bash\necho "build:${M5DIAL_BUILD_DIR:-unset}" >> "$LOG"\n')
        _exe(os.path.join(tools, "bin", "arduino-cli"),
             '#!/usr/bin/env bash\necho "cli:$*" >> "$LOG"\n')
        # why: record the post-upload watchdog reset (reset.sh) call and its port
        _exe(os.path.join(tools, "reset.sh"), '#!/usr/bin/env bash\necho "reset:$*" >> "$LOG"\n')
        log = os.path.join(tmp.name, "log")
        env = dict(os.environ, LOG=log, **env_extra)
        proc = subprocess.run([os.path.join(tools, "upload.sh"), os.path.join(tmp.name, "Foo"), "/dev/null"],
                              env=env, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        with open(log) as f:
            return tmp.name, f.read()

    def test_default_builds_and_uploads_device_dir(self):
        _, log = self.run_upload({})
        self.assertEqual(log.count(".build/Foo"), 2)
        self.assertNotIn("build:unset", log)

    def test_inherited_sim_build_dir_is_ignored(self):
        root, log = self.run_upload({"M5DIAL_BUILD_DIR": "/tmp/sim/Foo"})
        self.assertNotIn("/tmp/sim/Foo", log)
        self.assertEqual(log.count(".build/Foo"), 2)

    def test_watchdog_reset_runs_after_upload(self):
        _, log = self.run_upload({})
        lines = log.splitlines()
        self.assertTrue(lines[-1].startswith("reset:"), lines)
        self.assertTrue(lines[-2].startswith("cli:"), lines)


if __name__ == "__main__":
    unittest.main()
