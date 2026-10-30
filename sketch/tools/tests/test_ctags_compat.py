"""Regression tests for .ino prototype generation (tools/ctags/ctags + the real build).

Background: on Apple Silicon, arduino-cli's bundled x86_64 ctags cannot run (no Rosetta) or may be a
stale raw-ctags symlink (older setup.sh runs).
Raw universal-ctags made arduino-cli emit ``setup();`` call statements *inside* setup(),
so the firmware recursed forever at boot. env.sh/build.sh now select the tools/ctags/ctags
wrapper per build. These tests pin the wrapper's output format and check build.sh end to end.
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest

TOOLS = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SKETCH_ROOT = os.path.dirname(TOOLS)
WRAPPER = os.path.join(TOOLS, "ctags", "ctags")
FIXTURE = os.path.join(os.path.dirname(__file__), "fixtures", "CtagsProto")

# What arduino-cli runs (see `arduino-cli compile -v`), minus the input file.
CTAGS_ARGS = ["-u", "--language-force=c++", "-f", "-", "--c++-kinds=svpf",
              "--fields=KSTtzns", "--line-directives"]


def _fields(row):
    """Map of the ``key:value`` extension fields of one ctags row."""
    return dict(p.split(":", 1) for p in row.split('/;"\t', 1)[1].split("\t"))


class WrapperFormatTest(unittest.TestCase):
    """tools/ctags/ctags rewrites universal-ctags rows into the Arduino ctags format."""

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

    def _run(self, real_ctags, check=True):
        """Run the wrapper on ``self.src`` with ``real_ctags`` as the universal-ctags binary."""
        res = subprocess.run([WRAPPER, *CTAGS_ARGS, self.src], capture_output=True, text=True,
                             env={**os.environ, "M5DIAL_UCTAGS": real_ctags}, check=check)
        return res if not check else res.stdout.splitlines()

    def _fake(self, version, rows=()):
        """A stand-in ctags binary printing ``version`` for --version, else canned ``rows``."""
        fake = os.path.join(self.dir, "fake-ctags")
        with open(fake, "w") as f:
            f.write(f'#!/bin/sh\n[ "$1" = --version ] && {{ echo "{version}"; exit 0; }}\n'
                    "cat <<'EOF'\n" + "\n".join(rows) + "\nEOF\n")
        os.chmod(fake, 0o755)
        return fake

    def test_rewrites_canned_universal_rows(self):
        """Needs no universal-ctags: a fake binary prints rows in its raw format."""
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
        fake = self._fake("Universal Ctags 6.2.1, Copyright (C) 2015-2025", rows)
        out = [_fields(r) for r in self._run(fake)]
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
        out = {r.split("\t")[0]: _fields(r) for r in self._run(uctags)}
        self.assertEqual(out["setup"]["line"], "2")
        self.assertEqual(out["setup"]["returntype"], "void")
        self.assertEqual(out["f"]["line"], "20")
        self.assertEqual(out["f"]["returntype"], "int")

    def test_universal_ctags_reports_physical_lines(self):
        """Pins why the wrapper renumbers: even with --line-directives (always passed by
        arduino-cli) universal-ctags applies line markers to the file name only, and
        reports physical lines. If a future universal-ctags fixes this, the wrapper's
        renumbering would double-translate - this test then fails first."""
        uctags = shutil.which("ctags")
        if not uctags or "Universal" not in subprocess.run(
                [uctags, "--version"], capture_output=True, text=True).stdout:
            self.skipTest("universal-ctags not installed")
        rows = subprocess.run([uctags, *CTAGS_ARGS, self.src], capture_output=True,
                              text=True, check=True).stdout.splitlines()
        raw = {r.split("\t")[0]: (r.split("\t")[1], _fields(r)["line"]) for r in rows}
        self.assertEqual(raw["setup"], ("/x/A.ino", "3"))  # logical line is 2
        self.assertEqual(raw["f"], ("/x/A.ino", "7"))      # logical line is 20

    def test_rejects_non_universal_ctags(self):
        """BSD/Exuberant ctags must fail loudly instead of mis-generating prototypes."""
        res = self._run(self._fake("Exuberant Ctags 5.8"), check=False)
        self.assertEqual(res.returncode, 1)
        self.assertIn("not universal-ctags", res.stderr)
        self.assertEqual(res.stdout, "")


class CtagsSelectionTest(unittest.TestCase):
    """env.sh picks the wrapper on every Darwin-arm64 host, Rosetta or not; never on Linux."""

    def _ctags_dir(self, uname_s, uname_m, rosetta):
        """Source env.sh with PATH shims faking uname/arch; return M5DIAL_CTAGS_DIR."""
        with tempfile.TemporaryDirectory() as shims:
            for name, body in (
                    ("uname", '[ "$1" = -s ] && echo %s || echo %s' % (uname_s, uname_m)),
                    ("arch", "exit %d" % (0 if rosetta else 1))):
                path = os.path.join(shims, name)
                with open(path, "w") as f:
                    f.write("#!/bin/sh\n" + body + "\n")
                os.chmod(path, 0o755)
            env = dict(os.environ, PATH=shims + os.pathsep + os.environ["PATH"])
            res = subprocess.run(
                ["bash", "-c", '. "$1" && printf %s "$M5DIAL_CTAGS_DIR"', "_",
                 os.path.join(TOOLS, "env.sh")],
                capture_output=True, text=True, env=env)
        self.assertEqual(res.returncode, 0, res.stderr)
        return res.stdout

    def test_darwin_arm64_without_rosetta(self):
        self.assertEqual(self._ctags_dir("Darwin", "arm64", False), os.path.join(TOOLS, "ctags"))

    def test_darwin_arm64_with_rosetta(self):
        self.assertEqual(self._ctags_dir("Darwin", "arm64", True), os.path.join(TOOLS, "ctags"))

    def test_linux_uses_bundled_ctags(self):
        self.assertEqual(self._ctags_dir("Linux", "x86_64", False), "")

    def test_darwin_x86_64_uses_bundled_ctags(self):
        self.assertEqual(self._ctags_dir("Darwin", "x86_64", True), "")


class SketchPrototypesTest(unittest.TestCase):
    """End to end via build.sh (ctags selection included): the bug that broke setup()."""

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
            raw = f.read().splitlines()
        code = [l for l in raw if not l.startswith("#line")]
        hint = ("broken .ino prototypes - check the ctags selection in tools/env.sh\n"
                + "\n".join(code))
        start = code.index("void setup() {")
        protos = {re.sub(r"\s+", "", l) for l in code[:start]}
        for p in ("voidsetup();", "voidloop();", "inthelper(intx);", "constchar*label();"):
            self.assertIn(p, protos, hint)
        body = code[start + 1:code.index("}", start)]
        self.assertEqual(body, ["  Serial.begin(115200);", "  Serial.println(label());"], hint)
        # Each prototype's #line must name the source line of its definition, so compiler
        # diagnostics point at the right place.
        with open(os.path.join(FIXTURE, "CtagsProto.ino")) as f:
            src = f.read().splitlines()
        defs = {"setup": "void setup() {", "loop": "void loop() {",
                "helper": "int helper(int x) {", "label": "const char* label() {"}
        for i, line in enumerate(raw[:raw.index("void setup() {")]):
            m = re.match(r"\S.* \*?(\w+)\(.*\);$", line)
            if m and m.group(1) in defs:
                target = re.match(r'#line (\d+) "', raw[i - 1])
                self.assertIsNotNone(target, hint)
                self.assertEqual(src[int(target.group(1)) - 1], defs.pop(m.group(1)), hint)
        self.assertEqual(defs, {}, hint)


if __name__ == "__main__":
    unittest.main()
