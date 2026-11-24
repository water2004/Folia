"""Flowchart geometry, route, marker, paint and rich-text acceptance."""

from __future__ import annotations

import re

import numpy as np

from .corpus import require
from .family_checks import Check, exact, exact_fields, numbers


FAMILIES = {"flowchart"}


def translate(value):
    return [float(x) for x in re.split(r"[ ,]+", value[value.index("(") + 1:value.index(")")].strip())]


def dash_paint(value):
    return "none" if re.fullmatch(r"(?:0(?:px)?)(?:[,\s]+0(?:px)?)*", value) else value


def sketch_paths(actual, expected, check, location, document=False):
    require(actual is not None and len(actual) == len(expected), location + " path count")
    for index, (a, e) in enumerate(zip(actual, expected)):
        path = location + f".{index}"
        check.compare(a["length"], e["length"], path + ".length", 1.5)
        exact_fields(a, e, ("stroke", "width", "fill", "opacity") if document else ("stroke", "width"), path)
        check.compare(a["samples"], e["samples"], path + ".samples", 1.5)


def outline(actual, expected, tolerance, location):
    require(bool(actual) and bool(expected), location + " missing shape outline")
    for source, target in ((actual, expected), (expected, actual)):
        target = np.array([[v["x"], v["y"]] for v in target])
        source = np.array([[v["x"], v["y"]] for v in source])
        for start in range(0, len(source), 256):
            distances = np.sqrt(((source[start:start + 256, None, :] - target[None, :, :]) ** 2).sum(axis=2))
            require(float(distances.min(axis=1).max()) <= tolerance, location + " outline geometry")


