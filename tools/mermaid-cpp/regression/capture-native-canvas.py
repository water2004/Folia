#!/usr/bin/env python3
"""Capture native ZenUML plus actual pinned Skia measurements and RGBA samples."""

import argparse
import sys
from pathlib import Path

from comparison.canvas_capture import capture_native


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--official", type=Path, default=Path("build/mermaid-regression/zenuml-canvas"))
    parser.add_argument("--output", type=Path, default=Path("build/mermaid-regression/zenuml-canvas-native"))
    parser.add_argument("--node", default="node")
    args = parser.parse_args()
    print(capture_native(args.probe, args.official, args.output, node=args.node))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
