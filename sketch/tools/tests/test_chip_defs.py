"""Static checks on the custom Wokwi chips' definitions (sim/chips/*/*.chip.json).

why: wokwi-cli validates a chip definition only when a scenario really runs, i.e. with CI quota;
an invalid control id once failed every scenario at once. These checks run on every push.
"""
import glob
import json
import os
import re
import unittest

SIM = os.path.join(os.path.dirname(__file__), "..", "..", "sim")
CHIP_JSONS = sorted(glob.glob(os.path.join(SIM, "chips", "*", "*.chip.json")))
SCENARIOS = sorted(glob.glob(os.path.join(SIM, "scenarios", "**", "*.test.yaml"), recursive=True))

# the pattern wokwi-cli's chip schema reports for /controls/N/id
CONTROL_ID = re.compile(r"^[a-zA-Z][a-zA-Z0-9]*$")
SET_CONTROL = re.compile(r"set-control:\s*\{([^}]*)\}")
FIELD = r"\b{}:\s*([^,}}\s]+)"


def controls(path):
    """Returns the control ids a chip definition declares."""
    return list(control_defs(path))


def control_defs(path):
    """Returns {control id: definition} for a chip definition."""
    with open(path) as f:
        return {c["id"]: c for c in json.load(f).get("controls", [])}


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
        # why only chip-* parts: built-in Wokwi parts (e.g. wokwi-pushbutton `pressed`) have
        # their own controls, documented by Wokwi, not in this repo
        with open(os.path.join(SIM, "diagram.json")) as f:
            part_type = {p["id"]: p["type"] for p in json.load(f)["parts"]}
        for path in SCENARIOS:
            with open(path) as f:
                steps = SET_CONTROL.findall(f.read())
            for step in steps:
                part = re.search(FIELD.format("part-id"), step).group(1)
                cid = re.search(FIELD.format("control"), step).group(1)
                ptype = part_type.get(part, "")
                if not ptype.startswith("chip-"):
                    continue
                name = ptype[len("chip-"):]
                chip = os.path.join(SIM, "chips", name, name + ".chip.json")
                with self.subTest(scenario=os.path.relpath(path, SIM), part=part, control=cid):
                    defs = control_defs(chip)
                    self.assertIn(cid, defs)
                    # why: an out-of-range value never reaches the chip (Brightness once set
                    # steps=100 on a -20..20 control and the knob never turned)
                    value = float(re.search(FIELD.format("value"), step).group(1))
                    self.assertGreaterEqual(value, defs[cid]["min"])
                    self.assertLessEqual(value, defs[cid]["max"])

if __name__ == "__main__":
    unittest.main()
