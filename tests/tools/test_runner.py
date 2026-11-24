"""The public runner fails early and records the actual failed command."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("folia_runner", Path(__file__).parents[1] / "run.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def test_explicit_suite_selection_and_unknown_names(self):
        suites = runner.catalog()
        self.assertEqual(runner.select_suites(None, suites), ["core"])
        self.assertEqual(runner.select_suites(["math", "core", "math"], suites), ["math", "core"])
        self.assertEqual(runner.select_suites(["all"], suites), list(suites))
        for names in [["cor"], ["all", "core"]]:
            with self.subTest(names=names), self.assertRaises(ValueError):
                runner.select_suites(names, suites)

    def test_failed_build_prevents_ctest_and_keeps_failure_report(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(runner.subprocess, "run", side_effect=[
                subprocess.CompletedProcess([], 0), subprocess.CompletedProcess([], 7)
            ]) as execute:
                self.assertEqual(runner.main(["--build-dir", directory]), 7)
            self.assertEqual(execute.call_count, 2)
            report = json.loads((Path(directory) / "run.json").read_text())
            self.assertFalse(report["success"])
            self.assertEqual([row["returncode"] for row in report["commands"]], [0, 7])

    def test_ctest_failure_is_not_masked(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(runner.subprocess, "run", side_effect=[
                subprocess.CompletedProcess([], 0), subprocess.CompletedProcess([], 0),
                subprocess.CompletedProcess([], 8)
            ]) as execute:
                self.assertEqual(runner.main(["--suite", "math", "--build-dir", directory,
                                              "--test-regex", "does-not-exist"]), 8)
            command = execute.call_args.args[0]
            self.assertIn("--no-tests=error", command)
            self.assertEqual(command[-1], "does-not-exist")
            self.assertFalse(json.loads((Path(directory) / "run.json").read_text())["success"])

    def test_missing_program_reports_failure_without_attempting_later_steps(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(runner.subprocess, "run", side_effect=FileNotFoundError) as execute:
                self.assertEqual(runner.main(["--build-dir", directory]), 127)
            self.assertEqual(execute.call_count, 1)
            self.assertEqual(json.loads((Path(directory) / "run.json").read_text())["commands"][0]["returncode"], 127)


if __name__ == "__main__":
    unittest.main()
