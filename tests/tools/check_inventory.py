#!/usr/bin/env python3
"""Preserve original registrations and the recorded Windows-runner source baseline."""
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


def source_status(data: bytes, expected: str, added_import: str | None = None) -> str:
    data = data.replace(b"\r\n", b"\n")
    if hashlib.sha256(data).hexdigest() == expected:
        return "unchanged"
    if added_import is not None:
        lines = data.split(b"\n")
        permitted = added_import.encode("utf-8")
        if lines.count(permitted) == 1:
            lines.remove(permitted)
            if hashlib.sha256(b"\n".join(lines)).hexdigest() == expected:
                return "portable_import_fix"
    return "modified"


def check(root: Path = ROOT, windows_baseline: bool = False) -> dict:
    manifest = json.loads((root / MANIFEST.relative_to(ROOT)).read_bytes())
    if manifest.get("schema") != 1 or not manifest.get("cases"):
        raise ValueError("Missing/unsupported preservation inventory")
    additions = manifest.get("portable_import_additions", {})
    if not isinstance(additions, dict) or any(
        relative not in manifest["windows_sources"] or not isinstance(line, str)
        or not re.fullmatch(r"import [A-Za-z_][A-Za-z_0-9.]*;", line)
        for relative, line in additions.items()
    ):
        raise ValueError("Invalid portable import additions in preservation inventory")
    current = registrations(root)
    failures = check_names(current, manifest["cases"])
    unchanged = import_fixes = 0
    for relative, expected in (manifest["windows_sources"].items() if windows_baseline else []):
        path = root / relative
        if not path.is_file():
            failures.append("Missing protected Windows test: " + relative)
            continue
        status = source_status(path.read_bytes(), expected, additions.get(relative))
        if status == "unchanged":
            unchanged += 1
        elif status == "portable_import_fix":
            import_fixes += 1
        else:
            failures.append("Modified protected source beyond recorded imports: " + relative)
    return {"baseline_commit": manifest["baseline_commit"],
            "original_registrations": sum(row["count"] for row in manifest["cases"]),
            "current_registrations": sum(current.values()),
            "recorded_windows_sources": len(manifest["windows_sources"]),
            "verified_windows_sources": unchanged + import_fixes,
            "unchanged_windows_sources": unchanged,
            "portable_import_fixes": import_fixes,
            "failures": failures, "success": not failures}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows-baseline", action="store_true",
                        help="Audit original source bytes, allowing only recorded portable import additions")
    args = parser.parse_args()
    result = check(windows_baseline=args.windows_baseline)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result["success"] else 1)
