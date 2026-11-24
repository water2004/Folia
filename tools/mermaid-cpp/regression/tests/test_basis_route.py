"""Mindmap route coordinates captured from the pinned D3 basis interpolator."""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.family_checks import numbers
from comparison.family_structures import basis_route


class BasisRouteChecks(unittest.TestCase):
    pass


def register(index, case):
    def check(self):
        self.assertEqual(basis_route(case["source"], case["target"]), numbers(case["path"]))
    setattr(BasisRouteChecks, "test_d3_route_" + str(index), check)


for index, case in enumerate(json.loads(Path(__file__).with_name("fixtures").joinpath("basis-routes.json").read_text())):
    register(index, case)
