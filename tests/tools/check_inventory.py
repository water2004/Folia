#!/usr/bin/env python3
"""Fail if reorganization loses original test registrations or changes Windows tests."""
from collections import Counter
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "tests/fixtures/preservation/registered-cases.json"
REGISTRATION = re.compile(r'"((?:\\.|[^"\\])*)"\s*_test\b')


def registrations(root: Path) -> Counter:
    names = Counter()
    for path in (root / "tests").rglob("*"):
        if path.suffix in (".cpp", ".hpp", ".ixx"):
            names.update(REGISTRATION.findall(path.read_text(encoding="utf-8-sig")))
    return names


def check_names(current: Counter, cases: list) -> list[str]:
    return [f"Missing test registration: {row['name']} ({current[row['name']]}/{row['count']})"
            for row in cases if current[row["name"]] < row["count"]]


def check(root: Path = ROOT, windows_baseline: bool = False) -> dict:
    manifest = json.loads((root / MANIFEST.relative_to(ROOT)).read_bytes())
    if manifest.get("schema") != 1 or not manifest.get("cases"):
        raise ValueError("Missing/unsupported preservation inventory")
    current = registrations(root)
    failures = check_names(current, manifest["cases"])
    for relative, expected in (manifest["windows_sources"].items() if windows_baseline else []):
        path = root / relative
        if not path.is_file():
            failures.append("Missing protected Windows test: " + relative)
        elif hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest() != expected:
            failures.append("Modified protected Windows test: " + relative)
    return {"baseline_commit": manifest["baseline_commit"],
            "original_registrations": sum(row["count"] for row in manifest["cases"]),
            "current_registrations": sum(current.values()),
            "recorded_windows_sources": len(manifest["windows_sources"]),
            "verified_windows_sources": len(manifest["windows_sources"]) if windows_baseline else 0,
            "failures": failures, "success": not failures}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows-baseline", action="store_true",
                        help="Audit the unchanged Windows sources during this refactor")
    args = parser.parse_args()
    result = check(windows_baseline=args.windows_baseline)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result["success"] else 1)
