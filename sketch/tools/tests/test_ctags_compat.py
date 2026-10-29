"""Regression tests for .ino prototype generation (ctags-compat.sh + the installed ctags).

Background: on Apple Silicon without Rosetta, setup.sh replaces arduino-cli's x86_64 ctags.
Raw universal-ctags made arduino-cli emit ``setup();`` call statements *inside* setup(),
so the firmware recursed forever at boot. These tests pin the wrapper's output format and
check the real toolchain end to end.
"""
import os
import re
import shutil
import stat
import subprocess
import tempfile
import unittest

TOOLS = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SKETCH_ROOT = os.path.dirname(TOOLS)
WRAPPER = os.path.join(TOOLS, "ctags-compat.sh")
FIXTURE = os.path.join(os.path.dirname(__file__), "fixtures", "CtagsProto")

# What arduino-cli runs (see `arduino-cli compile -v`), minus the input file.
CTAGS_ARGS = ["-u", "--language-force=c++", "-f", "-", "--c++-kinds=svpf",
              "--fields=KSTtzns", "--line-directives"]


def _render_wrapper(directory, real_ctags):
    """Install ctags-compat.sh into ``directory`` bound to ``real_ctags`` (as setup.sh does)."""
    with open(WRAPPER) as f:
        body = f.read().replace("@UCTAGS@", real_ctags)
    path = os.path.join(directory, "ctags")
    with open(path, "w") as f:
        f.write(body)
    os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)
    return path


def _fields(row):
    """Map of the ``key:value`` extension fields of one ctags row."""
    return dict(p.split(":", 1) for p in row.split('/;"\t', 1)[1].split("\t"))


class WrapperFormatTest(unittest.TestCase):
    """ctags-compat.sh rewrites universal-ctags rows into the Arduino ctags format."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = self._tmp.name
        # Input as arduino-cli's filtered sketch: markers at physical lines 1 and 6.
        self.src = os.path.join(self.dir, "in.cpp")
        with open(self.src, "w") as f:
            f.write('#line 1 "/x/A.ino"\n\nvoid setup() {\n}\n\n# 20 "/x/A.ino"\n'
                    "int f(int a) {\n}\n")

    def tearDown(self):
        self._tmp.cleanup()

    def _run(self, wrapper):
        return subprocess.run([wrapper, *CTAGS_ARGS, self.src],
                              capture_output=True, text=True, check=True).stdout.splitlines()

    def test_rewrites_canned_universal_rows(self):
        """Needs no universal-ctags: a fake binary prints rows in its raw format."""
        fake = os.path.join(self.dir, "fake-uctags")
        rows = [
            'setup\t/x/A.ino\t/^void setup() {$/;"\tkind:function\tline:3\t'
            "typeref:typename:void\tsignature:()",
            'f\t/x/A.ino\t/^int f(int a) {$/;"\tkind:function\tline:7\t'
            "typeref:typename:int\tsignature:(int a)",
            'g\t/x/A.ino\t/^struct P g() {$/;"\tkind:function\tline:8\t'
            "typeref:struct:P\tsignature:()",
            't\t/x/A.ino\t/^template <class T> T t(T a) {}$/;"\tkind:function\tline:8\t'
            "typeref:typename:T\tsignature:(T a)\ttemplate:<class T>",
            'v\t/x/A.ino\t/^P v;$/;"\tkind:variable\tline:8\ttyperef:typename:P',
        ]
        with open(fake, "w") as f:
            f.write("#!/bin/sh\ncat <<'EOF'\n" + "\n".join(rows) + "\nEOF\n")
        os.chmod(fake, 0o755)
        out = [_fields(r) for r in self._run(_render_wrapper(self.dir, fake))]
        self.assertEqual(out[0], {"kind": "function", "line": "2",
                                  "returntype": "void", "signature": "()"})
        self.assertEqual(out[1]["line"], "20")
        self.assertEqual(out[1]["returntype"], "int")
        self.assertEqual(out[2]["returntype"], "struct P")
        self.assertEqual(out[3]["returntype"], "template <class T> T")
        self.assertNotIn("template", out[3])
        # non-function tags keep their typeref (arduino-cli ignores it)
        self.assertEqual(out[4], {"kind": "variable", "line": "21", "typeref": "typename:P"})

    def test_real_universal_ctags(self):
        uctags = shutil.which("ctags")
        if not uctags or "Universal" not in subprocess.run(
                [uctags, "--version"], capture_output=True, text=True).stdout:
            self.skipTest("universal-ctags not installed")
        out = {r.split("\t")[0]: _fields(r) for r in self._run(_render_wrapper(self.dir, uctags))}
        self.assertEqual(out["setup"]["line"], "2")
        self.assertEqual(out["setup"]["returntype"], "void")
        self.assertEqual(out["f"]["line"], "20")
        self.assertEqual(out["f"]["returntype"], "int")


class SketchPrototypesTest(unittest.TestCase):
    """End to end with the *installed* toolchain: the class of bug that broke setup()."""

    def test_prototypes_precede_setup_and_body_is_intact(self):
        if not shutil.which("arduino-cli"):
            self.skipTest("arduino-cli not installed")
        if not os.path.isdir(os.path.join(SKETCH_ROOT, ".arduino", "data", "packages", "m5stack")):
            self.skipTest("M5Stack core not installed (run sketch/tools/setup.sh)")
        # why: --only-compilation-database runs the .ino preprocessing (ctags) but skips
        # the slow compile; build.sh uses the project's config, FQBN and data dir.
        res = subprocess.run(["bash", os.path.join(TOOLS, "build.sh"), FIXTURE,
                              "--only-compilation-database"], capture_output=True, text=True)
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
        with open(os.path.join(SKETCH_ROOT, ".build", "CtagsProto", "sketch",
                               "CtagsProto.ino.cpp")) as f:
            code = [l for l in f.read().splitlines() if not l.startswith("#line")]
        hint = ("broken .ino prototypes - on Apple Silicon without Rosetta rerun "
                "sketch/tools/setup.sh to install ctags-compat.sh\n" + "\n".join(code))
        start = code.index("void setup() {")
        protos = {re.sub(r"\s+", "", l) for l in code[:start]}
        for p in ("voidsetup();", "voidloop();", "inthelper(intx);", "constchar*label();"):
            self.assertIn(p, protos, hint)
        body = code[start + 1:code.index("}", start)]
        self.assertEqual(body, ["  Serial.begin(115200);", "  Serial.println(label());"], hint)


if __name__ == "__main__":
    unittest.main()
