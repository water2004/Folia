#!/usr/bin/env python3
"""Compare saved official family records with native SVGs in a real browser."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "regression"))
from comparison.corpus import require
from comparison.family_browser import Observer, load_cases
from comparison.family_checks import Check
from comparison.families import FAMILIES, compare_family


def run(args):
    from playwright.sync_api import sync_playwright
    rows = load_cases(args.family, args.reference, os.environ.get("FOLIA_TREE_VIEW_THEME"))
    check, results = Check(), []
    with sync_playwright() as playwright:
        options = {"headless": True}
        if args.browser:
            options["executable_path"] = args.browser
        browser = playwright.chromium.launch(**options)
        try:
            page_options = {"viewport": {"width": 1600, "height": 1200}} if args.family in ("sequence", "flowchart", "mindmap") else {}
            observer = Observer(browser.new_page(**page_options), args.family)
            for result, actual, expected in observer.pairs(rows, args.native, args.oracle_measurements):
                if actual is not None:
                    compare_family(args.family, actual, expected, check, args.oracle_measurements)
                results.append(result)
                print(result["status"].upper() + " " + result["name"])
        finally:
            browser.close()
    checked = sum(result["status"] == "passed" for result in results)
    report = {"checked": checked, "family": args.family, "maximumDifference": check.maximum,
              "maximumDifferenceAt": check.maximum_at, "results": results,
              "measurementSource": "original-browser" if args.oracle_measurements else "native-host"}
    output = args.output or args.reference.parent / ("original-measurements-comparison.json" if args.oracle_measurements else "comparison.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Compared {checked} native {args.family} SVGs against official captures")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("family", choices=sorted(FAMILIES))
    parser.add_argument("reference", type=Path)
    parser.add_argument("native", type=Path)
    parser.add_argument("--browser", default=os.environ.get("FOLIA_BROWSER"))
    parser.add_argument("--oracle-measurements", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    require(not args.oracle_measurements or args.family == "xy", "Original measurement mode is only defined for XY")
    run(args)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
