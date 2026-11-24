"""Policies for chart families with ordered SVG element capture records."""

from __future__ import annotations

import math

from .corpus import require
from .family_checks import MISSING, Check, numbers


FAMILIES = {
    "block", "c4", "cynefin", "gantt", "git-graph", "journey", "kanban", "quadrant",
    "radar", "sankey", "timeline", "treemap", "venn", "wardley", "xy",
}


def compare_family(family, actual, expected, check: Check, oracle=False):
    name = expected["name"]
    if family in ("radar", "quadrant"):
        keys = ["view", "shapes", "texts"] if family == "radar" else ["view", "rects", "lines", "circles", "texts"]
        check.fields(actual, expected, keys, name, .05)
        return
    if family == "venn":
        check.compare(actual, {k: v for k, v in expected.items() if k not in ("name", "svg")}, name, 1.5)
        return
    if family == "wardley":
        wardley(actual, {k: v for k, v in expected.items() if k not in ("name", "svg")}, name, check)
        return
    if family == "sankey":
        def normalized(row):
            return {"view": row["view"], "elements": [{**e, "path": numbers(e["path"]) if e.get("path") else None}
                                                        for e in row["elements"]]}
        check.compare(normalized(actual), normalized(expected), name, .05)
        return
    if family == "xy":
        xy(actual, expected, name, check, oracle)
        return
    view_tolerance = .05 if family in ("gantt", "treemap") else 1.5
    check.compare(actual["view"], expected["view"], name + ".view", view_tolerance)
    if family == "cynefin":
        for category in ("paths", "rects", "labels"):
            for a, e, path in check.pairs(actual, expected, category, name):
                if category == "paths":
                    check.fields(a, e, ("cls", "fill", "stroke"), path)
                    check.fields(a, e, ("width", "box"), path, 1.5)
                    check.compare(numbers(a["d"]), numbers(e["d"]), path + ".path", 1.5)
                else:
                    check.compare(a, e, path, 1.5)
        return
    if family in ("block", "kanban", "journey"):
        for a, e, path in check.pairs(actual, expected, "shapes", name):
            check.fields(a, e, ("type", "cls", "path", "paint") if family == "journey" else ("type", "paint"), path)
            check.compare(a["box"], e["box"], path + ".box", 1.5)
            if family != "journey":
                for key in ("path", "points"):
                    check.token_path(a[key], e[key], path + "." + key)
        for a, e, path in check.pairs(actual, expected, "labels", name):
            check.fields(a, e, ("text", "font"), path)
            check.compare(a["box"], e["box"], path + ".box", 1.5)
        return
    for a, e, path in check.pairs(actual, expected, "elements", name):
        keys = ["type", "text", "font"] if family == "c4" else ["type", "cls", "text"]
        if family not in ("c4", "treemap"):
            keys.append("font")
        if family not in ("c4", "git-graph"):
            keys.append("paint")
        check.fields(a, e, keys, path)
        box_tolerance = 1.5 if e.get("type") == "text" else view_tolerance
        check.compare(a["box"], e["box"], path + ".box", box_tolerance)
        if family in ("c4", "git-graph"):
            check.matrix_paint(a["paint"], e["paint"], path + ".paint")
            for key in ("path", "points"):
                if e.get(key):
                    check.path(a[key], e[key], path + "." + key)
        elif family == "treemap":
            check.compare(a["transform"], e["transform"], path + ".transform", .05)
        elif e.get("path"):
            check.path(a["path"], e["path"], path + ".path", view_tolerance, whitespace=False)


def wardley(actual, expected, path, check):
    numeric_string = isinstance(expected, str) and bool(expected.strip())
    if numeric_string:
        try:
            numeric_string = math.isfinite(float(expected))
        except ValueError:
            numeric_string = False
    if type(expected) in (int, float) or numeric_string:
        try:
            value = float(actual)
        except (ValueError, TypeError) as error:
            raise ValueError(path + ": invalid numeric value") from error
        check.compare(value, float(expected), path, 1e-8 if ".rotation." in path else 1.5)
    elif isinstance(expected, list):
        require(isinstance(actual, list) and len(actual) == len(expected), path + " length")
        for i, value in enumerate(expected):
            wardley(actual[i], value, f"{path}.{i}", check)
    elif isinstance(expected, dict):
        require(isinstance(actual, dict), path + " object")
        for key, value in expected.items():
            wardley(actual.get(key, MISSING), value, path + "." + key, check)
    else:
        check.compare(actual, expected, path)


def xy(actual, expected, name, check, oracle):
    def normalized(row):
        elements = []
        for e in row["elements"]:
            group = "/".join(g for g in e["groups"] if g)
            elements.append({"type": e["type"], "group": "background" if group == "main" else group,
                             "text": e["text"], "box": e["box"], "transform": e["transform"],
                             "paint": e["paint"], "path": numbers(e["attrs"]["d"]) if e["attrs"].get("d") else None})
        return sorted(elements, key=lambda e: (e["group"], e["type"], e["text"], e["box"]["x"], e["box"]["y"]))
    check.compare(actual["view"], expected["view"], name + ".view")
    a, e = normalized(actual), normalized(expected)
    require(len(a) == len(e), name + " element count")
    tolerance = .05 if oracle else 1.5
    for index, (av, ev) in enumerate(zip(a, e)):
        path = f"{name}.elements.{index}"
        check.fields(av, ev, ("type", "group", "text", "paint"), path)
        check.fields(av, ev, ("box", "path"), path, tolerance)
        check.compare(av["transform"][:4], ev["transform"][:4], path + ".linear", 1e-5)
        check.compare(av["transform"][4:], ev["transform"][4:], path + ".translation", tolerance)