def compare_family(family, actual, expected, check: Check, oracle=False):
    name, offset = expected["name"], translate(actual["offset"])
    bounds, view = actual["bounds"], actual["view"]
    require(bounds["x"] + offset[0] >= view["x"] - 1 and bounds["y"] + offset[1] >= view["y"] - 1 and
            bounds["x"] + bounds["width"] + offset[0] <= view["x"] + view["width"] + 1 and
            bounds["y"] + bounds["height"] + offset[1] <= view["y"] + view["height"] + 1, name + ": clipped SVG content")
    require(bool(actual.get("diagramTitle")) == bool(expected.get("diagramTitle")), name + " diagram title")
    if re.match(r"^(hand-drawn|extra-shapes|elk-painted)-", name) or expected.get("diagramTitle"):
        check.fields(actual["view"], expected["view"], ("x", "y", "width", "height"), name + ".view", 1.5)
    if expected.get("diagramTitle"):
        check.fields(actual["diagramTitle"], expected["diagramTitle"], ("text", "fill", "size"), name + ".title")
        check.fields(actual["diagramTitle"], expected["diagramTitle"], ("x", "y", "width", "height"), name + ".title", 1.5)
    require(len(actual["nodes"]) == len(expected["nodes"]), name + " nodes")
    for index, a in enumerate(actual["nodes"]):
        e = next((e for e in expected["nodes"] if e["label"] == a["label"]), None)
        require(e is not None, name + ": missing node " + a["label"])
        path = name + f".node.{index}"
        p, q = translate(a["transform"]), [e["x"], e["y"]]
        if e.get("sketchPaths") == []:
            require(a.get("sketchPaths") == [], path + " shape painted for label-only node")
        else:
            require(a["contained"], path + " clipped label")
        if name.startswith(("labels-", "fonts-", "rich-")):
            exact(a["lines"], e["lines"], path + ".lines")
            check.fields(a["labelCenter"], e["labelCenter"], ("x", "y"), path + ".labelCenter", 2)
        check.fields(a, e, ("width", "height"), path, 1.5)
        check.compare([p[i] + offset[i] for i in (0, 1)], q, path + ".position", 1.5)
        exact_fields(a, e, ("font", "characters"), path)
        if "-document-" in name:
            exact_fields(a, e, ("invisibleStrokes",), path)
        exact_fields(a, e, ("fill", "stroke", "thickness", "color"), path)
        exact(dash_paint(a["dash"]), dash_paint(e["dash"]), path + ".dash")
        if name.startswith("extra-shapes-dashed-"):
            exact(a["dashSegments"], e["dashSegments"], path + ".dash phase resets")
        if e.get("sketchPaths") is not None:
            check.fields(a["labelCenter"], e["labelCenter"], ("x", "y"), path + ".text", 1.5)
            sketch_paths(a.get("sketchPaths"), e["sketchPaths"], check, path, "-document-" in name)
        if name.startswith("neo-"):
            exact_fields(a, e, ("filter",), path)
        if e["outline"]:
            tolerance = 1.5 if name.startswith(("extra-shapes-", "neo-document-")) else 2
            check.fields(a["labelCenter"], e["labelCenter"], ("x", "y"), path + ".labelCenter", tolerance)
            outline(a["outline"], e["outline"], tolerance, path)
    require(len(actual["labels"]) == len(expected["labels"]), name + " labels")
    remaining = list(expected["labels"])
    for index, a in enumerate(actual["labels"]):
        found = next((i for i, e in enumerate(remaining) if e["text"] == a["text"]), None)
        require(found is not None, name + ": missing label " + a["text"])
        e, path = remaining.pop(found), name + f".label.{index}"
        check.compare([a["x"] + offset[0], a["y"] + offset[1]], [e["x"], e["y"]], path + ".position", 1.5)
        exact_fields(a, e, ("color", "font", "characters", "lines"), path)
        check.fields(a, e, ("width", "height"), path, 1.5)
    require(len(actual["edges"]) == len(expected["edges"]), name + " edges")
    edge_tolerance = 1.5 if re.match(r"^(hand-drawn|curve|extra-shapes|neo-document|elk-painted)-", name) else 3
    for index, a in enumerate(actual["edges"]):
        e = next((e for e in expected["edges"] if e["id"].endswith("-" + a["id"])), None)
        require(e is not None, name + ": missing edge " + a["id"])
        path = name + f".edge.{index}"
        check.compare(a["length"], e["length"], path + ".length", edge_tolerance)
        exact_fields(a, e, ("stroke", "thickness"), path)
        if name.startswith("neo-") and a["dash"] != "none" and e["dash"] != "none":
            check.compare(numbers(a["dash"]), numbers(e["dash"]), path + ".dash", 1.5)
        else:
            exact_fields(a, e, ("dash",), path)
        for side in ("start", "end"):
            require(bool(a[side]) == bool(e[side]), path + "." + side + " marker")
            if e[side]:
                exact_fields(a[side], e[side], ("attributes", "fill", "stroke", "width", "dash"), path + "." + side)
                check.compare(a[side]["points"], e[side]["points"], path + "." + side + ".outline", .01)
        shifted = [{"x": p["x"] + offset[0], "y": p["y"] + offset[1]} for p in a["points"]]
        check.compare(shifted, e["points"], path + ".points", edge_tolerance)
    require(len(actual["clusters"]) == len(expected["clusters"]), name + " groups")
    for index, a in enumerate(actual["clusters"]):
        e = next((e for e in expected["clusters"] if e["id"].endswith("-" + a["id"])), None)
        require(e is not None, name + ": missing group " + a["id"])
        path = name + f".group.{index}"
        exact_fields(a, e, ("label", "characters", "lines", "font", "fill", "stroke", "thickness", "dash", "color"), path)
        check.fields(a, e, ("x", "y", "width", "height"), path, 1.5)
        check.fields(a["title"], e["title"], ("x",), path + ".title", 1.5)
        check.fields(a["title"], e["title"], ("y",), path + ".title", 1.5 if e.get("sketchPaths") is not None else 2)
        if e.get("sketchPaths") is not None:
            sketch_paths(a.get("sketchPaths"), e["sketchPaths"], check, path)
