#!/usr/bin/env python3
"""Configure, build and run selected portable Folia suites from any directory."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
CATALOG = Path(__file__).with_name("suites.json")


def catalog() -> dict:
    document = json.loads(CATALOG.read_text(encoding="utf-8"))
    if document.get("schema") != 1 or not document.get("suites"):
        raise ValueError("Unsupported or empty test suite catalog")
    return document["suites"]


def select_suites(requested: list[str] | None, available: dict) -> list[str]:
    names = requested or ["core"]
    if "all" in names:
        if names != ["all"]:
            raise ValueError("Use --suite all on its own")
        return list(available)
    unknown = set(names) - available.keys()
    if unknown:
        raise ValueError("Unknown suites: " + ", ".join(sorted(unknown)))
    return list(dict.fromkeys(names))


def positive(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be at least one")
    return number


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", nargs="?", choices=("list", "configure", "build", "run"), default="run")
    parser.add_argument("--suite", action="append", help="repeat to select suites; all selects every portable suite")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build" / "tests" / "portable")
    parser.add_argument("--config", default="Release")
    parser.add_argument("--jobs", type=positive, default=min(4, os.cpu_count() or 1))
    parser.add_argument("--test-jobs", type=positive, default=1)
    parser.add_argument("--timeout", type=positive, default=180)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--cmake-arg", action="append", default=[], help="extra configure argument, e.g. --cmake-arg=-DCMAKE_TOOLCHAIN_FILE=...")
    parser.add_argument("--test-regex", help="optional CTest name filter; an empty selection fails")
    parser.add_argument("--json", action="store_true", help="machine-readable list output")
    args = parser.parse_args(argv)
    try:
        available = catalog()
        selected = select_suites(args.suite, available)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.action == "list":
        if args.json:
            print(json.dumps(available, indent=2))
        else:
            for name, suite in available.items():
                print(f"{name:14} {suite['description']}")
        return 0
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    configure = [args.cmake, "-S", str(ROOT / "tests"), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=" + args.config, "-DPython3_EXECUTABLE=" + sys.executable,
                 *args.cmake_arg,
                 "-DFOLIA_TEST_SUITES=" + ";".join(selected)]
    commands = [configure]
    if args.action in ("build", "run"):
        commands.append([args.cmake, "--build", str(build), "--config", args.config,
                         "--target", "FoliaSelectedTests", "--parallel", str(args.jobs)])
    if args.action == "run":
        commands.append([args.ctest, "--test-dir", str(build), "-C", args.config,
                         "--output-on-failure", "--no-tests=error", "--timeout", str(args.timeout),
                         "--parallel", str(args.test_jobs), "--output-junit", str(build / "results.xml")])
        if args.test_regex:
            commands[-1] += ["--tests-regex", args.test_regex]
    report = {"schema": 1, "suites": selected, "action": args.action, "commands": [], "success": False}
    status = 0
    try:
        for command in commands:
            print("+ " + shlex.join(command), flush=True)
            start = time.monotonic()
            try:
                status = subprocess.run(command, cwd=ROOT, check=False).returncode
            except FileNotFoundError:
                print(f"Required program not found: {command[0]}", file=sys.stderr)
                status = 127
            report["commands"].append({"argv": command, "returncode": status,
                                       "seconds": round(time.monotonic() - start, 3)})
            if status:
                break
        report["success"] = status == 0
    except KeyboardInterrupt:
        status = 130
    finally:
        (build / "run.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return status if status >= 0 else 128 - status


if __name__ == "__main__":
    sys.exit(main())
