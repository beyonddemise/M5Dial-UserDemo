"""The Wokwi contribution package (issue #20): chips, examples, M5Dial board draft, workflow, docs.

why: each chip is meant to be split into its own public repo (`git subtree split`) and used from
Wokwi as `github:beyonddemise/chip-<name>@<version>`. Nothing in this repo runs those outward
steps, so these checks guard the parts that would only break after publishing: a chip that
needs files from outside its folder, an example or board that names a pin the chip does not
have, or a board pin map that drifts from the reference assembly (sim/diagram.json).
"""
import glob
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

import yaml

TOOLS = os.path.join(os.path.dirname(__file__), "..")
REPO = os.path.abspath(os.path.join(TOOLS, "..", ".."))
SIM = os.path.join(REPO, "sketch", "sim")
CHIPS_DIR = os.path.join(SIM, "chips")
CHIPS = sorted(os.path.basename(os.path.dirname(m))
               for m in glob.glob(os.path.join(CHIPS_DIR, "*", "Makefile")))
BOARD_DIR = os.path.join(SIM, "board-m5dial")
WORKFLOW = os.path.join(REPO, ".github", "workflows", "wokwi-chips.yml")
CONTRIBUTING = os.path.join(SIM, "CONTRIBUTING-WOKWI.md")
WASI_SDK = os.path.join(TOOLS, "wasi-sdk")

OWNER = "beyonddemise"
DEVKIT = "board-esp32-s3-devkitc-1"
# why: a pinned action is a full commit SHA; a tag (v4) can be moved to other code
PINNED_USES = re.compile(r"^[\w.-]+/[\w./-]+@[0-9a-f]{40}$")
CHIP_TYPE = re.compile(r"^github:(?P<owner>[\w-]+)/chip-(?P<name>[\w-]+)@(?P<ver>\d+\.\d+\.\d+)$")
# the board.json targets that are not chip pins: ESP32-S3 GPIOs, supply rails, reset
MCU_TARGET = re.compile(r"^(GPIO([0-9]|[1-3][0-9]|4[0-8])|GND|power\((3\.3|5)\)|NRST)$")


def chip_version(chip):
    """The release version a chip's example/diagram.json loads, e.g. "0.1.0".

    why the example as the source: each chip is released on its own, so versions differ per
    chip; the README and board.json must name the same one (CONTRIBUTING-WOKWI.md).
    """
    with open(os.path.join(CHIPS_DIR, chip, "example", "diagram.json")) as f:
        dep = json.load(f)["dependencies"]["chip-" + chip]
    m = CHIP_TYPE.match(dep)
    if not m or m["name"] != chip:
        raise ValueError(f"{chip}: example dependency {dep!r} is not github:<owner>/chip-{chip}@x.y.z")
    return m["ver"]


def dependency(chip):
    """The `dependencies` value that loads `chip` from its release (see CONTRIBUTING-WOKWI.md)."""
    return f"github:{OWNER}/chip-{chip}@{chip_version(chip)}"


def chip_pins(chip):
    """The pin names a chip's definition declares."""
    with open(os.path.join(CHIPS_DIR, chip, chip + ".chip.json")) as f:
        return json.load(f)["pins"]


def load_jsonc(path):
    """Parses JSON with comments and trailing commas, the dialect wokwi-boards' board.json uses.

    why a hand-rolled scanner: comment markers inside strings (URLs, "a//b") must survive, so a
    plain regex over the text is not enough; the board files are small, so speed does not matter.
    """
    with open(path) as f:
        text = f.read()
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            i = text.find("\n", i)
            i = n if i < 0 else i
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            if end < 0:
                raise ValueError(f"{path}: unterminated /* comment")
            i = end + 2
        else:
            out.append(c)
            i += 1
    # why after comment removal: `"x": 1, // note` + newline + `}` leaves `1, }`
    return json.loads(re.sub(r",(\s*[}\]])", r"\1", "".join(out)))


def endpoint_pin(endpoint):
    """Splits a diagram.json endpoint "part:pin" into (part, pin)."""
    part, _, pin = endpoint.partition(":")
    return part, pin


def mcu_net(esp_pin):
    """Maps a devkit pin name from diagram.json ("11", "3V3.1", "GND.2") to a board.json target."""
    if esp_pin.isdigit():
        return "GPIO" + esp_pin
    if esp_pin.startswith("3V3"):
        return "power(3.3)"
    if esp_pin.startswith("5V"):
        return "power(5)"
    if esp_pin.startswith("GND"):
        return "GND"
    raise ValueError(f"unmapped devkit pin {esp_pin}")


