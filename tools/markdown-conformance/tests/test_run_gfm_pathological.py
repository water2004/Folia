from __future__ import annotations

import importlib.util
import subprocess
import unittest
from pathlib import Path
from unittest.mock import Mock

SCRIPT = Path(__file__).resolve().parents[1] / "run_gfm_pathological.py"
SPEC = importlib.util.spec_from_file_location("run_gfm_pathological", SCRIPT)
runner = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(runner)


class PathologicalRunnerTests(unittest.TestCase):
    def completed(self, code: int, registered: int, skipped: int = 0) -> subprocess.CompletedProcess:
        if code == 0:
            stdout = (f"Suite 'unnamed suite': all tests passed (3 asserts in {registered} tests)"
                      + (f"; {skipped} tests skipped" if skipped else "") + "\n")
        else:
            stdout = f"Suite unnamed suite\ntests:   {registered} | 1 failed\nasserts: 3 | 1 failed\n"
            if skipped:
                stdout += f"{skipped} tests skipped\n"
        return subprocess.CompletedProcess(
            args=[], returncode=code,
            stdout=stdout,
            stderr="",
        )

    def test_zero_matching_boost_ut_tests_fail(self) -> None:
        invoke = Mock(return_value=self.completed(0, 21, 21))
        results = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, invoke, runner.CASES[:2])
        self.assertEqual(results[0]["status"], "failed")
        self.assertEqual(results[0]["matched_tests"], 0)
        self.assertEqual(results[1]["status"], "failed")

    def test_abnormal_process_exit_marks_remainder_not_run(self) -> None:
        invoke = Mock(return_value=self.completed(3221225477, 21, 21))
        results = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, invoke, runner.CASES[:2])
        self.assertEqual(results[0]["status"], "failed")
        self.assertEqual(results[0]["reason"], "abnormal process exit 3221225477")
        self.assertTrue(all(item["status"] == "not_run" for item in results[1:]))

    def test_timeout_fails_case_and_continues_with_next_isolated_process(self) -> None:
        invoke = Mock(side_effect=[
            subprocess.TimeoutExpired(cmd="focused", timeout=30),
            self.completed(0, 21, 20),
        ])
        results = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, invoke, runner.CASES[:2])
        self.assertEqual(results[0]["status"], "failed")
        self.assertEqual(results[0]["reason"], "exceeded 30 seconds")
        self.assertEqual(results[1]["status"], "passed")

    def test_one_matched_test_and_zero_exit_pass(self) -> None:
        invoke = Mock(return_value=self.completed(0, 21, 20))
        results = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, invoke, runner.CASES[:1])
        self.assertEqual(results[0]["status"], "passed")
        self.assertEqual(results[0]["matched_tests"], 1)

    def test_ansi_colored_boost_success_summary_is_parsed(self) -> None:
        completed = subprocess.CompletedProcess(
            args=[], returncode=0,
            stdout="\x1b[32mSuite 'unnamed suite': all tests passed (3 asserts in 21 tests); "
                   "20 tests skipped\x1b[0m\n",
            stderr="",
        )
        result = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, Mock(return_value=completed),
                                  runner.CASES[:1])[0]
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["registered_tests"], 21)
        self.assertEqual(result["skipped_tests"], 20)
        self.assertEqual(result["matched_tests"], 1)

    def test_ansi_colored_failure_summary_is_parsed(self) -> None:
        completed = subprocess.CompletedProcess(
            args=[], returncode=1,
            stdout="\x1b[31mSuite unnamed suite\ntests:   21 | 1 failed\n"
                   "asserts: 3 | 1 failed\n20 tests skipped\x1b[0m\n",
            stderr="",
        )
        result = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, Mock(return_value=completed),
                                  runner.CASES[:1])[0]
        self.assertEqual(result["status"], "failed")
        self.assertEqual(result["registered_tests"], 21)
        self.assertEqual(result["skipped_tests"], 20)
        self.assertEqual(result["matched_tests"], 1)

    def test_boost_fatal_exit_minus_one_with_failure_summary_is_test_failure(self) -> None:
        fatal = subprocess.CompletedProcess(
            args=[], returncode=0xFFFFFFFF,
            stdout="Suite unnamed suite\ntests:   21 | 1 failed\nasserts: 3 | 1 failed\n20 tests skipped\n",
            stderr="",
        )
        invoke = Mock(side_effect=[fatal, self.completed(0, 21, 20)])
        results = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, invoke, runner.CASES[:2])
        self.assertEqual(results[0]["status"], "failed")
        self.assertEqual(results[0]["reason"], "process exited 4294967295")
        self.assertEqual(results[0]["matched_tests"], 1)
        self.assertEqual(results[1]["status"], "passed")

    def test_real_failure_summary_counts_registered_minus_skipped_and_fails(self) -> None:
        invoke = Mock(side_effect=[
            self.completed(1, 21, 20),
            self.completed(0, 21, 20),
        ])
        results = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, invoke, runner.CASES[:2])
        self.assertEqual(results[0]["status"], "failed")
        self.assertEqual(results[0]["matched_tests"], 1)
        self.assertEqual(results[0]["registered_tests"], 21)
        self.assertEqual(results[0]["skipped_tests"], 20)
        self.assertEqual(results[1]["status"], "passed")

    def test_failure_summary_without_skip_count_does_not_count_registered_as_matched(self) -> None:
        completed = subprocess.CompletedProcess(
            args=[], returncode=1,
            stdout="Suite unnamed suite\ntests:   21 | 1 failed\nasserts: 3 | 1 failed\n",
            stderr="",
        )
        result = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, Mock(return_value=completed),
                                  runner.CASES[:1])[0]
        self.assertEqual(result["matched_tests"], 21)
        self.assertEqual(result["status"], "failed")

    def test_all_success_and_failure_suite_summaries_are_aggregated(self) -> None:
        completed = subprocess.CompletedProcess(
            args=[], returncode=1,
            stdout=("Suite 'parser suite': all tests passed (4 asserts in 5 tests); 4 tests skipped\n"
                    "Suite ownership suite\ntests:   20 | 1 failed\nasserts: 2 | 1 failed\n"
                    "15 tests skipped\n"),
            stderr="",
        )
        result = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, Mock(return_value=completed),
                                  runner.CASES[:1])[0]
        self.assertEqual(result["registered_tests"], 25)
        self.assertEqual(result["skipped_tests"], 19)
        self.assertEqual(result["matched_tests"], 6)
        self.assertEqual(result["status"], "failed")

    def test_multiple_suites_can_aggregate_to_exactly_one_match(self) -> None:
        completed = subprocess.CompletedProcess(
            args=[], returncode=0,
            stdout=("Suite 'other suite': all tests passed (0 asserts in 5 tests); 5 tests skipped\n"
                    "Suite 'pathological suite': all tests passed (3 asserts in 21 tests); 20 tests skipped\n"),
            stderr="",
        )
        result = runner.run_cases(Path("FoliaGfmPathologicalTests.exe"), 30, Mock(return_value=completed),
                                  runner.CASES[:1])[0]
        self.assertEqual(result["registered_tests"], 26)
        self.assertEqual(result["skipped_tests"], 25)
        self.assertEqual(result["matched_tests"], 1)
        self.assertEqual(result["status"], "passed")


if __name__ == "__main__":
    unittest.main()
