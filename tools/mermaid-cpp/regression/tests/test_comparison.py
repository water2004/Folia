"""All 74 original comparator checks, registered individually from data files."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.geometry import capture_viewport, compare_text, visible_presentation
from comparison.pixels import compare_pixels
from comparison.svg import compare_svg


FIXTURES = Path(__file__).with_name("fixtures")


def load(name):
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


def raster(description):
    data = np.full((description["height"], description["width"], 4), 255, dtype=np.uint8)
    data[:, :, 3] = description["alpha"]
    for item in description["rectangles"]:
        data[item["y"]:item["y"] + item["height"], item["x"]:item["x"] + item["width"], :3] = item["color"]
        data[item["y"]:item["y"] + item["height"], item["x"]:item["x"] + item["width"], 3] = 255
    return {"width": description["width"], "height": description["height"], "data": data}


class ComparisonTests(unittest.TestCase):
    def acceptance(self, accept, operation):
        if accept:
            operation()
        else:
            with self.assertRaises(ValueError):
                operation()


def register(group, record, check):
    def test(self):
        check(self, record)
    test.__doc__ = group + ": " + record["id"]
    setattr(ComparisonTests, "test_" + group + "_" + record["id"].replace("-", "_"), test)


for item in load("text-comparison.json"):
    register("text", item, lambda self, row: self.acceptance(row["accept"],
        lambda: compare_text(row["actual"], row["expected"])))

for item in load("svg-comparison.json"):
    register("svg", item, lambda self, row: self.acceptance(row["accept"],
        lambda: compare_svg(row["official"], row["native"], row["family"])))


def check_raster(self, row):
    result = compare_pixels(actual=raster(row["actual"]), expected=raster(row["expected"]),
                            scale=2, tolerance=1.5, colorTolerance=2)
    self.assertEqual(result["ok"], row["accept"])
    if row["failed_tiles"]:
        self.assertGreater(result["failedTiles"], 0)


for item in load("pixel-comparison.json"):
    register("raster", item, check_raster)


def check_viewport(self, row):
    data = row["input"]
    if data["bounds"]["width"] == "NaN":
        data = {**data, "bounds": {**data["bounds"], "width": float("nan")}}
    if row["expected"] is None:
        with self.assertRaises(ValueError):
            capture_viewport(**data)
    else:
        self.assertEqual(capture_viewport(**data), row["expected"])


for item in load("viewports.json"):
    register("viewport", item, check_viewport)

for item in load("visibility.json"):
    register("visibility", item, lambda self, row: self.assertEqual(visible_presentation(**row["input"]), row["expected"]))


if __name__ == "__main__":
    unittest.main()
