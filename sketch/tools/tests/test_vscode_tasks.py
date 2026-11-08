"""Checks on .vscode/tasks.json, the VSCode tasks of the sketchbook.

why: the "Sim test" task picks its scenario from a static list (input `scenario`); a scenario
file missing there cannot be started from VSCode (SETUP.md step 9).
"""
import glob
import json
import os
import re
import unittest

SKETCH = os.path.join(os.path.dirname(__file__), "..", "..")


class VscodeTasksTest(unittest.TestCase):
    def test_every_scenario_is_in_the_pick_list(self):
        """Every sim/scenarios/*/*.test.yaml is an option of the `scenario` input."""
        with open(os.path.join(SKETCH, ".vscode", "tasks.json")) as f:
            # why: VSCode allows // comments in tasks.json; json does not
            tasks = json.loads(re.sub(r"^\s*//.*$", "", f.read(), flags=re.M))
        options = next(i for i in tasks["inputs"] if i["id"] == "scenario")["options"]
        files = sorted(os.path.relpath(p, SKETCH)
                       for p in glob.glob(os.path.join(SKETCH, "sim", "scenarios", "*", "*.test.yaml")))
        self.assertEqual([], [p for p in files if p not in options])


if __name__ == "__main__":
    unittest.main()
