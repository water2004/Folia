"""Same-host paint acceptance, including every feature-removal negative control."""

from copy import deepcopy
import re

from comparison.corpus import digest
from paint_dom import canvas, finite, materialize, paint_checks, require, serialize, target_nodes
from paint_samples import difference, ink


def compare_part(item, name, source_root, actual_root, metrics, styles, raster, destination):
    require(all(finite(value) for value in metrics.values()) and metrics["width"] > 0 and metrics["height"] > 0,
            "Paint viewport metrics must be finite and positive")
    require(re.fullmatch(r"[A-Za-z0-9_-]+", name) is not None, "Invalid paint sample name")
    source, actual = serialize(source_root), serialize(actual_root)
    for key in ("viewBox", "data-mjx-viewBox"):
        def view(root):
            text = root.get(key)
            return [float(value) for value in text.strip().split()] if text is not None else None
        source_view = view(source_root)
        require(source_view is None or len(source_view) == 4 and all(finite(value) for value in source_view), "Invalid Source " + key)
        require(view(actual_root) == source_view, "Native " + key + " differs from upstream")
    views = {"official": canvas(source_root, metrics, styles["css"]), "official-bare": canvas(source_root, metrics),
             "official-explicit": canvas(materialize(source_root, styles["rules"]), metrics), "native": canvas(actual_root, metrics)}
    for suffix, svg in views.items():
        (destination / f"{name}-{suffix}.svg").write_text(svg, encoding="utf-8", newline="")
    a = raster.raster(views["official"], name, destination / f"{name}-official.png")
    b = raster.raster(views["native"], name, destination / f"{name}-native.png")
    bare = raster.raster(views["official-bare"], name, destination / f"{name}-official-bare.png")
    explicit = raster.raster(views["official-explicit"], name)
    row = {"name": name, "accepted": False, "metrics": metrics, "raster": difference(a, b),
           "sourceCssEffect": difference(a, bare), "sourceProjection": difference(a, explicit),
           "inkPixels": {"official": ink(a), "native": ink(b)}, "paint": paint_checks(actual_root, item), "features": []}
    for feature in item["features"]:
        if feature in ("invisible", "dashed", "dotted", "undefined-command"):
            continue
        source_nodes, native_nodes = target_nodes(source_root, feature), target_nodes(actual_root, feature)
        require(len(native_nodes) == len(source_nodes), feature + " node count differs")
        require(bool(source_nodes), feature + " has no source primitives")
        count = len(source_nodes) if feature in ("table-lines", "table-frame", "table-viewport") else 1
        for index in range(count):
            without_source, without_native = deepcopy(source_root), deepcopy(actual_root)
            for root in (without_source, without_native):
                node = target_nodes(root, feature)[index]
                node.getparent().remove(node)
            removed_a = raster.raster(canvas(without_source, metrics, styles["css"]), name)
            removed_b = raster.raster(canvas(without_native, metrics), name)
            official_pixels, native_pixels = difference(a, removed_a)["differentPixels"], difference(b, removed_b)["differentPixels"]
            row["features"].append({"feature": feature, "index": index, "officialPixels": official_pixels,
                                    "nativePixels": native_pixels, "passed": official_pixels > 0 and native_pixels == official_pixels})
    if "invisible" in item["features"]:
        row["features"].append({"feature": "invisible", "passed": row["inkPixels"]["official"] == row["inkPixels"]["native"] == 0})
    row["accepted"] = (row["raster"]["differentPixels"] == row["sourceProjection"]["differentPixels"] == 0
                       and all(check["passed"] for check in [*row["paint"], *row["features"]]))
    row["hashes"] = {"source": digest(source), "sourceCss": digest(styles["css"]), "native": digest(actual),
                     "officialPng": a["pngSha256"], "nativePng": b["pngSha256"]}
    return row


def summarize_parts(row):
    row["accepted"] = bool(row["parts"]) and all(part["accepted"] for part in row["parts"])
    for key in ("raster", "sourceProjection", "sourceCssEffect"):
        row[key] = {"differentPixels": sum(part[key]["differentPixels"] for part in row["parts"]),
                    "maximumChannelDifference": max(part[key]["maximumChannelDifference"] for part in row["parts"])}


def failure(row, error):
    row["accepted"] = False
    row["error"] = {"name": type(error).__name__, "message": str(error)}
