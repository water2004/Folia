"""Shared assertions for historical diagram-family capture formats."""

from __future__ import annotations

import math
import re

from .corpus import require


NUMBER = re.compile(r"[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?", re.I)
MISSING = object()


def numbers(value):
    return [float(n) for n in NUMBER.findall(value)]


class Check:
    def __init__(self):
        self.maximum = 0
        self.maximum_at = ""

    def compare(self, actual, expected, location, tolerance=0):
        if type(expected) in (int, float):
            require(type(actual) in (int, float) and math.isfinite(actual) and
                    math.isfinite(expected) and abs(actual - expected) <= tolerance,
                    f"{location}: {actual!r} != {expected!r} (tolerance {tolerance})")
            delta = abs(actual - expected)
            if delta > self.maximum:
                self.maximum, self.maximum_at = delta, location
        elif isinstance(expected, list):
            require(isinstance(actual, list) and len(actual) == len(expected), location + " count")
            for i, value in enumerate(expected):
                self.compare(actual[i], value, f"{location}.{i}", tolerance)
        elif isinstance(expected, dict):
            require(isinstance(actual, dict), location + " object")
            for key, value in expected.items():
                self.compare(actual.get(key, MISSING), value, location + "." + key, tolerance)
        else:
            require(type(actual) is type(expected) and actual == expected,
                    f"{location}: {actual!r} != {expected!r}")

    def fields(self, actual, expected, names, location, tolerance=0):
        for key in names:
            self.compare(actual.get(key, MISSING), expected.get(key, MISSING), location + "." + key, tolerance)

    def pairs(self, actual, expected, key, location):
        a, e = actual[key], expected[key]
        require(isinstance(a, list) and len(a) == len(e), location + "." + key + " count")
        return [(av, ev, f"{location}.{key}.{i}") for i, (av, ev) in enumerate(zip(a, e))]

    def path(self, actual, expected, location, tolerance=1.5, whitespace=True):
        def commands(value):
            text = NUMBER.sub("#", value)
            return re.sub(r"[\s,]", "", text) if whitespace else text
        self.compare(commands(actual), commands(expected), location + ".commands")
        self.compare(numbers(actual), numbers(expected), location + ".numbers", tolerance)

    def token_path(self, actual, expected, location, tolerance=1.5):
        def tokens(value):
            return [float(v) if NUMBER.fullmatch(v) else v
                    for v in re.findall(r"[A-Za-z]|[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?", value, re.I)]
        if expected is None:
            self.compare(actual, None, location)
        else:
            self.compare(tokens(actual), tokens(expected), location, tolerance)

    def matrix_paint(self, actual, expected, location):
        for key, value in expected.items():
            if key == "transform" and value != "none":
                a, e = numbers(actual[key]), numbers(value)
                require(len(a) == len(e) == 6, location + ".transform matrix")
                self.compare(a[:4], e[:4], location + ".transform.linear", 1e-5)
                self.compare(a[4:], e[4:], location + ".transform.translation", 1.5)
            else:
                self.compare(actual[key], value, location + "." + key)
