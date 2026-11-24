#!/usr/bin/env python3
"""Validate complete saved Canvas observations, optionally capturing them first."""

import argparse
import json
import sys
from pathlib import Path

from comparison.canvas_records import compare_observations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--official", type=Path, default=Path("build/mermaid-regression/zenuml-canvas"))
    parser.add_argument("--output", type=Path, default=Path("build/mermaid-regression/zenuml-canvas-native"))
    parser.add_argument("--observations", type=Path, help="Saved native-observations.json; comparison then runs without Node")
    parser.add_argument("--probe", type=Path, help="Capture fresh native/Skia observations before comparing")
    parser.add_argument("--node", default="node", help="Used only by the optional oracle sample capture stage")
    args = parser.parse_args()
    if args.probe:
        from comparison.canvas_capture import capture_native
        observations = capture_native(args.probe, args.official, args.output, node=args.node)
    else:
        observations = args.observations or args.output / "native-observations.json"
    report = compare_observations(args.official, observations)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for row in report["results"]:
        print(("PASS " if row["ok"] else "FAIL ") + row["id"])
    print(json.dumps({key: report[key] for key in ("passed", "total", "identicalRaster", "nativeFontRequests")}))
    return 0 if report["passed"] == report["total"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
