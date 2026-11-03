"""sketch/.gitignore covers the re-creatable tool folders even when they are symlinks.

why: git records a symlink as a file, so a `name/` (directory-only) pattern misses it. Worktrees
link these folders to one shared copy, and `git add sketch` then committed the links (PR #42).
"""
import os
import subprocess
import unittest

REPO = os.path.join(os.path.dirname(__file__), "..", "..", "..")
LOCAL = (".arduino", ".build", "backup", "tools/.venv", "tools/bin", "tools/wasi-sdk")


def _ignored(path):
    """True if git's ignore rules match `path` as a file (how git sees a symlink)."""
    proc = subprocess.run(["git", "-C", REPO, "check-ignore", "--no-index", "-q", path])
    return proc.returncode == 0


class GitignoreTest(unittest.TestCase):
    def test_local_folders_are_ignored_as_symlinks(self):
        for name in LOCAL:
            with self.subTest(name=name):
                self.assertTrue(_ignored("sketch/" + name))

    def test_tracked_sources_are_not_ignored(self):
        for path in ("sketch/tools/build.sh", "sketch/UserDemo/UserDemo.ino",
                     "sketch/libraries/M5DialDemo/library.properties"):
            with self.subTest(path=path):
                self.assertFalse(_ignored(path))


if __name__ == "__main__":
    unittest.main()
