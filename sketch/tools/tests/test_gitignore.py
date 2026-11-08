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
REPO = os.path.join(SKETCH, "..")
LOCAL = (".arduino", ".build", "backup", "tools/.venv", "tools/bin", "tools/wasi-sdk")


def _ignored(repo, path):
    """True if the ignore rules in `repo`'s own .gitignore match `path`."""
    # why core.excludesFile: a user's global ignore (e.g. `.venv`) must not mask a missing rule
    proc = subprocess.run(["git", "-C", repo, "-c", "core.excludesFile=" + os.devnull,
                           "check-ignore", "--no-index", "-q", path])
    return proc.returncode == 0


class GitignoreTest(unittest.TestCase):
    def setUp(self):
        """A scratch repo holding only a copy of sketch/.gitignore."""
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = tmp.name
        # why an empty template: a custom init template could add info/exclude rules
        subprocess.run(["git", "init", "-q", "--template=", self.repo], check=True)
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


class RepoTreeTest(unittest.TestCase):
    def test_repo_tracks_no_gitlinks(self):
        # why: actions/checkout runs `git submodule foreach`, which fails on a gitlink (mode
        # 160000) that .gitmodules does not declare - a committed worktree folder breaks all CI
        out = subprocess.run(["git", "-C", REPO, "ls-files", "-s"], capture_output=True,
                             text=True, check=True).stdout
        gitlinks = [line.split("\t", 1)[1] for line in out.splitlines() if line.startswith("160000")]
        self.assertEqual([], gitlinks)

    def test_superpowers_working_folder_is_ignored_as_a_symlink(self):
        # why a symlink in a scratch repo: `/.superpowers/` would match a real folder only
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        subprocess.run(["git", "init", "-q", "--template=", tmp.name], check=True)
        shutil.copy(os.path.join(REPO, ".gitignore"), tmp.name)
        os.mkdir(os.path.join(tmp.name, "shared"))
        os.symlink(os.path.join(tmp.name, "shared"), os.path.join(tmp.name, ".superpowers"))
        self.assertTrue(_ignored(tmp.name, ".superpowers"))

    def test_backup_folder_is_ignored(self):
        # why: .backup holds local copies of files git removed from the tree; never re-add them
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        subprocess.run(["git", "init", "-q", "--template=", tmp.name], check=True)
        shutil.copy(os.path.join(REPO, ".gitignore"), tmp.name)
        os.mkdir(os.path.join(tmp.name, ".backup"))
        self.assertTrue(_ignored(tmp.name, ".backup/scratch-ctags"))

    def test_repo_tracks_no_chip_build_output(self):
        # why: every `make` / `make test` rewrites sim/chips/*/dist (wasm, zip, host_test), so a
        # tracked copy leaves the tree dirty after each test run; the Makefiles re-create it
        out = subprocess.run(["git", "-C", REPO, "ls-files", "sketch/sim/chips"],
                             capture_output=True, text=True, check=True).stdout
        self.assertEqual([], [p for p in out.splitlines() if "/dist/" in p])


if __name__ == "__main__":
    unittest.main()
