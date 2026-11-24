"""Real subprocess exit, truncation, timeout and Unicode protocol behavior."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.observation_process import JsonProcess


class ObservationProcessChecks(unittest.TestCase):
    def command(self, mode):
        return [sys.executable, str(Path(__file__).with_name("fixtures") / "observation-child.py"), mode]

    def test_unicode_round_trip(self):
        with JsonProcess(self.command("echo")) as process:
            process.write({"text": "A\U0001f600\u4e2d\u6587"})
            self.assertEqual(process.read(), {"text": "A\U0001f600\u4e2d\u6587"})

    def test_missing_response(self):
        with self.assertRaises(ValueError), JsonProcess(self.command("missing")) as process:
            process.read()

    def test_malformed_response(self):
        with self.assertRaises(ValueError), JsonProcess(self.command("malformed")) as process:
            process.read()

    def test_abnormal_exit_after_response(self):
        with self.assertRaises(ValueError), JsonProcess(self.command("abnormal")) as process:
            self.assertEqual(process.read(), {"received": True})

    def test_timeout_terminates_process(self):
        with self.assertRaises(ValueError), JsonProcess(self.command("timeout"), timeout=.2) as process:
            process.read()
        self.assertIsNotNone(process.process.poll())
