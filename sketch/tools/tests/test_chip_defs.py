"""Static checks on the custom Wokwi chips' definitions (sim/chips/*/*.chip.json).

why: wokwi-cli validates a chip definition only when a scenario really runs, i.e. with CI quota;
an invalid control id once failed every scenario at once. These checks run on every push.
"""
import glob
import json
import os
import re
import unittest

import yaml

SIM = os.path.join(os.path.dirname(__file__), "..", "..", "sim")
CHIP_JSONS = sorted(glob.glob(os.path.join(SIM, "chips", "*", "*.chip.json")))
SCENARIOS = sorted(glob.glob(os.path.join(SIM, "scenarios", "**", "*.test.yaml"), recursive=True))

# the pattern wokwi-cli's chip schema reports for /controls/N/id
CONTROL_ID = re.compile(r"^[a-zA-Z][a-zA-Z0-9]*$")


def controls(path):
    """Returns the control ids a chip definition declares."""
    return list(control_defs(path))


def control_defs(path):
    """Returns {control id: definition} for a chip definition."""
    with open(path) as f:
        return {c["id"]: c for c in json.load(f).get("controls", [])}


def set_controls(text):
    """Returns (part id, control, value) for each set-control step of a scenario's YAML text.

    why a YAML parser, not a regex: steps may be flow or block style, and Wokwi accepts the
    control as `name:` as well as `control:`.
    """
    steps = (yaml.safe_load(text) or {}).get("steps") or []
    out = []
    for step in steps:
        if not isinstance(step, dict) or "set-control" not in step:
            continue
        args = step["set-control"] or {}
        part, control = args.get("part-id"), args.get("control", args.get("name"))
        if part is None or control is None:
            raise ValueError(f"set-control step without part-id or control: {args}")
        out.append((part, control, args.get("value")))
    return out


class SetControlsTest(unittest.TestCase):
    """set_controls(): the set-control steps of one scenario text."""

    def test_flow_and_block_style_with_control_or_name_key(self):
        text = """
name: t
version: 1
steps:
  - wait-serial: 'x'
  - set-control: { part-id: rfid, control: cardPresent, value: 1 }
  - set-control:
      part-id: enc
      name: steps
      value: -20
"""
        self.assertEqual([("rfid", "cardPresent", 1), ("enc", "steps", -20)], set_controls(text))

    def test_step_without_part_or_control_is_an_error(self):
        with self.assertRaises(ValueError):
            set_controls("steps:\n  - set-control: { value: 1 }\n")

    def test_no_steps(self):
        self.assertEqual([], set_controls("name: t\n"))


class ChipDefsTest(unittest.TestCase):
    def test_chips_found(self):
        self.assertTrue(CHIP_JSONS, "no sim/chips/*/*.chip.json found")

    def test_control_ids_match_wokwi_pattern(self):
        for path in CHIP_JSONS:
            for cid in controls(path):
                with self.subTest(chip=os.path.basename(path), control=cid):
                    self.assertRegex(cid, CONTROL_ID)

    def test_chip_source_reads_each_control(self):
        # why: a control's value reaches the chip only through attr_init[_float]("<id>", ...);
        # the id as a string literal also covers names kept in a table (ws1850s uid0..uid3)
        for path in CHIP_JSONS:
            src = path.replace(".chip.json", ".chip.c")
            with open(src) as f:
                code = f.read()
            for cid in controls(path):
                with self.subTest(chip=os.path.basename(path), control=cid):
                    self.assertIn(f'"{cid}"', code)

    def test_scenarios_set_only_declared_controls(self):
        with open(os.path.join(SIM, "diagram.json")) as f:
            part_type = {p["id"]: p["type"] for p in json.load(f)["parts"]}
        for path in SCENARIOS:
            with open(path) as f:
                steps = set_controls(f.read())
            for part, cid, value in steps:
                with self.subTest(scenario=os.path.relpath(path, SIM), part=part, control=cid):
                    # why first: a mistyped part id would otherwise be skipped as "not a chip"
                    self.assertIn(part, part_type)
                    # why only chip-* parts: built-in Wokwi parts (e.g. wokwi-pushbutton
                    # `pressed`) have their own controls, documented by Wokwi, not in this repo
                    if not part_type[part].startswith("chip-"):
                        continue
                    name = part_type[part][len("chip-"):]
                    defs = control_defs(os.path.join(SIM, "chips", name, name + ".chip.json"))
                    self.assertIn(cid, defs)
                    # why: an out-of-range value never reaches the chip (Brightness once set
                    # steps=100 on a -20..20 control and the knob never turned)
                    self.assertGreaterEqual(float(value), defs[cid]["min"])
                    self.assertLessEqual(float(value), defs[cid]["max"])


if __name__ == "__main__":
    unittest.main()
