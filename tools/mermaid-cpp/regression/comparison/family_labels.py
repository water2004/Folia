"""Acceptance of portable labels, boundaries and semantic diagram elements."""

from __future__ import annotations

import re

from .corpus import require
from .family_checks import Check, exact, exact_fields, numbers


FAMILIES = {"usecase", "swimlane", "eventmodeling", "ishikawa", "railroad", "zenuml", "info", "error"}


def paths(check, actual, expected, location):
    for a, e, path in check.pairs(actual, expected, "paths", location):
        exact_fields(a, e, ("paint",), path)
        check.path(a["d"], e["d"], path + ".d")


def usecase(actual, expected, check):
    name = expected["name"]
    check.fields(actual["view"], expected["view"], ("x", "y", "width", "height"), name + ".view", 1.5)
    for a, e, path in check.pairs(actual, expected, "labels", name):
        check.fields(a, e, ("text", "size", "fill"), path)
        check.fields(a["box"], e["box"], ("x", "y", "width", "height"), path + ".box", 1.5)
        if e.get("glyphBox"):
            require(bool(a.get("glyphBox")), path + ": missing painted glyph box")
            check.fields(a["glyphBox"], e["glyphBox"], ("x", "y", "width", "height"), path + ".glyphBox", 1.5)
        if name.startswith("rich-"):
            require(a["tag"] == "text", path + ".tag")
            check.compare(a["portable"], e["tag"] == "foreignObject", path + ".layout mode")
            exact_fields(a, e, ("runs",), path)
    for a, e, path in check.pairs(actual, expected, "nodes", name):
        exact_fields(a, e, ("id", "shape", "paint"), path)
        check.fields(a["box"], e["box"], ("x", "y", "width", "height"), path + ".box", 1.5)
        paths(check, a, e, path)
    for a, e, path in check.pairs(actual, expected, "boundaries", name):
        check.fields(a, e, ("id",), path)
        check.fields(a["box"], e["box"], ("x", "y", "width", "height"), path + ".box", 1.5)
        for av, ev, part_path in check.pairs(a, e, "parts", path):
            check.fields(av["box"], ev["box"], ("x", "y", "width", "height"), part_path + ".box", 1.5)
            exact_fields(av, ev, ("paint",), part_path)
            paths(check, av, ev, part_path)
    for a, e, path in check.pairs(actual, expected, "edges", name):
        check.fields(a, e, ("stroke",), path)
        check.compare(numbers(a["dash"]), numbers(e["dash"]), path + ".dash", 1.5)
        check.path(a["d"], e["d"], path + ".d")
        for side in ("markerStart", "markerEnd"):
            require(bool(a[side]) == bool(e[side]), path + "." + side)
            if a[side]:
                check.fields(a[side], e[side], ("shape", "r"), path + "." + side)
                check.fields(a[side], e[side], ("width", "height", "refX", "refY"), path + "." + side, 1.5)
                check.path(a[side]["d"], e[side]["d"], path + "." + side)


def swimlane(actual, expected, check):
    name = expected["name"]
    check.compare(actual["view"], expected["view"], name + ".view", 1.5)
    a, e = dict(actual), dict(expected)
    a["labels"], e["labels"] = sorted(a["labels"], key=lambda v: v["text"]), sorted(e["labels"], key=lambda v: v["text"])
    for av, ev, path in check.pairs(a, e, "labels", name):
        check.fields(av, ev, ("text", "fontSize", "fontFamily", "color", "fontWeight", "stroke"), path)
        check.fields(av, ev, ("box", "glyphBox"), path, 1.5)
    check.fields(actual, expected, ("lanes", "nodes"), name, 1.5)
    check.fields(actual, expected, ("edgeLabelBackgrounds",), name)
    for av, ev, path in check.pairs(actual, expected, "edges", name):
        check.fields(av, ev, ("stroke", "strokeWidth", "markerFill"), path)
        dash = lambda value: "solid" if value in ("none", "0", "0px") else value
        check.compare(dash(av["dasharray"]), dash(ev["dasharray"]), path + ".dasharray")
        command = lambda value: re.findall(r"[MmZzLlHhVvCcSsQqTtAa]", value)
        check.compare(command(av["d"]), command(ev["d"]), path + ".commands")
        check.compare(numbers(av["d"]), numbers(ev["d"]), path + ".geometry", 1.5)


