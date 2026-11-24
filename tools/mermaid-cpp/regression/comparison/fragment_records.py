"""Math fragment evidence capture and offline metric/vector/RGBA acceptance."""

from __future__ import annotations

import json
import math

from .corpus import digest, require
from .exact_pixels import exact_pixels
from .math_fragments import canvas, dimensions, first_difference, svg_tree
from .observation_process import SampleCapture, encoded, sample_file


SAMPLE_METADATA = {"schema", "kind", "inputSha256", "backend", "sourceSha256", "rgba", "rgbaSha256", "png", "pngSha256"}


def has_error(record):
    error = record.get("error")
    return isinstance(error, (dict, list)) or bool(error)


def load_reports(reference_path, native_path, corpus_path):
    reference = json.loads(reference_path.read_text(encoding="utf-8"))
    native = json.loads(native_path.read_text(encoding="utf-8"))
    corpus = json.loads(corpus_path.read_text(encoding="utf-8"))
    require(all(type(report["schema"]) is int and report["schema"] == 1 for report in (reference, native, corpus)), "Wrong fragment report schema")
    require(reference["corpusSha256"] == digest(corpus_path.read_bytes()), "Official fragment corpus changed")
    if native.get("corpusSha256"):
        require(native["corpusSha256"] == reference["corpusSha256"], "Native fragment corpus changed")
    names = [case["name"] for case in corpus["cases"]]
    require([case["name"] for case in reference["records"]] == names, "Official fragment coverage/order mismatch")
    require([case["name"] for case in native["records"]] == names, "Native fragment coverage/order mismatch")
    return reference, native


def common_canvas(reference, native):
    dimensions(reference, native)
    width = max(1, math.ceil(max(reference["width"], native["width"]))) + 2
    height = max(1, math.ceil(max(reference["height"], native["height"]))) + 2
    return width, height


def capture_fragments(reference_path, native_path, corpus_path, destination, script, *, node="node"):
    reference, native = load_reports(reference_path, native_path, corpus_path)
    records = []
    with SampleCapture(script, destination / "samples", node=node) as oracle:
        for expected, actual in zip(reference["records"], native["records"]):
            row = {"name": expected["name"]}
            if not has_error(expected) and not has_error(actual):
                width, height = common_canvas(expected, actual)
                for kind, value in (("official", expected), ("native", actual)):
                    row[kind] = oracle.sample({"kind": "svg", "source": canvas(value["svg"], width, height)}, expected["name"])
            records.append(row)
    report = {"schema": 1, "referenceSha256": digest(reference_path.read_bytes()), "nativeSha256": digest(native_path.read_bytes()),
              "corpusSha256": reference["corpusSha256"], "samplesSha256": digest((destination / "samples/samples.jsonl").read_bytes()), "records": records}
    path = destination / "raster-observations.json"
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return path


def compare_fragments(reference_path, native_path, corpus_path, observation_path, destination):
    reference, native = load_reports(reference_path, native_path, corpus_path)
    observations = json.loads(observation_path.read_text(encoding="utf-8"))
    for key, path in (("referenceSha256", reference_path), ("nativeSha256", native_path)):
        require(observations[key] == digest(path.read_bytes()), "Fragment report changed: " + key)
    require(type(observations["schema"]) is int and observations["schema"] == 1 and observations["corpusSha256"] == reference["corpusSha256"], "Wrong fragment observation provenance")
    require([row["name"] for row in observations["records"]] == [row["name"] for row in reference["records"]], "Missing fragment raster observations")
    sample_directory = observation_path.parent / "samples"
    log = (sample_directory / "samples.jsonl").read_bytes()
    require(digest(log) == observations["samplesSha256"], "Fragment raster observation log changed")
    entries, rasterizer = {}, None
    names = {row["name"] for row in reference["records"]}
    for line in log.splitlines():
        entry = json.loads(line)
        request, sample = entry["request"], entry["sample"]
        require(entry["case"] in names and type(sample["schema"]) is int and sample["schema"] == 1 and request["kind"] == sample["kind"] == "svg", "Unknown fragment sample")
        require(sample["inputSha256"] == digest(encoded(request)) and sample["sourceSha256"] == digest(request["source"]), "Fragment sample request changed")
        backend = sample["backend"]
        require(backend["name"] == "sharp" and backend["versions"]["sharp"] == "0.35.5", "Unexpected fragment raster backend")
        require(rasterizer is None or rasterizer == backend["versions"], "Mixed raster backends in fragment observations")
        rasterizer = backend["versions"]
        entries[(entry["case"], sample["inputSha256"])] = entry
    destination.mkdir(parents=True, exist_ok=True)
    results = []
    for expected, actual, observed in zip(reference["records"], native["records"], observations["records"]):
        name = expected["name"]
        if has_error(expected) or has_error(actual):
            results.append({"name": name, "accepted": False, "referenceError": expected.get("error"), "nativeError": actual.get("error")})
            continue
        metrics = dimensions(expected, actual)
        metrics_equal = all(value["difference"] <= 1e-9 for value in metrics.values())
        tree_difference = first_difference(svg_tree(expected["svg"]), svg_tree(actual["svg"]))
        width, height = common_canvas(expected, actual)
        rasters, infos, png_hashes = [], [], []
        for kind, value in (("official", expected), ("native", actual)):
            sample = observed[kind]
            entry = entries.get((name, sample["inputSha256"]))
            require(entry is not None and entry["sample"] == sample and entry["request"] == {"kind": "svg", "source": canvas(value["svg"], width, height)},
                    name + ": raster does not observe the original unmodified SVG")
            require(sample["width"] == width * 2 and sample["height"] == height * 2 and sample["channels"] == 4, name + ": wrong 2x RGBA canvas")
            info = {key: value for key, value in sample.items() if key not in SAMPLE_METADATA}
            infos.append(info)
            rgba, png = sample_file(sample_directory, sample, "rgba"), sample_file(sample_directory, sample, "png")
            require(len(rgba) == width * height * 16, name + ": incomplete fragment pixels")
            rasters.append(rgba)
            png_hashes.append(digest(png))
            (destination / (name + "-" + kind + ".png")).write_bytes(png)
        require(infos[0] == infos[1], name + ": raster image metadata mismatch")
        delta = exact_pixels(rasters[1], rasters[0])
        (destination / (name + "-native.svg")).write_text(actual["svg"], encoding="utf-8", newline="")
        results.append({"name": name, "accepted": metrics_equal and tree_difference is None and delta["differentPixels"] == 0,
                        "metricsEqual": metrics_equal, "dimensions": metrics, "vectorEqual": tree_difference is None, "treeDifference": tree_difference,
                        "raster": {"width": width * 2, "height": height * 2, "differentPixels": delta["differentPixels"],
                                   "maximumChannelDifference": delta["maximumChannelDifference"], "referenceSha256": png_hashes[0], "nativeSha256": png_hashes[1]}})
    return {"schema": 1, "referenceSha256": digest(reference_path.read_bytes()), "nativeSha256": digest(native_path.read_bytes()),
            "corpusSha256": reference["corpusSha256"], "observationsSha256": digest(observation_path.read_bytes()), "rasterizer": rasterizer,
            "metricsToleranceCssPixels": 1e-9, "vectorComparison": "Exact ordered geometry and paint tree after serialization-only normalization",
            "rasterComparison": "Exact RGBA pixels, common 2x SVG canvas, no image alignment or filtering",
            "total": len(results), "accepted": sum(row["accepted"] for row in results), "records": results}
