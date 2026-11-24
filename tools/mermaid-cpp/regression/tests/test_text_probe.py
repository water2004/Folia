"""Text-probe ceilings, evidence ordering and malformed observations."""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.text_probe import compare_text_probe


class TextProbeChecks(unittest.TestCase):
    pass


document = json.loads(Path(__file__).with_name("fixtures").joinpath("text-probe.json").read_text())


def register(case):
    def check(self):
        fields = document["native"].split("\t")
        if "field" in case:
            fields[case["field"]] = case["value"]
        if "bounds" in case:
            fields[7] = case["bounds"]
        reference = "\n".join([document["reference"]] * 96)
        native = "\n".join(["\t".join(fields)] * case.get("count", 96))
        if case["accept"]:
            self.assertEqual(compare_text_probe(reference, native)["cases"], 96)
        else:
            with self.assertRaises(ValueError):
                compare_text_probe(reference, native)
    setattr(TextProbeChecks, "test_" + case["id"].replace("-", "_"), check)


for case in document["cases"]:
    register(case)
