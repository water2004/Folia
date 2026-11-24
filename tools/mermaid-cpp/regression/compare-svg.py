#!/usr/bin/env python3
"""Compare all official/native SVG captures without a Node.js test runtime."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from comparison.corpus import UPSTREAM, load_corpus, require
from comparison.geometry import GEOMETRY_TOLERANCE
from comparison.pixels import CHANNEL_TOLERANCE, RASTER_SCALE
from comparison.reports import Report
from comparison.svg import compare_svg, fontconfig_resolver


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--official")
    parser.add_argument("--native")
    parser.add_argument("--output")
    parser.add_argument("--browser", default=os.environ.get("FOLIA_BROWSER"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        import unittest
        suite = unittest.defaultTestLoader.discover(str(Path(__file__).with_name("tests")), pattern="test_*.py")
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    if not args.official or not args.native:
        parser.error("Both --official and --native reports are required")
    records, summary = load_corpus()
    official, native = Report(args.official), Report(args.native)
    require(official.document.get("upstream") == UPSTREAM, "Official Mermaid version/commit mismatch")
    require(official.document.get("stage") == "official-svg", "An official SVG capture report is required")
    known = {item["id"] for item in records}
    for report in (official, native):
        require(set(report.rows) <= known, "Report contains unknown or removed corpus cases")
    resolve_font = fontconfig_resolver() if sys.platform.startswith("linux") else None
    browser = playwright = page = browser_error = None
    results = []
    try:
        for item in records:
            try:
                corpus_hash = next(row["sha256"] for row in summary if row["family"] == item["family"])
                expected, actual = official.source(item, corpus_hash), native.source(item, corpus_hash)
                capture = official.rows[item["id"]]
                free = item["family"] in ("packet", "info")
                if free:
                    require(capture.get("browser") is True or (capture.get("renderer") == "official-mermaid-jsdom" and
                        capture.get("geometryMeasurements") == "none-required"), "Official capture used synthetic measurements")
                else:
                    require(capture.get("browser") is True and capture.get("renderer") == "official-browser",
                            "Official layout was not captured in a real browser")
                if free and resolve_font:
                    result = compare_svg(expected, actual, item["family"], resolve_font)
                else:
                    if browser_error:
                        raise ValueError(browser_error)
                    if browser is None:
                        try:
                            from playwright.sync_api import sync_playwright
                            playwright = sync_playwright().start()
                            options = {"headless": True}
                            if args.browser:
                                options["executable_path"] = args.browser
                            browser = playwright.chromium.launch(**options)
                            page = browser.new_page(viewport={"width": 1600, "height": 900}, device_scale_factor=RASTER_SCALE)
                        except Exception as error:
                            browser_error = "Real browser unavailable: " + str(error)
                            raise ValueError(browser_error) from error
                    from comparison.browser import compare_browser
                    result = compare_browser(page, expected, actual, item["family"])
                results.append({"id": item["id"], "family": item["family"], "ok": True, **result})
            except Exception as error:
                results.append({"id": item["id"], "family": item["family"], "ok": False, "error": str(error)})
            row = results[-1]
            print(("PASS " if row["ok"] else "FAIL ") + row["id"] + ("" if row["ok"] else ": " + row["error"]))
    finally:
        if browser:
            browser.close()
        if playwright:
            playwright.stop()
    passed = sum(row["ok"] for row in results)
    report = {"schema": 1, "stage": "complete-svg-comparison", "upstream": UPSTREAM,
              "geometryTolerance": GEOMETRY_TOLERANCE, "rasterScale": RASTER_SCALE,
              "channelRoundingTolerance": CHANNEL_TOLERANCE, "coverage": summary,
              "corpusHashes": {row["family"] + ".json": row["sha256"] for row in summary},
              "results": results, "total": len(results), "passed": passed,
              "browserCompared": sum(row["ok"] and row.get("rasterCompared", False) for row in results),
              "officialSvgCompared": len(results) == len(records) == passed}
    output = Path(args.output) if args.output else native.directory / "comparison.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{passed}/{len(results)} complete SVG comparisons passed")
    return 0 if report["officialSvgCompared"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
