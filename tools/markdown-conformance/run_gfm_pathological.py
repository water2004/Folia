#!/usr/bin/env python3
"""Run pinned full-size Folia parser cases in separate timed processes."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable

CASES = [
    ("gfm_pathological_nested_strong_emphasis_65000", "nested strong emph", 910001, "65,000 nested emphasis/strong pairs"),
    ("gfm_pathological_emphasis_closers_65000", "many emph closers with no openers", 195000, "65,000 unmatched emphasis closers"),
    ("gfm_pathological_emphasis_openers_65000", "many emph openers with no closers", 195000, "65,000 unmatched emphasis openers"),
    ("gfm_pathological_link_closers_65000", "many link closers with no openers", 130000, "65,000 unmatched closing brackets"),
    ("gfm_pathological_link_openers_65000", "many link openers with no closers", 130000, "65,000 unmatched opening brackets"),
    ("gfm_pathological_mismatched_markers_50000", "mismatched openers and closers", 200000, "50,000 mismatched marker groups"),
    ("gfm_pathological_marker_runs_50000", "openers and closers multiple of 3", 150004, "50,000 marker runs"),
    ("gfm_pathological_link_and_emphasis_markers_50000", "link openers and emph closers", 200000, "50,000 mixed marker groups"),
    ("gfm_pathological_bracket_paren_80000", "pattern [ (]( repeated", 400000, "80,000 bracket/parenthesis groups"),
    ("gfm_pathological_image_link_160000", "pattern ![[]() repeated", 960000, "160,000 image/link groups"),
    ("gfm_pathological_hard_link_emphasis", "hard link/emph case", 16, "one mixed link/emphasis construct"),
    ("gfm_pathological_nested_brackets_50000", "nested brackets", 100001, "50,000 nested bracket pairs"),
    ("gfm_pathological_nested_blockquotes_50000", "nested block quotes", 100001, "50,000 block quote prefixes"),
    ("gfm_pathological_deep_lists_1000", "deeply nested lists", 1003000, "1,000 nested list levels"),
    ("gfm_pathological_embedded_nul", "U+0000 in input", 7, "two embedded NUL characters"),
    ("gfm_pathological_backticks_4999_runs", "backticks", 12502499, "4,999 increasing backtick runs"),
    ("gfm_pathological_unclosed_angle_links_30000", "unclosed links A", 180000, "30,000 incomplete angle destinations"),
    ("gfm_pathological_unclosed_paren_links_30000", "unclosed links B", 150000, "30,000 incomplete parenthesized destinations"),
    ("gfm_pathological_unclosed_comments_300000", "unclosed <!--", 1200002, "300,000 comment-like openers"),
    ("gfm_pathological_tables_30000_rows", "tables", 330000, "30,000 CR/LF/vertical-tab row groups"),
    ("gfm_pathological_reference_collisions_50000", "reference collisions", 1143036, "49,999 definitions/references after selected key x8"),
]
SUCCESS_SUMMARY = re.compile(r"\((\d+) asserts? in (\d+) tests?\)")
FAILURE_SUMMARY = re.compile(r"^\s*tests:\s*(\d+)\s*\|\s*(\d+) failed\s*$", re.MULTILINE)
SKIPPED_SUMMARY = re.compile(r"\b(\d+) tests? skipped\b", re.IGNORECASE)
ANSI_ESCAPE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
MAX_OUTPUT_CHARS = 12000


def _report_output(value: str | None) -> str | None:
    if value is None or len(value) <= MAX_OUTPUT_CHARS:
        return value
    return value[:MAX_OUTPUT_CHARS] + "\n… output truncated in machine report …"


def _write_report(path: Path, timeout: float, results: list[dict]) -> None:
    counts = {status: sum(item["status"] == status for item in results)
              for status in ("passed", "failed", "not_run")}
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({
        "suite": "pinned cmark-gfm generated parsing adaptation",
        "source_commit": "27d942c8b0a62d192f616e5bf3578f4b6a89e180",
        "timeout_seconds": timeout,
        "expected_cases": len(CASES),
        **counts,
        "result": "passed" if counts["passed"] == len(CASES) else "failed",
        "cases": results,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def run_cases(executable: Path, timeout: float,
              invoke: Callable = subprocess.run, cases=CASES) -> list[dict]:
    results = [{
        "name": name,
        "upstream_case": upstream,
        "source_bytes": source_bytes,
        "scale_and_coverage": coverage,
        "status": "not_run",
        "reason": "not reached",
    } for name, upstream, source_bytes, coverage in cases]

    for item in results:
        started = time.monotonic()
        try:
            completed = invoke(
                [str(executable), f"*{item['name']}*"],
                check=False,
                timeout=timeout,
                capture_output=True,
                text=True,
                errors="replace",
            )
            item["duration_seconds"] = round(time.monotonic() - started, 3)
            item["exit_code"] = completed.returncode
            item["stdout"] = _report_output(completed.stdout)
            item["stderr"] = _report_output(completed.stderr)
            output = ANSI_ESCAPE.sub("", completed.stdout + "\n" + completed.stderr)
            success = SUCCESS_SUMMARY.findall(output)
            failure = FAILURE_SUMMARY.findall(output)
            registered_tests = sum(int(summary[1]) for summary in success)
            registered_tests += sum(int(summary[0]) for summary in failure)
            skipped = SKIPPED_SUMMARY.findall(output)
            if skipped:
                skipped_tests = sum(int(summary) for summary in skipped)
            else:
                skipped_tests = len(re.findall(r"\bSKIPPED\b", output))
            matched_tests = max(0, registered_tests - skipped_tests)
            item["registered_tests"] = registered_tests
            item["skipped_tests"] = skipped_tests
            item["matched_tests"] = matched_tests
            # Boost.UT's fatal assertion path calls std::exit(-1) after printing
            # its ordinary failure summary. Windows reports that as 0xffffffff.
            is_ut_fatal_failure = completed.returncode in (-1, 0xFFFFFFFF) and bool(failure)
            if completed.returncode not in (0, 1) and not is_ut_fatal_failure:
                item["status"] = "failed"
                item["reason"] = f"abnormal process exit {completed.returncode}"
                for remaining in results[results.index(item) + 1:]:
                    remaining["reason"] = "not run after abnormal process exit"
                break
            if matched_tests != 1:
                item["status"] = "failed"
                item["reason"] = (f"expected exactly one Boost.UT test, found {matched_tests} "
                                   f"({registered_tests} registered, {skipped_tests} skipped)")
            elif completed.returncode == 0:
                item["status"] = "passed"
                item["reason"] = None
            else:
                item["status"] = "failed"
                item["reason"] = f"process exited {completed.returncode}"
        except subprocess.TimeoutExpired as error:
            item["duration_seconds"] = round(time.monotonic() - started, 3)
            item["status"] = "failed"
            item["reason"] = f"exceeded {timeout:g} seconds"
            timeout_stdout = error.stdout.decode(errors="replace") if isinstance(error.stdout, bytes) else error.stdout
            timeout_stderr = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else error.stderr
            item["stdout"] = _report_output(timeout_stdout)
            item["stderr"] = _report_output(timeout_stderr)
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=30.0)
    parser.add_argument("--report", type=Path, default=Path("gfm_pathological_results.json"))
    args = parser.parse_args(argv)
    print(f"Running {len(CASES)} pinned full-size generated parser cases; per-case timeout {args.timeout_seconds:g}s", flush=True)
    results = run_cases(args.executable, args.timeout_seconds)
    _write_report(args.report, args.timeout_seconds, results)
    counts = {status: sum(item["status"] == status for item in results)
              for status in ("passed", "failed", "not_run")}
    for item in results:
        print(f"[{item['status'].upper()}] {item['name']}: {item['reason'] or 'one test matched'}", flush=True)
        if item["status"] == "failed":
            if item.get("stdout"):
                print(item["stdout"], end="" if item["stdout"].endswith("\n") else "\n", flush=True)
            if item.get("stderr"):
                print(item["stderr"], end="" if item["stderr"].endswith("\n") else "\n", file=sys.stderr, flush=True)
    print(f"{counts['passed']} passed, {counts['failed']} failed, {counts['not_run']} not run; report: {args.report}")
    return int(counts["passed"] != len(CASES))


if __name__ == "__main__":
    sys.exit(main())
