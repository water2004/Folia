"""Measured State, ER, Requirement, Sequence and Class acceptance policies."""

from __future__ import annotations

import math
import re

from .corpus import require
from .family_checks import NUMBER, Check, exact_fields, numbers


FAMILIES = {"state", "er", "requirement", "sequence", "class", "class-elk"}


def canonical_rough_stroke(path):
    current, output = None, []
    for chunk in re.findall(r"[MC][^MC]*", path):
        command, values = chunk[0], numbers(chunk)
        if command == "M":
            require(len(values) == 2, "RoughJS moveto coordinates")
            current = values
            output.append("M" + " ".join(map(str, current)))
            continue
        require(command == "C" and current is not None and len(values) == 6, "RoughJS cubic coordinates")
        end = values[4:]
        dx, dy = end[0] - current[0], end[1] - current[1]
        length_squared = dx * dx + dy * dy
        require(length_squared > 0, "RoughJS nonempty edge")
        for i in (0, 2):
            px, py = values[i] - current[0], values[i + 1] - current[1]
            require(abs(dx * py - dy * px) <= 1e-6 * max(1, math.sqrt(length_squared)), "RoughJS noncollinear control point")
            require(0 <= (px * dx + py * dy) / length_squared <= 1, "RoughJS control point outside edge")
        canonical = [current[0] + dx / 3, current[1] + dy / 3,
                     current[0] + 2 * dx / 3, current[1] + 2 * dy / 3, *end]
        output.append("C" + " ".join(map(str, canonical)))
        current = end
    return " ".join(output)


def rough_segments(path, fill):
    commands = re.sub(r"[\s,]", "", NUMBER.sub("#", path))
    values = numbers(path)
    require(commands == ("M##L##L##L##" if fill else "M##C######" * 8), "RoughJS command structure")
    if fill:
        return values
    segments = []
    for i in range(0, len(values), 8):
        x, y, cx, cy, dx, dy, end_x, end_y = values[i:i + 8]
        sx, sy = end_x - x, end_y - y
        length = math.hypot(sx, sy)
        require(length > 0, "RoughJS nonempty segment")
        previous = 0
        for px, py in ((cx, cy), (dx, dy)):
            vx, vy = px - x, py - y
            require(abs(vx * sy - vy * sx) / length <= 1e-7, "RoughJS collinear handles")
            parameter = (vx * sx + vy * sy) / (length * length)
            require(previous <= parameter <= 1, "RoughJS ordered handles")
            previous = parameter
        segments.extend((x, y, end_x, end_y))
    return segments


def state_or_er(family, actual, expected, check):
    name = expected["name"]
    check.compare(actual["view"], expected["view"], name + ".view", 1.5)
    for category in (("elements", "labels", "backgrounds", "markers") if family == "er" else ("elements", "labels", "backgrounds")):
        unmatched = list(actual.get("labels", []))
        for a, e, path in check.pairs(actual, expected, category, name):
            if category == "labels" and family == "er":
                index = next((i for i, label in enumerate(unmatched) if label["text"] == e["text"] and
                    abs(label["box"]["x"] - e["box"]["x"]) <= 1.5 and abs(label["box"]["y"] - e["box"]["y"]) <= 1.5), None)
                require(index is not None, path + ": missing label by position")
                a = unmatched.pop(index)
            if category == "markers":
                check.compare({k: v for k, v in a.items() if k != "contents"},
                              {k: v for k, v in e.items() if k != "contents"}, path + ".attributes")
                check.compare(a["contents"], [{k: v for k, v in item.items() if k != "path"} for item in e["contents"]], path + ".contents")
                for i, item in enumerate(e["contents"]):
                    if item.get("path"):
                        check.path(a["contents"][i]["path"], item["path"], path + f".contents.{i}.path")
                continue
            check.compare(a["box"], e["box"], path + ".box", 1.5)
            if category == "backgrounds":
                check.fields(a, e, ("part", "color") if family == "er" else ("color",), path)
            elif category == "labels":
                check.fields(a, e, ("text", "font"), path)
            else:
                check.fields(a, e, ("type", "rough", "paint", "markers") if family == "er" else ("type", "paint"), path)
                if family == "er" and e["rough"]:
                    fill = e["paint"]["fill"] != "none"
                    check.compare(rough_segments(a["path"], fill), rough_segments(e["path"], fill), path + ".path", 1.5)
                elif e.get("path"):
                    if family == "state" and e.get("roughRectangleStroke") and e["paint"]["fill"] == "none":
                        check.path(canonical_rough_stroke(a["path"]), canonical_rough_stroke(e["path"]), path + ".path")
                    else:
                        check.path(a["path"], e["path"], path + ".path")


