"""Coverage, source identity and offline acceptance for real Skia observations."""

from __future__ import annotations

import json
import math
from collections import Counter
from pathlib import Path

from lxml import etree

from .corpus import digest, load_corpus, require
from .exact_pixels import exact_pixels
from .observation_process import encoded, sample_file


BACKEND = {"name": "@napi-rs/canvas", "version": "1.1.1", "engine": "Skia",
           "browser": False, "estimatedWidths": False}
JS_SPACE = "\u0009\u000a\u000b\u000c\u000d\u0020\u00a0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200a\u2028\u2029\u202f\u205f\u3000\ufeff"


def verify_backend(backend):
    for key, value in BACKEND.items():
        require(type(backend.get(key)) is type(value) and backend[key] == value,
                "Unexpected Canvas backend " + key)


def official_records(directory):
    report = json.loads((directory / "official.json").read_text(encoding="utf-8"))
    require(report["upstream"]["package"] == "@zenuml/core" and report["upstream"]["version"] == "3.49.2" and
            report["upstream"]["gitHead"] == "213c239587ffa1f3dbb54d8b64468c3038ced55d", "Wrong ZenUML source version")
    verify_backend(report["backend"])
    corpus, summary = load_corpus()
    cases = [item for item in corpus if item["family"] == "zenuml"]
    require([row["id"] for row in report["cases"]] == [row["id"] for row in cases],
            "Every current ZenUML regression requires official SVG evidence")
    for row, case in zip(report["cases"], cases):
        require(row["source"] == case["source"] and row["sourceSha256"] == digest(row["source"]),
                row["id"] + ": stale official source")
        require(not case.get("config"), row["id"] + ": standalone ZenUML has no Mermaid configuration adapter")
    return report, next(row["sha256"] for row in summary if row["family"] == "zenuml")


def labels(svg):
    root = etree.fromstring(svg.encode("utf-8"), etree.XMLParser(resolve_entities=False, no_network=True))
    texts = ["".join(node.itertext()).strip(JS_SPACE) for node in root.iter()
             if isinstance(node.tag, str) and etree.QName(node).localname == "text"]
    # Empty catch-condition placeholders have no displayed text in the upstream SVG.
    return sorted((text for text in texts if text), key=lambda text: text.encode("utf-16-be", errors="surrogatepass"))


def compare_case(reference, native, official_svg, native_svg, expected_pixels, actual_pixels):
    for value in (reference["width"], reference["height"], native["width"], native["height"]):
        require(type(value) in (int, float) and math.isfinite(value), "Non-finite Canvas viewport")
    width_difference, height_difference = abs(reference["width"] - native["width"]), abs(reference["height"] - native["height"])
    expected_labels, actual_labels = labels(official_svg), labels(native_svg)
    pixels = exact_pixels(actual_pixels, expected_pixels)
    same_labels = expected_labels == actual_labels
    return {"ok": width_difference <= 1.5 and height_difference <= 1.5 and same_labels and pixels["differentPixels"] == 0,
            "native": {key: native[key] for key in ("width", "height")},
            "official": {key: reference[key] for key in ("width", "height")},
            "widthDifference": width_difference, "heightDifference": height_difference,
            "sameLabels": same_labels, "expectedLabels": expected_labels, "actualLabels": actual_labels,
            **pixels, "identicalRaster": pixels["differentPixels"] == 0}


