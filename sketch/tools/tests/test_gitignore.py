"""sketch/.gitignore covers the re-creatable tool folders even when they are symlinks.

why: git records a symlink as a file, so a `name/` (directory-only) pattern misses it. Worktrees
link these folders to one shared copy, and `git add sketch` then committed the links (PR #42).
"""
import os
import shutil
import subprocess
import tempfile
import unittest

SKETCH = os.path.join(os.path.dirname(__file__), "..", "..")
LOCAL = (".arduino", ".build", "backup", "tools/.venv", "tools/bin", "tools/wasi-sdk")


def _ignored(repo, path):
    """True if git's ignore rules in `repo` match `path`."""
    proc = subprocess.run(["git", "-C", repo, "check-ignore", "--no-index", "-q", path])
    return proc.returncode == 0


class GitignoreTest(unittest.TestCase):
    def setUp(self):
        """A scratch repo holding only a copy of sketch/.gitignore."""
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = tmp.name
        subprocess.run(["git", "init", "-q", self.repo], check=True)
        shutil.copy(os.path.join(SKETCH, ".gitignore"), self.repo)

    def test_local_folders_are_ignored_as_symlinks(self):
        # why a real symlink per entry: a real directory would also match a `name/` rule
        target = os.path.join(self.repo, "shared")
        os.mkdir(target)
        for name in LOCAL:
            with self.subTest(name=name):
                link = os.path.join(self.repo, name)
                os.makedirs(os.path.dirname(link), exist_ok=True)
                os.symlink(target, link)
                self.assertTrue(_ignored(self.repo, name))

    def test_tracked_sources_are_not_ignored(self):
        for path in ("tools/build.sh", "UserDemo/UserDemo.ino",
                     "libraries/M5DialDemo/library.properties"):
            with self.subTest(path=path):
                self.assertFalse(_ignored(self.repo, path))


if __name__ == "__main__":
    unittest.main()
