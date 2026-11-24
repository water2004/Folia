#!/usr/bin/env python3
"""Compare saved browser and native font measurements (no Windows SDK needed)."""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "regression"))
from comparison.text_probe import compare_text_probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path, default=Path("build/mermaid-cpp"))
    args = parser.parse_args()
    report = compare_text_probe((args.directory / "browser-text.tsv").read_text(encoding="utf-8"),
                                (args.directory / "native-text.tsv").read_text(encoding="utf-8"))
    (args.directory / "text-comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
