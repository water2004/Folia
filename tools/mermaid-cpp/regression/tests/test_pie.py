"""Saved decisions from the original Pie DOM gate and capture integrity checks."""

import copy
import json
import math
import sys
import tempfile
import unittest
from pathlib import Path

from lxml import etree

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.corpus import UPSTREAM, digest
from comparison.pie import FIXTURE, compare_case, compare_capture, equal, load_capture, parse, snapshot


DATA = json.loads((Path(__file__).with_name("fixtures") / "pie-dom.json").read_text(encoding="utf-8"))


class PieDomChecks(unittest.TestCase):
    def test_original_js_snapshot(self):
        self.assertEqual(snapshot(parse(DATA["source"])), DATA["snapshot"])

    def test_all_eighty_inputs_retained(self):
        rows = [row for row in FIXTURE.read_text().splitlines() if row and not row.startswith("#")]
        self.assertEqual(len(rows), 80)
        self.assertEqual(len({row.split("\t")[0] for row in rows}), 80)

    def test_numeric_limit_is_exclusive(self):
        equal(0, 0.9e-10)
        with self.assertRaises(ValueError):
            equal(0, 1e-10)

    def test_numeric_nonfinite_rejected(self):
        for value in (math.nan, math.inf, -math.inf):
            with self.assertRaises(ValueError):
                equal(value, value)

    def test_numeric_bool_rejected(self):
        with self.assertRaises(ValueError):
            equal(1, True)

    def test_missing_snapshot_key_rejected(self):
        actual = copy.deepcopy(DATA["snapshot"])
        del actual["radius"]
        with self.assertRaises(ValueError):
            equal(DATA["snapshot"], actual)

    def test_nonfinite_upstream_requires_exact_error(self):
        item = {"name": "nonfinite", "finite": False}
        self.assertTrue(compare_case(item, "ERROR")["expectedError"])
        for value in ("ERROR\n", "", DATA["source"]):
            with self.assertRaises(ValueError):
                compare_case(item, value)

    def test_all_seven_rules_required(self):
        item = {"name": "sample", "finite": True, "snapshot": DATA["snapshot"], "styles": DATA["styles"][:-1]}
        with self.assertRaisesRegex(ValueError, "seven"):
            compare_case(item, DATA["source"])


def register(case):
    def check(self):
        item = {"name": case["id"], "finite": True, "snapshot": DATA["snapshot"], "styles": DATA["styles"]}
        if case["accepted"]:
            self.assertEqual(compare_case(item, case["svg"])["status"], "passed")
        else:
            with self.assertRaises((ValueError, etree.XMLSyntaxError)):
                compare_case(item, case["svg"])
    setattr(PieDomChecks, "test_original_" + case["id"].replace("-", "_"), check)


for case in DATA["cases"]:
    register(case)


class PieCaptureChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.fixture = self.root / "cases.tsv"
        self.fixture.write_text("sample\t007000690065\tright\t-\t120\t0\t0.75\t-\t1\t0\t1\n")
        self.lock = self.root / "lock.json"
        self.lock.write_text("{}\n")
        item = {"name": "sample", "sourceSha256": digest("pie"), "finite": True,
                "svg": DATA["source"], "svgSha256": digest(DATA["source"]),
                "snapshot": copy.deepcopy(DATA["snapshot"]), "styles": DATA["styles"]}
        self.capture = {"schemaVersion": 1, "upstream": UPSTREAM, "cases": [item],
                        "sourceLockSha256": digest(self.lock.read_bytes()), "fixtureSha256": digest(self.fixture.read_bytes())}
        self.path = self.root / "original.json"
        (self.root / "sample.svg").write_text(DATA["source"], encoding="utf-8")

    def load(self):
        self.path.write_text(json.dumps(self.capture), encoding="utf-8")
        return load_capture(self.path, self.fixture, self.lock)

    def test_complete_saved_capture(self):
        self.load()
        report = compare_capture(self.path, self.root, self.fixture, self.lock)
        self.assertEqual((report["checked"], report["compared"]), (1, 1))

    def test_missing_capture_case(self):
        self.capture["cases"].clear()
        with self.assertRaisesRegex(ValueError, "every fixture"):
            self.load()

    def test_duplicate_capture_case(self):
        self.capture["cases"].append(self.capture["cases"][0])
        with self.assertRaisesRegex(ValueError, "every fixture"):
            self.load()

    def test_changed_input_source(self):
        self.capture["cases"][0]["sourceSha256"] = digest("different")
        with self.assertRaisesRegex(ValueError, "source differs"):
            self.load()

    def test_changed_official_svg(self):
        self.capture["cases"][0]["svg"] += " "
        with self.assertRaisesRegex(ValueError, "SVG hash"):
            self.load()

    def test_changed_official_snapshot(self):
        self.capture["cases"][0]["snapshot"]["radius"] += 1
        with self.assertRaisesRegex(ValueError, "radius"):
            self.load()

    def test_changed_finite_status(self):
        self.capture["cases"][0]["finite"] = False
        with self.assertRaisesRegex(ValueError, "finite"):
            self.load()

    def test_nonboolean_finite_status(self):
        self.capture["cases"][0]["finite"] = 1
        with self.assertRaisesRegex(ValueError, "finite"):
            self.load()

    def test_changed_fixture_bytes(self):
        self.fixture.write_text(self.fixture.read_text() + "# changed\n")
        with self.assertRaisesRegex(ValueError, "fixture bytes"):
            self.load()

    def test_changed_source_lock(self):
        self.lock.write_text('{"changed": true}\n')
        with self.assertRaisesRegex(ValueError, "source lock"):
            self.load()

    def test_changed_upstream(self):
        self.capture["upstream"] = {"version": "different"}
        with self.assertRaisesRegex(ValueError, "upstream"):
            self.load()

    def test_schema_boolean_rejected(self):
        self.capture["schemaVersion"] = True
        with self.assertRaisesRegex(ValueError, "schema"):
            self.load()
