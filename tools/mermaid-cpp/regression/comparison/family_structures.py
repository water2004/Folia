"""Compound, tree and force-layout diagram acceptance."""

from __future__ import annotations

import math

from .corpus import require
from .family_checks import Check, numbers


FAMILIES = {"agentflow", "architecture", "tree-view", "mindmap"}


def agentflow(actual, expected, check):
    name = expected["name"]
    for category in ("clusters", "nodes", "labels"):
        for a, e, path in check.pairs(actual, expected, category, name):
            check.fields(a, e, ("text",) if category == "labels" else ("label",), path)
            require(a.get("box") is not None, path + ": no shape geometry")
            check.fields(a["box"], e["box"], ("x", "y", "width", "height"), path + ".box", 1.5)
            if category == "nodes":
                if a.get("labelCenter") and e.get("labelCenter"):
                    check.fields(a["labelCenter"], e["labelCenter"], ("x", "y"), path + ".labelCenter", 1.5)
                if a["label"] == "Style guide":
                    check.compare(a["shapeSamples"], e["shapeSamples"], path + ".shapeSamples", 1.5)
    check.fields(actual["view"], expected["view"], ("x", "y", "width", "height"), name + ".view", 1.5)
    for a, e, path in check.pairs(actual, expected, "edges", name):
        require(len(a["points"]) >= 2, path + ": no routed endpoints")
        check.fields(a, e, ("marker",), path)
        check.compare(a["samples"], e["samples"], path + ".samples", 1.5)


def architecture(actual, expected, check):
    name = expected["name"]
    check.fields(actual, expected, ("view",), name, 1.5)
    check.compare(actual["services"], [{"position": e["position"], "box": e["box"]} for e in expected["services"]], name + ".services", 1.5)
    for a, e, path in check.pairs(actual, expected, "groups", name):
        check.fields(a, e, ("box",), path, 1.5)
        check.fields(a, e, ("stroke", "width"), path)
    for a, e, path in check.pairs(actual, expected, "edges", name):
        check.path(a["path"], e["path"], path + ".path")
        check.fields(a, e, ("stroke", "width"), path)
    for a, e, path in check.pairs(actual, expected, "arrows", name):
        check.path(a["points"], e["points"], path + ".points")
        check.path(a["transform"], e["transform"], path + ".transform")
        check.fields(a, e, ("fill",), path)
    for a, e, path in check.pairs(actual, expected, "labels", name):
        check.fields(a, e, ("text", "font", "size", "fill"), path)
        check.fields(a, e, ("global",), path, 1.5)
    for a, e, path in check.pairs(actual, expected, "iconTexts", name):
        check.fields(a, e, ("text", "fill"), path)
        check.fields(a, e, ("global",), path, 1.5)


def tree_view(actual, expected, check):
    name = expected["name"]
    check.fields(actual, expected, ("labels", "lines", "highlights", "view"), name, 1.5)
    check.fields(actual, expected, ("icons",), name)


def basis_route(source, target):
    """Three-point d3-shape 3.2.0 curveBasis, rounded like d3-path 3.1.0.

    The route clips the two 30 px Cytoscape ellipses and uses the midpoint.
    This geometric contract is independent of the native C++ implementation.
    """
    dx, dy = target["x"] - source["x"], target["y"] - source["y"]
    length = math.hypot(dx, dy)
    require(length > 0, "Mindmap nonempty edge")
    a = (source["x"] + 15 * dx / length, source["y"] + 15 * dy / length)
    b = ((source["x"] + target["x"]) / 2, (source["y"] + target["y"]) / 2)
    c = (target["x"] - 15 * dx / length, target["y"] - 15 * dy / length)
    mix = lambda x, y, wx, wy, denominator: tuple((wx * x[i] + wy * y[i]) / denominator for i in (0, 1))
    points = [a, mix(a, b, 5, 1, 6), mix(a, b, 2, 1, 3), mix(a, b, 1, 2, 3),
              tuple((a[i] + 4 * b[i] + c[i]) / 6 for i in (0, 1)),
              mix(b, c, 2, 1, 3), mix(b, c, 1, 2, 3), mix(b, c, 1, 5, 6), c]
    return [math.floor(value * 1000 + .5) / 1000 for point in points for value in point]


def mindmap(actual, expected, check):
    name, measured = expected["name"], expected.get("_measured", False)
    if measured:
        check.fields(actual["view"], expected["view"], ("x", "y", "width", "height"), name + ".view", 1e-4)
    for key in ("x", "y"):
        check.compare(actual["view"][key], actual["bounds"][key] - 10, name + ".padding." + key, 1e-4)
    for key in ("width", "height"):
        check.compare(actual["view"][key], actual["bounds"][key] + 20, name + ".extent." + key, 1e-4)
    for a, e, path in check.pairs(actual, expected, "nodes", name):
        if measured:
            check.fields(a["position"], e["position"], ("x", "y"), path + ".position", 1e-4)
        check.fields(a["bounds"], e["nodeBox"], ("x", "y", "width", "height"), path + ".bounds", 1.5)
        for actual_key, expected_key in (("fill", "fill"), ("color", "textColor"), ("text", "labelText")):
            check.compare(a[actual_key], e[expected_key], path + "." + actual_key)
    for index, (a, e, path) in enumerate(check.pairs(actual, expected, "edges", name)):
        check.fields(a, e, ("stroke", "width"), path)
        link = expected["links"][index]
        check.compare(a["id"], link["id"], path + ".identity")
        source, target = (actual["nodes"][int(link[key])]["position"] for key in ("source", "target"))
        reference = numbers(e["path"]) if measured else basis_route(source, target)
        check.compare(numbers(a["path"]), reference, path + ".coordinates", .001)


def compare_family(family, actual, expected, check: Check, oracle=False):
    {"agentflow": agentflow, "architecture": architecture, "tree-view": tree_view, "mindmap": mindmap}[family](actual, expected, check)