def eventmodeling(actual, expected, check):
    name = expected["name"]
    check.compare(actual["view"], expected["view"], name + ".view", 1.5)
    check.fields(actual, expected, ("arrowhead",), name)
    for category in ("rects", "routes", "labels"):
        for a, e, path in check.pairs(actual, expected, category, name):
            check.compare(a.get("box", a.get("points")), e.get("box", e.get("points")), path + ".geometry", 1.5)
            exact({k: v for k, v in a.items() if k not in ("box", "points")},
                  {k: v for k, v in e.items() if k not in ("box", "points")}, path)
    for a, e, path in check.pairs(actual, expected, "payloads", name):
        check.compare(a["box"], e["box"], path + ".box", 1.5)
        for av, ev, run_path in check.pairs(a, e, "runs", path):
            exact({k: v for k, v in av.items() if k != "boxes"},
                  {k: v for k, v in ev.items() if k != "boxes"}, run_path + ".paint")
            check.compare(av["boxes"], ev["boxes"], run_path + ".boxes", 1.5)


def ishikawa(actual, expected, check):
    name = expected["name"]
    check.compare(actual["view"], expected["view"], name + ".view", 1.5)
    for a, e, path in check.pairs(actual, expected, "lines", name):
        check.fields(a, e, ("cls", "marker"), path)
        check.fields(a, e, ("x1", "y1", "x2", "y2"), path, 1.5)
    for a, e, path in check.pairs(actual, expected, "labels", name):
        check.fields(a, e, ("text", "cls", "fontSize"), path)
        check.fields(a, e, ("box",), path, 1.5)
    for a, e, path in check.pairs(actual, expected, "shapes", name):
        check.fields(a, e, ("cls", "fill", "stroke"), path)
        check.fields(a, e, ("box",), path, 1.5)


def railroad(actual, expected, check):
    name = expected["name"]
    check.fields(actual, expected, ("view",), name, 1.5)
    require(bool(actual["sizing"]) and bool(expected["sizing"]), name + ".missing intrinsic size")
    check.compare(actual["sizing"][0], expected["sizing"][0], name + ".size mode")
    check.compare(actual["sizing"][1:], expected["sizing"][1:], name + ".intrinsic size", 1.5)
    exact_fields(actual, expected, ("accessibility",), name)
    for a, e, path in check.pairs(actual, expected, "shapes", name):
        exact_fields(a, e, ("identity", "style", "commands"), path)
        check.fields(a, e, ("bounds", "numeric"), path, 1.5)


def zenuml(actual, expected, check):
    def compare(a, e, path, tolerance=0):
        if isinstance(e, dict):
            require(isinstance(a, dict), path + " object")
            for key, value in e.items():
                if key == "name":
                    continue
                compare(a.get(key), value, path + "." + key, 1.5 if key in ("box", "view") else 0)
        elif isinstance(e, list):
            require(isinstance(a, list) and len(a) == len(e), path + " count")
            for index, value in enumerate(e):
                compare(a[index], value, path + f".{index}", tolerance)
        else:
            check.compare(a, e, path, tolerance)
    compare(actual, expected, expected["name"])


def info(actual, expected, check):
    name = expected["name"]
    exact_fields(actual, expected, ("text", "view", "width", "height", "style"), name)
    for key in ("viewport", "box"):
        check.fields(actual[key], expected[key], ("x", "y", "width", "height"), name + "." + key, 1.5)
    check.fields(actual["font"], expected["font"], ("size", "anchor", "fill"), name + ".font")


def error(actual, expected, check):
    name = expected["name"]
    exact({k: v for k, v in actual.items() if k not in ("paths", "labels")},
          {k: v for k, v in expected.items() if k not in ("paths", "labels", "name", "theme")}, name + ".viewport")
    for key in ("paths", "labels"):
        for a, e, path in check.pairs(actual, expected, key, name):
            exact({k: v for k, v in a.items() if k != "box"}, {k: v for k, v in e.items() if k != "box"}, path + ".paint")
            check.compare(a["box"], e["box"], path + ".box", 1.5)


def compare_family(family, actual, expected, check: Check, oracle=False):
    policies = {"usecase": usecase, "swimlane": swimlane, "eventmodeling": eventmodeling,
                "ishikawa": ishikawa, "railroad": railroad, "zenuml": zenuml, "info": info, "error": error}
    policies[family](actual, expected, check)
