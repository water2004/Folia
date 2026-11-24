"""Offline negative gates using captured real-font Skia sample bytes."""

import copy
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from comparison.canvas_records import compare_case, compare_observations
from comparison.corpus import digest


class CanvasEvidenceChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(Path(__file__).with_name("fixtures") / "canvas-evidence", self.root, dirs_exist_ok=True)
        self.document = json.loads((self.root / "native-observations.json").read_text())
        self.official = json.loads((self.root / "official.json").read_text())
        self.entries = [json.loads(line) for line in (self.root / "samples/samples.jsonl").read_text().splitlines()]

    def compare(self, rewrite_log=False):
        if rewrite_log:
            log = "".join(json.dumps(entry) + "\n" for entry in self.entries)
            (self.root / "samples/samples.jsonl").write_text(log)
            self.document["samplesSha256"] = digest(log)
        path = self.root / "native-observations.json"
        path.write_text(json.dumps(self.document))
        with patch("comparison.canvas_records.official_records", return_value=(self.official, self.document["corpusSha256"])):
            return compare_observations(self.root, path)

    def test_real_sample_identity(self):
        report = self.compare()
        self.assertEqual((report["passed"], report["identicalRaster"], report["nativeFontRequests"]), (1, 1, 1))

    def test_missing_case(self):
        self.document["records"].clear()
        with self.assertRaises(ValueError): self.compare()

    def test_duplicate_case(self):
        self.document["records"] *= 2
        with self.assertRaises(ValueError): self.compare()

    def test_changed_native_svg(self):
        (self.root / "native.svg").write_text("changed")
        with self.assertRaises(ValueError): self.compare()

    def test_changed_official_svg(self):
        (self.root / "official.svg").write_text("changed")
        with self.assertRaises(ValueError): self.compare()

    def test_changed_observation_log(self):
        with (self.root / "samples/samples.jsonl").open("a") as output: output.write("\n")
        with self.assertRaises(ValueError): self.compare()

    def test_changed_pixel_bytes(self):
        sample = self.document["records"][0]["nativeRaster"]
        (self.root / "samples" / sample["rgba"]).write_bytes(b"changed")
        with self.assertRaises(ValueError): self.compare()

    def test_one_channel_difference_including_transparent_rgb(self):
        sample = self.document["records"][0]["nativeRaster"]
        path = self.root / "samples" / sample["rgba"]
        pixels = bytearray(path.read_bytes())
        self.assertEqual(pixels[3], 0)
        pixels[0] ^= 1
        path.write_bytes(pixels)
        sample["rgbaSha256"] = digest(pixels)
        for entry in self.entries:
            if entry["sample"]["inputSha256"] == sample["inputSha256"]:
                entry["sample"] = copy.deepcopy(sample)
        report = self.compare(rewrite_log=True)
        self.assertEqual(report["passed"], 0)
        self.assertEqual(report["results"][0]["differentPixels"], 1)

    def test_missing_font_measurement(self):
        self.entries = [entry for entry in self.entries if entry["request"]["kind"] != "text"]
        with self.assertRaises(ValueError): self.compare(rewrite_log=True)

    def test_incomplete_font_measurement(self):
        next(entry for entry in self.entries if entry["request"]["kind"] == "text")["sample"]["metrics"].pop("baseline")
        with self.assertRaises(ValueError): self.compare(rewrite_log=True)

    def test_changed_measured_input(self):
        next(entry for entry in self.entries if entry["request"]["kind"] == "text")["request"]["text"] += " changed"
        with self.assertRaises(ValueError): self.compare(rewrite_log=True)

    def test_missing_font_profile(self):
        self.entries = self.entries[1:]
        with self.assertRaises(ValueError): self.compare(rewrite_log=True)

    def test_different_installed_fonts(self):
        self.entries[0]["sample"]["availableFonts"] = []
        with self.assertRaises(ValueError): self.compare(rewrite_log=True)

    def test_estimated_backend_rejected(self):
        self.document["backend"]["estimatedWidths"] = True
        with self.assertRaises(ValueError): self.compare()

    def test_abnormal_native_result(self):
        self.document["records"][0]["native"] = {"ok": False, "error": "native failure"}
        report = self.compare()
        self.assertEqual(report["passed"], 0)
        self.assertFalse(report["results"][0]["nativeOk"])

    def test_viewport_tolerance_boundary(self):
        row = self.document["records"][0]
        sample = row["nativeRaster"]
        pixels = (self.root / "samples" / sample["rgba"]).read_bytes()
        source = (self.root / "native.svg").read_text()
        expected = self.official["cases"][0]
        for delta, accepted in ((1.5, True), (1.50001, False)):
            result = compare_case(expected, dict(row["native"], width=16 + delta), source, source, pixels, pixels)
            self.assertEqual(result["ok"], accepted)

    def test_text_is_checked_even_when_rasters_match(self):
        row = self.document["records"][0]
        pixels = (self.root / "samples" / row["nativeRaster"]["rgba"]).read_bytes()
        source = (self.root / "native.svg").read_text()
        result = compare_case(self.official["cases"][0], row["native"], source, source.replace(">x<", ">y<"), pixels, pixels)
        self.assertFalse(result["ok"])
        self.assertFalse(result["sameLabels"])
