"""Family policies preserve the old checker decisions and strict tolerances."""

import copy
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.family_checks import Check
from comparison.family_simple import compare_family


class FamilyChecks(unittest.TestCase):
    pass


def register(document, case):
    def check(self):
        expected = document["expected"]
        actual = copy.deepcopy(expected)
        actual.pop("name")
        actual.pop("svg", None)
        for mutation in case["mutations"]:
            target = actual
            for key in mutation["path"][:-1]:
                target = target[key]
            target[mutation["path"][-1]] = mutation["value"]
        if case["accept"]:
            compare_family(document["family"], actual, expected, Check())
        else:
            with self.assertRaises((ValueError, KeyError, TypeError)):
                compare_family(document["family"], actual, expected, Check())
    setattr(FamilyChecks, "test_" + case["id"].replace("-", "_").replace(".", "_"), check)


for path in sorted(Path(__file__).with_name("fixtures").joinpath("families").glob("*.json")):
    document = json.loads(path.read_text(encoding="utf-8"))
    for case in document["cases"]:
        register(document, case)


if __name__ == "__main__":
    unittest.main()
