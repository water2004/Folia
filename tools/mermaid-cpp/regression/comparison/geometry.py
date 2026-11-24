"""Geometry and one-to-one visible grapheme comparisons."""

from __future__ import annotations

import math

from .corpus import require


GEOMETRY_TOLERANCE = 1.5


def near(actual: float, expected: float, location: str, tolerance: float = GEOMETRY_TOLERANCE) -> None:
    require(isinstance(actual, (int, float)) and isinstance(expected, (int, float)) and
            math.isfinite(actual) and math.isfinite(expected) and abs(actual - expected) <= tolerance,
            f"{location}: {actual} != {expected} (tolerance {tolerance})")


def close_object(actual, expected, location: str) -> None:
    if isinstance(expected, bool):
        require(type(actual) is bool and actual == expected, location)
    elif isinstance(expected, (int, float)):
        near(actual, expected, location)
    elif isinstance(expected, list):
        require(isinstance(actual, list) and len(actual) == len(expected), location + " count")
        for i, value in enumerate(expected):
            close_object(actual[i], value, f"{location}.{i}")
    elif isinstance(expected, dict):
        require(isinstance(actual, dict), location + " object")
        for key, value in expected.items():
            if key in ("fontSize", "strokeWidth", "opacity", "fillOpacity", "strokeOpacity"):
                near(actual.get(key), value, location + "." + key, .001)
            else:
                close_object(actual.get(key), value, location + "." + key)
    else:
        require(actual == expected, f"{location}: {actual!r} != {expected!r}")


def compare_text(actual: list[dict], expected: list[dict], tolerance: float = GEOMETRY_TOLERANCE) -> dict:
    require(len(actual) == len(expected), "Visible grapheme count")
    groups = {}
    for index, item in enumerate(actual):
        groups.setdefault(item["text"], []).append((index, item))
    options = []
    for target in expected:
        matching, best = [], math.inf
        for index, item in groups.get(target["text"], []):
            distance = max(abs(item[k] - target[k]) for k in ("x", "y", "width", "height"))
            best = min(best, distance)
            if distance <= tolerance and abs(item["size"] - target["size"]) <= .05 and all(
                    item[k] == target[k] for k in ("weight", "style", "paint")):
                matching.append((index, distance))
        require(bool(matching), f"Missing, displaced or restyled grapheme {target['text']!r} "
                f"at {target['x']},{target['y']}; nearest geometry difference {best}")
        options.append(sorted(matching, key=lambda item: item[1]))

    # Augmenting paths prevent a nearby repeated glyph from consuming the only
    # possible candidate for its neighbor. Use a stack to avoid recursion limits
    # for long documents containing many repeated glyphs.
    owners, assignments = {}, {}
    for target, glyph in enumerate(expected):
        stack, seen, previous = [(target, iter(options[target]))], set(), {}
        assigned = False
        while stack:
            current, candidates = stack[-1]
            option = next(candidates, None)
            if option is None:
                stack.pop()
                continue
            index, distance = option
            if index in seen:
                continue
            seen.add(index)
            if index not in owners:
                owners[index], assignments[current] = current, (index, distance)
                while current in previous:
                    current, previous_option = previous[current]
                    owners[previous_option[0]], assignments[current] = current, previous_option
                assigned = True
                break
            displaced = owners[index]
            previous[displaced] = current, option
            stack.append((displaced, iter(options[displaced])))
        require(assigned, f"No one-to-one visible glyph correspondence near {glyph['x']},{glyph['y']}")
    return {"graphemes": len(expected), "maximumDifference": max((d for _, d in assignments.values()), default=0)}


def capture_viewport(*, family, viewBoxPresent, view, bounds):
    intrinsic = family == "info" and not viewBoxPresent
    size = bounds if intrinsic else view
    require((intrinsic or viewBoxPresent) and size is not None and all(
        isinstance(size.get(k), (int, float)) and math.isfinite(size[k]) and size[k] > 0
        for k in ("width", "height")), "Invalid SVG viewport")
    return {"width": size["width"], "height": size["height"], "intrinsic": intrinsic}


def visible_presentation(*, visibility, ancestors):
    return visibility not in ("hidden", "collapse") and all(
        s.get("display") != "none" and float(s.get("opacity") or "1") != 0 for s in ancestors)
