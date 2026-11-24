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
from comparison.family_checks import Check
from comparison.family_simple import FAMILIES, compare_family


def observation(family):
    source = (ROOT / "oracle/observations" / (family + ".mjs")).read_text(encoding="utf-8")
    declaration = "export default "
    require(source.count(declaration) == 1, "Capture module must export one browser observation")
    return "(" + source.split(declaration, 1)[1].rstrip().removesuffix(";") + ")"


def run(args):
    from playwright.sync_api import sync_playwright
    rows = json.loads(args.reference.read_text(encoding="utf-8"))
    require(isinstance(rows, list) and bool(rows), "Official capture must contain cases")
    check, results = Check(), []
    with sync_playwright() as playwright:
        options = {"headless": True}
        if args.browser:
            options["executable_path"] = args.browser
        browser = playwright.chromium.launch(**options)
        try:
            page = browser.new_page()
            observer = observation(args.family)
            for expected in rows:
                prefix = "" if args.family in ("venn", "wardley") else args.family + "-"
                if args.family == "xy" and args.oracle_measurements:
                    prefix = "xy-oracle-"
                source = (args.native / (prefix + expected["name"] + ".svg")).read_text(encoding="utf-8")
                if args.family == "wardley":
                    source = '<style>svg{font-family:"trebuchet ms"}</style>' + source
                page.set_content(source)
                if args.family in ("venn", "wardley"):
                    actual = page.evaluate("() => (" + observer + ")(document)")
                else:
                    actual = page.evaluate(observer)
                compare_family(args.family, actual, expected, check, args.oracle_measurements)
                results.append({"name": expected["name"], "status": "passed"})
                print("PASS " + expected["name"])
        finally:
            browser.close()
    report = {"checked": len(results), "family": args.family, "maximumDifference": check.maximum,
              "maximumDifferenceAt": check.maximum_at, "results": results,
              "measurementSource": "original-browser" if args.oracle_measurements else "native-host"}
    output = args.output or args.reference.parent / ("original-measurements-comparison.json" if args.oracle_measurements else "comparison.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Compared {len(rows)} native {args.family} SVGs against official captures")


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
