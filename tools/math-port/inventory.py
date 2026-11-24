#!/usr/bin/env python3
"""Inventory the pinned MathJax scope; accepted names are never guessed again."""
import argparse
from pathlib import Path
import sys

from validation.scope import generate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    document = generate(Path(__file__).resolve().parents[2], args.check)
    unresolved = len(document["unresolved_imports"])
    print(f'{len(document["files"])} upstream files, {len(document["profile"]["tex_packages"])} '
          f'TeX packages, {unresolved} unresolved relative imports; external imports still require review')
    return 1 if unresolved else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
