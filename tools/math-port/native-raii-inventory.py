#!/usr/bin/env python3
"""Generate source facts and unreviewed provenance notes for native math."""

import argparse
import json
from pathlib import Path
import sys
import unittest

from validation.raii.files import discover_root
from validation.raii.graph import Inventory
from validation.raii.reports import reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--build-dir", type=Path, help="Existing File API build; defaults to build/core")
    parser.add_argument("--configuration", default="Debug")
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.discover(str(Path(__file__).with_name("tests")), pattern="test_raii_inventory*.py")
        result = unittest.TextTestRunner(verbosity=1).run(suite)
        return 0 if result.wasSuccessful() else 1
    root = discover_root(__file__)
    report, manifest = reports(Inventory(root, build_dir=args.build_dir, configuration=args.configuration), __file__)
    destination = root / "build/math-port/inventory"
    for path, value in ((args.report or destination / "native-raii-inventory.json", report),
                        (args.manifest or destination / "native-raii-files.json", manifest)):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
