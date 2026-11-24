#!/usr/bin/env python3
"""Check actual upstream CSS against CSS-free native SVG, with exact RGBA."""

import argparse
import json
from pathlib import Path
import sys

DIRECTORY = Path(__file__).resolve().parent
ROOT = DIRECTORY.parents[2]
sys.path.insert(0, str(ROOT / "tools/mermaid-cpp/regression"))

from paint_compare import compare_part, failure, summarize_parts
from paint_dom import dimensions, document_roots, finite, parse, require, target_nodes, validate_styles
from paint_records import file_hash, load_inputs, load_mathml, verify_source_lock
from paint_samples import RasterSamples
from comparison.corpus import digest


def tex_record(item, mode, expected, actual, destination, raster):
    name = actual["name"]
    row = {"name": name, "case": item["name"], "mode": mode, "accepted": False, "parts": []}
    try:
        require(not expected.get("error"), "Official capture failed: " + str(expected.get("error")))
        require(type(actual["status"]) is int and actual["status"] == 0 and not actual.get("error") and bool(actual.get("svg")),
                actual.get("error") or "Native render failed")
        require(digest(actual["svg"]) == actual["svgSha256"], "Native SVG bytes changed after capture")
        roots, actual_roots = [parse(svg) for svg in expected["roots"]], document_roots(actual["svg"])
        require(bool(roots), "Source has no direct SVG roots")
        require(len(actual_roots) == len(roots), "Native and Source SVG fragment counts differ")
        require(mode != "fragment" or len(roots) == 1, "Standalone export must have one SVG root")
        require(bool(expected["rootBox"]), "Missing Source root box")
        styles = expected["styles"]
        validate_styles(styles)
        (destination / (name + "-official-original.xml")).write_text(expected["original"], encoding="utf-8", newline="")
        (destination / (name + "-native-original.xml")).write_text(actual["svg"], encoding="utf-8", newline="")
        for index, (source_root, actual_root) in enumerate(zip(roots, actual_roots)):
            metrics = dimensions(item, source_root, expected if mode == "fragment" else None)
            if mode == "fragment":
                require(isinstance(actual.get("metrics"), dict), "Fragment metrics missing")
                for key in ("width", "height", "baseline"):
                    require(finite(actual["metrics"].get(key)) and abs(actual["metrics"][key] - metrics[key]) <= 1e-9,
                            "Fragment " + key + " differs")
            else:
                for key in ("width", "height"):
                    require(actual_root.get(key) == source_root.get(key), "Document " + key + " differs")
            part_name = name if len(roots) == 1 else name + "--part" + str(index + 1)
            row["parts"].append(compare_part(item, part_name, source_root, actual_root, metrics, styles, raster, destination))
        summarize_parts(row)
    except (OSError, ValueError, KeyError, TypeError, ZeroDivisionError) as error:
        failure(row, error)
    return row


def mathml_record(record, styles, destination, raster):
    item = dict(record["item"])
    name = "mathml--" + item["name"]
    row = {"name": name, "case": item["name"], "mode": "direct-mathml", "family": record["family"], "accepted": False, "parts": []}
    try:
        source_roots, actual_roots = document_roots(record["source"]), document_roots(record["native"])
        require(len(source_roots) == len(actual_roots) == 1, "Direct MathML fixture should have one document root")
        source_root, actual_root = source_roots[0], actual_roots[0]
        for key in ("width", "height"):
            require(actual_root.get(key) == source_root.get(key), "Direct MathML " + key + " differs")
        item["features"] = [feature for feature in ("table-lines", "table-frame", "table-viewport", "glyphs")
                            if target_nodes(source_root, feature)] or ["invisible"]
        row["parts"].append(compare_part(item, name, source_root, actual_root, dimensions(item, source_root), styles, raster, destination))
        summarize_parts(row)
        row["capture"] = record["capture"]
    except (OSError, ValueError, KeyError, TypeError, ZeroDivisionError) as error:
        failure(row, error)
    return row


def compare(args):
    corpus, reference, native, inputs = load_inputs(args.input, args.reference, args.native)
    source_lock = verify_source_lock(reference, ROOT)
    mathml, mathml_records = load_mathml(args.mathml, args.mathml_binary, ROOT)
    if mathml:
        inputs["mathml"] = {"metadata": mathml, "records": [row["capture"] for row in mathml_records]}
    destination = args.output.resolve()
    rows = []
    script = ROOT / "tools/mermaid-cpp/math-oracle/capture-raster-samples.mjs"
    with RasterSamples(destination, inputs, script, observations=args.observations, node=args.node) as raster:
        index = 0
        for item in corpus["cases"]:
            for mode in corpus["modes"]:
                row = tex_record(item, mode, reference["records"][index], native["records"][index], destination, raster)
                rows.append(row)
                index += 1
                print(("PASS " if row["accepted"] else "FAIL ") + row["name"] + (": " + row["error"]["message"] if "error" in row else ""))
        if mathml_records:
            styles = reference["records"][-1]["styles"]
            validate_styles(styles)
            for record in mathml_records:
                row = mathml_record(record, styles, destination, raster)
                rows.append(row)
                print(("PASS " if row["accepted"] else "FAIL ") + row["name"] + (": " + row["error"]["message"] if "error" in row else ""))
            require(file_hash(args.mathml_binary) == mathml["binary"]["sha256"], "MathML capture executable changed during comparison")
    styles = next((row["styles"] for row in reference["records"] if "styles" in row), None)
    if styles:
        (destination / "mathjax.css").write_text(styles["css"], encoding="utf-8", newline="")
        (destination / "mathjax-svg-host.css").write_text(styles["css"].replace("mjx-container", "svg[data-mathjax-host]"), encoding="utf-8", newline="")
    report = {"schema": 1, "reference": reference["reference"], "referenceReportSha256": file_hash(args.reference),
              "nativeReportSha256": file_hash(args.native), "nativeBinary": native["binary"],
              "corpusSha256": inputs["corpusSha256"], "sourceLock": source_lock, "rasterizer": raster.backend,
              "comparison": "Exact 2x transparent RGBA, no registration, filtering or tolerance",
              "hostProjection": "Only mjx-container selector tag maps to svg[data-mathjax-host]; all CSS declarations retained",
              "multipartInline": "Every SVG part compared independently; HTML line wrapping and part placement are not tested",
              "mathml": mathml, "browserCompared": False, "windowsUiTested": False,
              "total": len(rows), "accepted": sum(row["accepted"] for row in rows), "records": rows,
              "rasterObservationsSha256": file_hash(raster.observation_path)}
    (destination / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"destination": str(destination), "total": report["total"], "accepted": report["accepted"]}))
    return int(report["accepted"] != report["total"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True, help="Official capture-source.mjs report")
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--input", type=Path, default=DIRECTORY / "cases.json")
    parser.add_argument("--output", type=Path, default=ROOT / "build/math-svg-paint/comparison")
    parser.add_argument("--mathml", type=Path)
    parser.add_argument("--mathml-binary", type=Path)
    parser.add_argument("--observations", type=Path, help="Replay captured raster samples entirely without Node")
    parser.add_argument("--node", default="node", help="Used only to capture missing real Sharp samples")
    return compare(parser.parse_args())


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
