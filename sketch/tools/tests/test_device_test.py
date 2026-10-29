"""Unit tests for the pure log-matching logic of device_test.py."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import device_test  # noqa: E402

LOG = "I (12) M5Dial: boot: smoke\nI (30) app: step=1\nI (50) app: step=2\n"


class EvaluateTest(unittest.TestCase):
    def test_expects_in_order_pass(self):
        self.assertEqual(device_test.evaluate(LOG, ["boot: smoke", r"step=1", r"step=2"], []), [])

    def test_expects_out_of_order_fail(self):
        problems = device_test.evaluate(LOG, [r"step=2", r"step=1"], [])
        self.assertEqual(len(problems), 1)
        self.assertIn("step=1", problems[0])

    def test_missing_expect_reported(self):
        problems = device_test.evaluate(LOG, ["nope"], [])
        self.assertEqual(len(problems), 1)
        self.assertIn("expect", problems[0])

    def test_forbid_matches_across_lines_dotall(self):
        # why: --forbid uses DOTALL so one pattern can span several log lines
        problems = device_test.evaluate(LOG, [], [r"step=1.*step=2"])
        self.assertEqual(len(problems), 1)
        self.assertIn("forbid", problems[0])

    def test_forbid_absent_passes(self):
        self.assertEqual(device_test.evaluate(LOG, [], ["Guru Meditation"]), [])


if __name__ == "__main__":
    unittest.main()
