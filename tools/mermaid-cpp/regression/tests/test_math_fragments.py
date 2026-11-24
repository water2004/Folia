"""Exact drawing-tree decisions and evidence failures from the old Math gate."""

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

from lxml import etree

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.corpus import digest
from comparison.fragment_records import compare_fragments
from comparison.math_fragments import first_difference, svg_tree


FIXTURES = Path(__file__).with_name("fixtures")


class SvgTreeChecks(unittest.TestCase):
    pass


def register_tree(case):
    def check(self):
        if case.get("error"):
            with self.assertRaises((ValueError, etree.XMLSyntaxError)):
                svg_tree(case["svg"])
        else:
            self.assertEqual(svg_tree(case["svg"]), case["tree"])
    setattr(SvgTreeChecks, "test_" + case["id"].replace("-", "_"), check)


def register_difference(case):
    def check(self):
        self.assertEqual(first_difference(case["expected"], case["actual"]), case["difference"])
    setattr(SvgTreeChecks, "test_difference_" + case["id"].replace("-", "_"), check)


for case in json.loads((FIXTURES / "math-svg-trees.json").read_text(encoding="utf-8")):
    register_tree(case)
for case in json.loads((FIXTURES / "math-tree-differences.json").read_text(encoding="utf-8")):
    register_difference(case)


class FragmentEvidenceChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(FIXTURES / "math-evidence", self.root, dirs_exist_ok=True)
        self.native = json.loads((self.root / "native.json").read_text())
        self.observations = json.loads((self.root / "raster-observations.json").read_text())

    def compare(self):
        return compare_fragments(self.root / "official.json", self.root / "native.json", self.root / "cases.json",
                                 self.root / "raster-observations.json", self.root / "result")

    def write_native(self, rebind=False):
        path = self.root / "native.json"
        path.write_text(json.dumps(self.native))
        if rebind:
            self.observations["nativeSha256"] = digest(path.read_bytes())
            self.write_observations()

    def write_observations(self):
        (self.root / "raster-observations.json").write_text(json.dumps(self.observations))

    def test_actual_empty_mathjax_fragment(self):
        report = self.compare()
        self.assertEqual(report["accepted"], 1)
        self.assertEqual(report["records"][0]["raster"]["differentPixels"], 0)

    def test_original_metric_tolerance_accepts_small_delta(self):
        self.native["records"][0]["width"] += .5e-9
        self.write_native(rebind=True)
        self.assertEqual(self.compare()["accepted"], 1)

    def test_original_metric_tolerance_rejects_larger_delta(self):
        self.native["records"][0]["baseline"] += 2e-9
        self.write_native(rebind=True)
        report = self.compare()
        self.assertEqual(report["accepted"], 0)
        self.assertFalse(report["records"][0]["metricsEqual"])

    def test_empty_error_object_still_fails(self):
        self.native["records"][0]["error"] = {}
        self.write_native(rebind=True)
        self.assertEqual(self.compare()["accepted"], 0)

    def test_missing_case(self):
        self.native["records"].clear()
        self.write_native()
        with self.assertRaises(ValueError): self.compare()

    def test_extra_case(self):
        self.native["records"] *= 2
        self.write_native()
        with self.assertRaises(ValueError): self.compare()

    def test_boolean_schema_rejected(self):
        self.native["schema"] = True
        self.write_native()
        with self.assertRaises(ValueError): self.compare()

    def test_wrong_case_identity(self):
        self.native["records"][0]["name"] += " changed"
        self.write_native()
        with self.assertRaises(ValueError): self.compare()

    def test_changed_report_bytes(self):
        self.native["records"][0]["baseline"] += .5e-9
        self.write_native()
        with self.assertRaises(ValueError): self.compare()

    def test_changed_corpus(self):
        with (self.root / "cases.json").open("a") as output: output.write("\n")
        with self.assertRaises(ValueError): self.compare()

    def test_missing_raster_case(self):
        self.observations["records"].clear()
        self.write_observations()
        with self.assertRaises(ValueError): self.compare()

    def test_changed_pixel_bytes(self):
        sample = self.observations["records"][0]["native"]
        (self.root / "samples" / sample["rgba"]).write_bytes(b"changed")
        with self.assertRaises(ValueError): self.compare()

    def test_changed_pixel_log(self):
        with (self.root / "samples/samples.jsonl").open("a") as output: output.write("\n")
        with self.assertRaises(ValueError): self.compare()