def compare_observations(official_directory, observation_path):
    official, corpus_hash = official_records(official_directory)
    document = json.loads(observation_path.read_text(encoding="utf-8"))
    require(document["schema"] == 1 and document["stage"] == "zenuml-canvas-observations", "Wrong observation schema")
    require(document["officialSha256"] == digest((official_directory / "official.json").read_bytes()), "Official evidence changed")
    require(document["corpusSha256"] == corpus_hash, "ZenUML corpus changed since capture")
    verify_backend(document["backend"])
    require([row["id"] for row in document["records"]] == [row["id"] for row in official["cases"]],
            "Native observations must cover every official case in order")
    directory = observation_path.parent
    sample_directory = directory / "samples"
    log = (sample_directory / "samples.jsonl").read_bytes()
    require(digest(log) == document["samplesSha256"], "Oracle observation log changed")
    measurements, samples, profiles = Counter(), {}, 0
    case_ids = {row["id"] for row in official["cases"]}
    for line in log.splitlines():
        entry = json.loads(line)
        request, sample = entry["request"], entry["sample"]
        require(sample["schema"] == 1 and sample["inputSha256"] == digest(encoded(request)), "Oracle request provenance mismatch")
        verify_backend(sample["backend"])
        require(sample["kind"] == request["kind"], "Oracle response kind mismatch")
        if request["kind"] == "profile":
            profiles += 1
            require(entry["case"] is None and sample["availableFonts"] == official["backend"]["availableFonts"],
                    "Canvas observations use different fonts from the official capture")
        else:
            require(entry["case"] in case_ids, "Canvas sample belongs to an unknown case")
        samples[(entry["case"], sample["inputSha256"])] = entry
        if request["kind"] == "text":
            require(sample["text"] == request["text"], "Oracle measured different text")
            require(set(sample["metrics"]) == {"advance", "ink_width", "bounds_width", "line_height", "baseline"},
                    "Incomplete actual Canvas metrics")
            require(all(type(v) in (int, float) and math.isfinite(v) for v in sample["metrics"].values()) and
                    sample["metrics"]["advance"] >= 0, "Invalid real Canvas metrics")
            measurements[entry["case"]] += 1
    require(profiles == 1, "Canvas observations require one actual font-backend profile")
    result = []
    for reference, observed in zip(official["cases"], document["records"]):
        ident, native = reference["id"], observed["native"]
        if not native["ok"]:
            result.append({"id": ident, "ok": False, "nativeOk": False, "error": native.get("error"), "identicalRaster": False})
            continue
        require(measurements[ident] > 0 and measurements[ident] == observed["fontRequests"],
                ident + ": real remote Canvas measurements are mandatory")
        official_svg = (official_directory / reference["svg"]).read_text(encoding="utf-8")
        native_svg = (directory / observed["svg"]).read_text(encoding="utf-8")
        require(digest(official_svg) == reference["svgSha256"] and digest(native_svg) == observed["svgSha256"],
                ident + ": SVG evidence changed")
        width = math.ceil(max(reference["width"], native["width"]))
        height = math.ceil(max(reference["height"], native["height"]))
        rasters = []
        for key, source in (("officialRaster", official_svg), ("nativeRaster", native_svg)):
            sample = observed[key]
            entry = samples.get((ident, sample["inputSha256"]))
            require(entry is not None and entry["sample"] == sample, ident + ": raster absent from capture log")
            require(entry["request"] == {"kind": "svg", "source": source, "width": width, "height": height},
                    ident + ": wrong common raster canvas or SVG")
            require(sample["sourceSha256"] == digest(source) and sample["width"] == width and sample["height"] == height and sample["channels"] == 4,
                    ident + ": raster metadata mismatch")
            rgba = sample_file(sample_directory, sample, "rgba")
            require(len(rgba) == width * height * 4, ident + ": incomplete RGBA observation")
            sample_file(sample_directory, sample, "png")
            rasters.append(rgba)
        result.append({"id": ident, "sourceSha256": reference["sourceSha256"],
                       "officialSvgSha256": reference["svgSha256"], "nativeSvgSha256": observed["svgSha256"],
                       **compare_case(reference, native, official_svg, native_svg, *rasters)})
    return {"stage": "zenuml-complete-canvas-svg", "upstream": official["upstream"],
            "corpusSha256": corpus_hash, "nativeProbeSha256": document["nativeProbeSha256"],
            "observationsSha256": digest(observation_path.read_bytes()), "backend": "@napi-rs/canvas 1.1.1 (Skia)",
            "browser": False, "measurementSubstitution": False, "nativeFontRequests": sum(measurements.values()),
            "geometryTolerance": 1.5, "rasterBackground": "transparent", "comparedChannels": "RGBA",
            "identicalRaster": sum(row["identicalRaster"] for row in result),
            "passed": sum(row["ok"] for row in result), "total": len(result), "results": result}
