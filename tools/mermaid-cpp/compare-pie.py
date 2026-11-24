#!/usr/bin/env python3
"""Compare all Pie fixtures with saved official DOM and stylesheet evidence."""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "regression"))
from comparison.pie import compare_capture


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("native", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = compare_capture(args.reference, args.native)
    destination = args.output or args.reference.parent / "comparison.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {report['checked']} Pie cases; compared {report['compared']} native DOMs")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