def requirement(actual, expected, check):
    name = expected["name"]
    check.compare(actual["view"], expected["view"], name + ".view", 1.5)
    for category in ("nodes", "edges"):
        for a, e, path in check.pairs(actual, expected, category, name):
            check.fields(a, e, ("classes",), path)
            check.compare(a["box"], e["box"], path + ".box", 1.5)
            if category == "nodes":
                check.fields(a, e, ("fill", "stroke", "strokeWidth"), path)
                check.compare(a["divider"], e["divider"], path + ".divider", 1.5)
            else:
                check.fields(a, e, ("startMarker", "endMarker"), path)
                check.path(a["path"], e["path"], path + ".path")
    for a, e, path in check.pairs(actual, expected, "labels", name):
        check.fields(a, e, ("text", "font", "runs") if name.startswith("rich-") else ("text", "font"), path)
        check.compare(a["box"], e["box"], path + ".box", 1.5)


def sequence(actual, expected, check):
    name = expected["name"]
    check.fields(actual["view"], expected["view"], ("x", "y", "width", "height"), name + ".view", 1)
    for category in ("numbers", "backgrounds", "regions", "actors", "notes", "activations", "figures", "shapes", "lines", "texts", "signals"):
        for a, e, path in check.pairs(actual, expected, category, name):
            for key in ("tag", "dash", "id", "footer", "text", "size", "weight", "fill", "stroke", "strokeWidth", "kind"):
                if key in e:
                    check.fields(a, e, (key,), path)
            for key in ("x", "y", "width", "height", "r"):
                if key in e:
                    check.fields(a, e, (key,), path, 1.1 if category == "texts" else 1)
            if category in ("regions", "shapes") and e.get("points"):
                check.compare(a["points"], e["points"], path + ".polygon", 1)
            if category == "signals":
                exact_fields(a, e, ("start", "end"), path)
                check.compare(a["points"], e["points"], path + ".points", 1)


def class_diagram(actual, expected, check):
    name = expected["name"]
    for key in ("nodes", "edges", "labels", "clusters"):
        check.pairs(actual, expected, key, name)
    check.fields(actual["view"], expected["view"], ("x", "y", "width", "height"), name + ".view", .2)
    require(bool(actual.get("title")) == bool(expected.get("title")), name + ".title")

    def text(a, e, path, keys):
        exact_fields(a, e, keys, path)
        check.fields(a, e, ("x", "width"), path, .25)
        check.compare(a["y"] + a["height"] / 2, e["y"] + e["height"] / 2, path + ".centerY", 2.5)

    if expected.get("title"):
        text(actual["title"], expected["title"], name + ".title", ("text", "size", "fill"))
        check.fields(actual["title"], expected["title"], ("baseline",), name + ".title", .25)
    for e in expected["clusters"]:
        a = next((a for a in actual["clusters"] if a["id"] == e["id"]), None)
        require(a is not None, name + ".cluster." + e["id"])
        check.fields(a, e, ("x", "y", "width", "height"), name + ".cluster", .2)
        check.fields(a, e, ("fill", "stroke", "strokeWidth"), name + ".cluster")
        text(a["label"], e["label"], name + ".clusterLabel", ("text", "color", "size"))
    for e in expected["nodes"]:
        a = next((a for a in actual["nodes"] if a["id"] == e["id"]), None)
        require(a is not None, name + ".node." + e["id"])
        path = name + "." + e["id"]
        check.fields(a, e, ("x", "y", "width", "height"), path, .2)
        check.fields(a, e, ("fill", "stroke", "strokeWidth", "strokeDasharray", "opacity"), path)
        for av, ev, p in check.pairs(a, e, "dividers", path):
            check.fields(av, ev, ("x", "y", "width", "height"), p, .2)
            check.fields(av, ev, ("fill", "stroke", "strokeWidth", "strokeDasharray"), p)
        for av, ev, p in check.pairs(a, e, "groups", path):
            check.fields(av, ev, ("kind",), p)
            for at, et, label_path in check.pairs(av, ev, "labels", p):
                text(at, et, label_path, ("runs", "text", "color", "size", "weight", "italic", "decoration"))
    for a, e, path in check.pairs(actual, expected, "edges", name):
        exact_fields(a, e, ("fill", "stroke", "strokeWidth", "dash", "start", "end"), path)
        check.compare(a["points"], e["points"], path + ".points", .2)
    for a, e, path in check.pairs(actual, expected, "labels", name):
        text(a, e, path, ("text", "runs"))


def compare_family(family, actual, expected, check: Check, oracle=False):
    if family in ("state", "er"):
        state_or_er(family, actual, expected, check)
    elif family == "requirement":
        requirement(actual, expected, check)
    elif family == "sequence":
        sequence(actual, expected, check)
    else:
        class_diagram(actual, expected, check)
