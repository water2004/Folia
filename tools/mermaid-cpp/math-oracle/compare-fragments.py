#!/usr/bin/env python3
"""Compare complete MathJax/native fragment metrics, vectors and raw RGBA."""

import argparse
import json
import sys
from pathlib import Path

DIRECTORY = Path(__file__).resolve().parent
sys.path.insert(0, str(DIRECTORY.parent / "regression"))
from comparison.fragment_records import capture_fragments, compare_fragments
from profile_lock import verify_lock


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("native", type=Path)
    parser.add_argument("output", nargs="?", type=Path, default=Path("build/mermaid-math/comparison"))
    parser.add_argument("--observations", type=Path, help="Compare saved raster observations without running Node")
    parser.add_argument("--capture-only", action="store_true", help="Generate raster-observations.json without accepting any native result")
    parser.add_argument("--node", default="node", help="Used only for the optional real Sharp sample generation stage")
    args = parser.parse_args()
    verify_lock()
    corpus = DIRECTORY / "cases.json"
    observations = args.observations or capture_fragments(args.reference, args.native, corpus, args.output,
                                                         DIRECTORY / "capture-raster-samples.mjs", node=args.node)
    if args.capture_only:
        print(observations)
        return 0
    report = compare_fragments(args.reference, args.native, corpus, observations, args.output)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"destination": str(args.output), "total": report["total"], "accepted": report["accepted"],
                      "failures": [row["name"] for row in report["records"] if not row["accepted"]]}))
    return 0 if report["accepted"] == report["total"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