class LoadJsoncTest(unittest.TestCase):
    """load_jsonc(): comments and trailing commas go, strings stay intact."""

    def test_comments_trailing_commas_and_strings(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
            f.write('{\n  /* block */ "a": "x//y", // line\n  "b": ["/*z*/", 1,],\n}\n')
        self.addCleanup(os.unlink, f.name)
        self.assertEqual({"a": "x//y", "b": ["/*z*/", 1]}, load_jsonc(f.name))


class ChipPackageTest(unittest.TestCase):
    """Every chip folder is a complete Wokwi chip repo on its own."""

    def test_chips_found(self):
        self.assertEqual(["encoder", "ft3267", "gc9a01", "pcf8563", "ws1850s"], CHIPS)

    def test_template_files(self):
        for chip in CHIPS:
            for name in (chip + ".chip.json", chip + ".chip.c", "Makefile", "README.md",
                         "LICENSE", "wokwi-api.h", ".gitignore", "example/diagram.json"):
                with self.subTest(chip=chip, file=name):
                    self.assertTrue(os.path.isfile(os.path.join(CHIPS_DIR, chip, name)))

    def test_license_is_mit(self):
        for chip in CHIPS:
            with self.subTest(chip=chip), open(os.path.join(CHIPS_DIR, chip, "LICENSE")) as f:
                self.assertTrue(f.read().startswith("MIT License"))

    def test_readme_sections(self):
        # why these: what a Wokwi user needs before wiring the part (issue #20, step 2)
        sections = ("## Pins", "## Controls", "## Attributes", "## Interface behaviour",
                    "## Limitations", "## Use it in Wokwi", "## Build", "## License")
        for chip in CHIPS:
            with open(os.path.join(CHIPS_DIR, chip, "README.md")) as f:
                text = f.read()
            for heading in sections:
                with self.subTest(chip=chip, section=heading):
                    self.assertRegex(text, rf"(?m)^{re.escape(heading)}\s*$")
            with self.subTest(chip=chip, check="dependency snippet"):
                self.assertIn(f'"chip-{chip}": "{dependency(chip)}"', text)
                self.assertIn(f'"type": "chip-{chip}"', text)
                self.assertIn("example/diagram.json", text)

    def test_readme_lists_registers_or_commands(self):
        # why: the encoder has no bus interface; its behaviour section is the whole contract
        for chip in CHIPS:
            if chip == "encoder":
                continue
            with self.subTest(chip=chip), open(os.path.join(CHIPS_DIR, chip, "README.md")) as f:
                self.assertRegex(f.read(), r"(?m)^## (Registers|Supported (reader )?commands)")


class ExampleDiagramTest(unittest.TestCase):
    """example/diagram.json: a stock ESP32-S3 devkit plus that one chip, loaded by dependency."""

    def load(self, chip):
        with open(os.path.join(CHIPS_DIR, chip, "example", "diagram.json")) as f:
            return json.load(f)

    def test_only_devkit_and_the_chip(self):
        for chip in CHIPS:
            with self.subTest(chip=chip):
                d = self.load(chip)
                self.assertEqual(1, d["version"])
                self.assertEqual(sorted([DEVKIT, "chip-" + chip]),
                                 sorted(p["type"] for p in d["parts"]))
                self.assertEqual({"chip-" + chip: dependency(chip)}, d["dependencies"])

    def test_connections_name_real_pins(self):
        for chip in CHIPS:
            d = self.load(chip)
            ids = {p["id"]: p["type"] for p in d["parts"]}
            chip_id = next(i for i, t in ids.items() if t == "chip-" + chip)
            used = set()
            for conn in d["connections"]:
                for endpoint in conn[:2]:
                    part, pin = endpoint_pin(endpoint)
                    with self.subTest(chip=chip, endpoint=endpoint):
                        self.assertTrue(part in ids or part == "$serialMonitor")
                        if part == chip_id:
                            self.assertIn(pin, chip_pins(chip))
                            used.add(pin)
            with self.subTest(chip=chip, check="wired"):
                self.assertTrue(used, "the chip is not connected")


@unittest.skipUnless(os.path.isfile(os.path.join(WASI_SDK, "bin", "clang")),
                     "wasi-sdk not installed (sketch/tools/setup.sh)")
class ChipZipTest(unittest.TestCase):
    """`make zip` builds the release payload from a copy of the chip folder alone."""

    def test_zip_is_chip_json_and_wasm(self):
        for chip in CHIPS:
            with self.subTest(chip=chip), tempfile.TemporaryDirectory() as tmp:
                # why a copy outside the repo: proves the chip builds as its own repo would
                # (after `git subtree split`), with nothing from ../ in reach; dist/ is skipped
                # so a stale local build cannot stand in for the real one
                src = os.path.join(tmp, chip)
                shutil.copytree(os.path.join(CHIPS_DIR, chip), src,
                                ignore=shutil.ignore_patterns("dist"))
                subprocess.run(["make", "-s", "-C", src, "WASI_SDK_PATH=" + os.path.abspath(WASI_SDK),
                                "zip"], check=True, capture_output=True)
                with open(os.path.join(src, chip + ".chip.json")) as f:
                    definition = json.load(f)
                with zipfile.ZipFile(os.path.join(src, "dist", "chip.zip")) as z:
                    self.assertEqual(["chip.json", "chip.wasm"], sorted(z.namelist()))
                    self.assertEqual(definition, json.loads(z.read("chip.json")))
                    self.assertEqual(b"\0asm", z.read("chip.wasm")[:4])

    def test_zip_ignores_stale_dist(self):
        # why: a dist/ left from another checkout can be newer than the sources, and make then
        # skips the rebuild - wt-18 once packaged a chip.json with an old control id
        chip = CHIPS[0]
        with tempfile.TemporaryDirectory() as tmp:
            src = os.path.join(tmp, chip)
            shutil.copytree(os.path.join(CHIPS_DIR, chip), src,
                            ignore=shutil.ignore_patterns("dist"))
            os.mkdir(os.path.join(src, "dist"))
            future = 4102444800  # 2100-01-01, newer than any source
            for name, data in (("chip.json", b'{"name": "stale"}'), ("chip.wasm", b"stale")):
                path = os.path.join(src, "dist", name)
                with open(path, "wb") as f:
                    f.write(data)
                os.utime(path, (future, future))
            subprocess.run(["make", "-s", "-C", src, "WASI_SDK_PATH=" + os.path.abspath(WASI_SDK),
                            "zip"], check=True, capture_output=True)
            with open(os.path.join(src, chip + ".chip.json")) as f:
                definition = json.load(f)
            with zipfile.ZipFile(os.path.join(src, "dist", "chip.zip")) as z:
                self.assertEqual(definition, json.loads(z.read("chip.json")))
                self.assertEqual(b"\0asm", z.read("chip.wasm")[:4])


class BoardTest(unittest.TestCase):
    """board-m5dial/board.json: the wokwi-boards format, wired like sim/diagram.json."""

    @classmethod
    def setUpClass(cls):
        cls.board = load_jsonc(os.path.join(BOARD_DIR, "board.json"))
        cls.chip_of = {}  # board chip id -> chip folder name
        for c in cls.board.get("chips", []):
            m = CHIP_TYPE.match(c["type"])
            if m:
                cls.chip_of[c["id"]] = m["name"]
        cls.nets = {}  # target -> set of every target on the same net (incl. itself)
        for pin in cls.board["pins"].values():
            targets = pin["target"] if isinstance(pin["target"], list) else [pin["target"]]
            merged = set(targets)
            for t in targets:
                merged |= cls.nets.get(t, set())
            for t in merged:
                cls.nets[t] = merged

    def test_required_fields(self):
        b = self.board
        for key in ("name", "version", "description", "author", "width", "height", "mcu",
                    "fqbn", "pins", "chips", "displays"):
            with self.subTest(key=key):
                self.assertIn(key, b)
        self.assertEqual("esp32-s3", b["mcu"])
        self.assertEqual("8", b["mcuAttrs"]["flashSize"])

    def test_chips_are_released_packages_of_this_repo(self):
        types = sorted(c["type"] for c in self.board["chips"])
        self.assertEqual(sorted(dependency(c) for c in CHIPS), types)
        for c in self.board["chips"]:
            with self.subTest(chip=c["id"]):
                self.assertRegex(c["type"], CHIP_TYPE)
                self.assertEqual(OWNER, CHIP_TYPE.match(c["type"])["owner"])

    def test_targets_are_mcu_nets_or_chip_pins(self):
        for name, pin in self.board["pins"].items():
            targets = pin["target"] if isinstance(pin["target"], list) else [pin["target"]]
            for t in targets:
                with self.subTest(pin=name, target=t):
                    part, sep, chip_pin = t.partition(":")
                    if sep:
                        self.assertIn(part, self.chip_of)
                        self.assertIn(chip_pin, chip_pins(self.chip_of[part]))
                    else:
                        self.assertRegex(t, MCU_TARGET)

    def test_visible_pins_lie_on_the_board(self):
        for name, pin in self.board["pins"].items():
            if name.startswith("$"):
                continue  # internal net, not drawn
            with self.subTest(pin=name):
                self.assertTrue(0 <= pin["x"] <= self.board["width"])
                self.assertTrue(0 <= pin["y"] <= self.board["height"])

    def test_display_is_round_gc9a01(self):
        (disp,) = self.board["displays"]
        self.assertEqual("gc9a01", self.chip_of.get(disp["id"]))
        self.assertEqual("chip-gc9a01", disp["chip"])
        self.assertEqual((240, 240), (disp["pixelWidth"], disp["pixelHeight"]))
        self.assertAlmostEqual(disp["width"], disp["height"])

    def test_wiring_matches_reference_diagram(self):
        # why: sim/diagram.json is the assembly every scenario runs; a board that wires a chip
        # pin to another GPIO would boot the same firmware into a different machine
        with open(os.path.join(SIM, "diagram.json")) as f:
            d = json.load(f)
        part_chip = {p["id"]: p["type"][len("chip-"):] for p in d["parts"]
                     if p["type"].startswith("chip-")}
        board_id = {name: bid for bid, name in self.chip_of.items()}
        for conn in d["connections"]:
            ends = dict(endpoint_pin(e) for e in conn[:2])
            chip_end = next(((p, q) for p, q in ends.items() if p in part_chip), None)
            if chip_end is None or "esp" not in ends:
                continue
            part, chip_pin = chip_end
            target = f"{board_id[part_chip[part]]}:{chip_pin}"
            with self.subTest(connection=conn[:2]):
                self.assertIn(mcu_net(ends["esp"]), self.nets.get(target, set()))

    def test_m5dial_io(self):
        # why: the M5Dial's own I/O beyond the chips (M5Stack docs, main/hal/hal_common_define.h):
        # button G42, buzzer G3, Port A G13/G15, Port B G1/G2
        visible = {t for name, pin in self.board["pins"].items() if not name.startswith("$")
                   for t in (pin["target"] if isinstance(pin["target"], list) else [pin["target"]])}
        for gpio in ("GPIO42", "GPIO3", "GPIO13", "GPIO15", "GPIO1", "GPIO2"):
            with self.subTest(gpio=gpio):
                self.assertIn(gpio, visible)

    def test_svg(self):
        root = ET.parse(os.path.join(BOARD_DIR, "board.svg")).getroot()
        self.assertTrue(root.tag.endswith("svg"))
        self.assertEqual(f"{self.board['width']}mm", root.get("width"))
        self.assertEqual(f"{self.board['height']}mm", root.get("height"))
        self.assertTrue(os.path.isfile(os.path.join(BOARD_DIR, "README.md")))


class WorkflowTest(unittest.TestCase):
    """.github/workflows/wokwi-chips.yml builds every chip into a downloadable artefact."""

    @classmethod
    def setUpClass(cls):
        with open(WORKFLOW) as f:
            cls.text = f.read()
        cls.wf = yaml.safe_load(cls.text)
        # why: YAML 1.1 reads the bare key `on` as boolean true
        cls.on = cls.wf.get("on", cls.wf.get(True))

    def test_triggers(self):
        self.assertIn("workflow_dispatch", self.on)
        for event in ("pull_request", "push"):
            with self.subTest(event=event):
                paths = self.on[event]["paths"]
                self.assertIn("sketch/sim/chips/**", paths)
                self.assertIn(".github/workflows/wokwi-chips.yml", paths)

    def test_read_only(self):
        self.assertEqual({"contents": "read"}, self.wf["permissions"])
        self.assertNotIn("secrets.", self.text)
        # why: publishing a release is the owner's step (CONTRIBUTING-WOKWI.md), never CI's
        for needle in ("gh release", "action-gh-release", "contents: write", "git push", "git tag"):
            with self.subTest(needle=needle):
                self.assertNotIn(needle, self.text)

    def test_matrix_lists_every_chip(self):
        (job,) = self.wf["jobs"].values()
        self.assertEqual(CHIPS, sorted(job["strategy"]["matrix"]["chip"]))

    def test_actions_pinned_and_checkout_without_credentials(self):
        for job in self.wf["jobs"].values():
            for step in job["steps"]:
                if "uses" not in step:
                    continue
                with self.subTest(uses=step["uses"]):
                    self.assertRegex(step["uses"], PINNED_USES)
                    if step["uses"].startswith("actions/checkout@"):
                        self.assertIs(False, step["with"]["persist-credentials"])

    def test_builds_and_uploads_each_chip(self):
        self.assertIn("make", self.text)
        self.assertIn("zip", self.text)
        uploads = [s for j in self.wf["jobs"].values() for s in j["steps"]
                   if s.get("uses", "").startswith("actions/upload-artifact@")]
        self.assertEqual(1, len(uploads))
        self.assertEqual("${{ matrix.chip }}-chip", uploads[0]["with"]["name"])


class ContributingDocTest(unittest.TestCase):
    """CONTRIBUTING-WOKWI.md holds the owner's outward steps; nothing here runs them."""

    def test_covers_each_route(self):
        with open(CONTRIBUTING) as f:
            text = f.read()
        for needle in ("git subtree split --prefix sketch/sim/chips/",
                       "ver=0.1.0", "gh release create \"v$ver\"", "--prerelease", "chip.zip",
                       "Load custom board file", "wokwi/wokwi-boards",
                       "docs.google.com/document/d/1K59xkKL70uN_c_GNCs_Xk09F3tK12B-ygmFbw6i14t4",
                       "MIT", "#39"):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)
        for chip in CHIPS:
            with self.subTest(chip=chip):
                self.assertIn(f"chip-{chip}", text)

    def test_push_authenticates_through_gh(self):
        """The push in the release block works without a git credential helper set up.

        why: the first real release (chip-encoder v0.1.0) stopped at `git push https://...` with
        "could not read Username"; gh is the one tool the block already needs logged in.
        """
        with open(CONTRIBUTING) as f:
            text = f.read()
        pushes = [line for line in text.splitlines()
                  if line.lstrip().startswith("git") and " push " in line]
        self.assertEqual(1, len(pushes))
        # why credential.username=: gh returns nothing for a configured username that is not
        # the gh account (checked with `gh auth git-credential get`), and git then prompts
        self.assertIn("git -c credential.helper= -c 'credential.helper=!gh auth git-credential' "
                      "-c credential.username= push", pushes[0])

    def test_git_version_gate_stops_before_2_26(self):
        """Step 0 of the release block, run as written against a stubbed `git --version`.

        why a gate: Git before 2.26 can keep a URL-specific credential username despite the
        push's empty override, and the push would then prompt for a password.
        """
        with open(CONTRIBUTING) as f:
            text = f.read()
        gate = re.search(r"^# 0\..*?(?=^# 1\.)", text, re.S | re.M).group(0)
        self.assertNotIn("| sort -", gate)  # why: BusyBox sort lacks -C (and may lack -V)
        cases = {"2.25.1": False, "2.9.0": False, "2.26.0": True, "2.56.0": True,
                 "2.39.3 (Apple Git-145)": True, "3.0.0": True}
        with tempfile.TemporaryDirectory() as stub:
            git = os.path.join(stub, "git")
            with open(git, "w") as f:
                f.write('#!/bin/sh\necho "git version $FAKE_GIT_VERSION"\n')
            os.chmod(git, 0o755)
            for version, passes in cases.items():
                with self.subTest(version=version):
                    env = dict(os.environ, PATH=stub + os.pathsep + os.environ["PATH"],
                               FAKE_GIT_VERSION=version)
                    run = subprocess.run(["bash", "-eu", "-c", gate + "\necho GATE-PASSED"],
                                         env=env, capture_output=True, text=True)
                    self.assertEqual(passes, "GATE-PASSED" in run.stdout, run.stderr)
                    self.assertEqual(passes, run.returncode == 0)


if __name__ == "__main__":
    unittest.main()
