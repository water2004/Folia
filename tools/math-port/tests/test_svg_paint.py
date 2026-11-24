"""Actual upstream/legacy raster evidence and corrupted paint/input controls."""

from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

PORT = Path(__file__).resolve().parents[1]
ROOT = PORT.parents[1]
sys.path[:0] = [str(PORT / "svg-paint"), str(ROOT / "tools/mermaid-cpp/regression")]
spec = importlib.util.spec_from_file_location("svg_paint_check", PORT / "svg-paint/check.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)
from paint_dom import canvas, dimensions, document_roots, materialize, paint_checks, parse, target_nodes, validate_styles
from paint_records import file_hash, load_inputs
from paint_samples import RasterSamples, difference

FIXTURE = Path(__file__).with_name("fixtures") / "paint-evidence"
CORPUS, REFERENCE, NATIVE, INPUTS = load_inputs(FIXTURE / "cases.json", FIXTURE / "reference.json", FIXTURE / "native.json")
EXPECTED = json.loads((FIXTURE / "expected.json").read_bytes())["records"]


class SvgPaintChecks(unittest.TestCase):
    def test_recorded_legacy_rasters_and_every_removal_control(self):
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary)
            with RasterSamples(destination, INPUTS, Path("never-run.js"), observations=FIXTURE / "raster-observations.json", node="must-not-run") as raster:
                for item, expected, actual, legacy in zip(CORPUS["cases"], REFERENCE["records"], NATIVE["records"], EXPECTED):
                    row = checker.tex_record(item, "display", expected, actual, destination, raster)
                    self.assertTrue(row["accepted"], row)
                    for key in ("raster", "sourceProjection", "sourceCssEffect"):
                        self.assertEqual(row[key], legacy[key])
                    for got, want in zip(row["parts"], legacy["parts"]):
                        for key in ("metrics", "raster", "sourceCssEffect", "sourceProjection", "inkPixels", "paint", "features"):
                            self.assertEqual(got[key], want[key], (item["name"], key))
                        for key in ("officialPng", "nativePng"):
                            self.assertEqual(got["hashes"][key], want["hashes"][key])

    def assert_early_failure(self, mutate, contains):
        expected, actual = deepcopy(REFERENCE["records"][0]), deepcopy(NATIVE["records"][0])
        mutate(expected, actual)
        with tempfile.TemporaryDirectory() as temporary:
            row = checker.tex_record(CORPUS["cases"][0], "display", expected, actual, Path(temporary), None)
        self.assertFalse(row["accepted"])
        self.assertIn(contains, row["error"]["message"])

    def test_native_process_failure_is_not_a_pass(self):
        self.assert_early_failure(lambda e, a: a.update(status=4, error="native error"), "native error")

    def test_native_svg_corruption_is_rejected_before_rendering(self):
        self.assert_early_failure(lambda e, a: a.update(svg=a["svg"].replace('stroke-width="70"', 'stroke-width="0"')), "SVG bytes changed")

    def test_failed_reference_is_not_silently_removed(self):
        self.assert_early_failure(lambda e, a: e.update(error={"message": "oracle error"}), "Official capture failed")

    def test_missing_source_piece_is_rejected(self):
        self.assert_early_failure(lambda e, a: e.update(roots=[]), "Source has no direct")

    def test_extra_source_piece_is_rejected(self):
        self.assert_early_failure(lambda e, a: e["roots"].append(e["roots"][0]), "fragment counts differ")

    def test_changed_document_width_is_rejected(self):
        def mutate(expected, actual):
            expected["roots"][0] = expected["roots"][0].replace('width="', 'width="1', 1)
        self.assert_early_failure(mutate, "Document width differs")

    def test_missing_root_box_is_rejected(self):
        self.assert_early_failure(lambda e, a: e.update(rootBox=None), "Missing Source root box")

    def test_changed_source_rule_is_rejected(self):
        self.assert_early_failure(lambda e, a: e["styles"]["rules"]["tableLine"].update({"stroke-width": "0"}), "paint rules changed")

    def test_native_paint_on_ancestor_does_not_satisfy_line(self):
        item, actual = CORPUS["cases"][0], NATIVE["records"][0]
        root = document_roots(actual["svg"])[0]
        line = target_nodes(root, "table-lines")[0]
        line.getparent().set("stroke-width", "70")
        del line.attrib["stroke-width"]
        self.assertFalse(next(row["passed"] for row in paint_checks(root, item) if row["name"] == "table-lines-0-paint"))

    def test_glyph_width_and_dotted_caps_are_independent(self):
        root = document_roots(NATIVE["records"][0]["svg"])[0]
        target_nodes(root, "glyphs")[0].set("stroke-width", "0")
        line = target_nodes(root, "table-lines")[0]
        line.set("class", "mjx-dotted")
        line.set("stroke-dasharray", "0 140")
        checks = paint_checks(root, CORPUS["cases"][0])
        self.assertFalse(next(row["passed"] for row in checks if row["name"] == "table-lines-0-paint"))
        self.assertTrue(any(not row["passed"] for row in checks if row["name"] == "glyph-blacker"))

    def test_percentage_width_requires_recorded_container(self):
        root = parse('<svg width="50%" height="2ex"/>')
        self.assertEqual(dimensions({"containerWidth": 120}, root), {"width": 60, "height": 16})
        with self.assertRaisesRegex(ValueError, "container width"):
            dimensions({}, root)
        root.set("height", "20%")
        with self.assertRaisesRegex(ValueError, "Only document width"):
            dimensions({"containerWidth": 120}, root)

    def test_fragment_metrics_follow_original_box_scale(self):
        item = {"em": 20}
        observation = {"rootBox": {"w": 2, "h": 3, "d": 1, "pwidth": ""}, "xHeight": .5}
        self.assertEqual(dimensions(item, None, observation), {"width": 40, "height": 80, "baseline": 60})
        observation["rootBox"]["pwidth"] = "100%"
        with self.assertRaisesRegex(ValueError, "intrinsic"):
            dimensions(item, None, observation)

    def test_projection_keeps_inline_styles_and_foreign_html(self):
        styles = REFERENCE["records"][0]["styles"]
        root = parse('<svg><g stroke-width="0"><g data-mml-node="mtable"><line data-line="h" style="stroke-width:8px"/></g>'
                     '<foreignObject><a href="keep"><path data-c="a"/></a></foreignObject></g></svg>')
        projected = materialize(root, styles["rules"])
        line = target_nodes(projected, "table-lines")[0]
        self.assertEqual(line.get("stroke-width"), "70")
        self.assertEqual(line.get("style"), "stroke-width:8px")
        html_path = target_nodes(projected, "glyphs")[0]
        self.assertNotIn("stroke-width", html_path.attrib)

    def test_exact_rgba_does_not_mask_transparent_rgb(self):
        a = {"info": {"width": 1}, "data": bytes([0, 0, 0, 0])}
        b = {"info": {"width": 1}, "data": bytes([1, 0, 0, 0])}
        self.assertEqual(difference(a, b), {"differentPixels": 1, "maximumChannelDifference": 1})
        b["info"]["width"] = 2
        with self.assertRaisesRegex(ValueError, "dimensions"):
            difference(a, b)

    def test_corpus_missing_or_reordered_records_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "native.json"
            changed = deepcopy(NATIVE)
            changed["records"].reverse()
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "coverage/order"):
                load_inputs(FIXTURE / "cases.json", FIXTURE / "reference.json", path)
            changed["records"].pop()
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "coverage/order"):
                load_inputs(FIXTURE / "cases.json", FIXTURE / "reference.json", path)

    def test_wrong_corpus_hash_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "native.json"
            changed = deepcopy(NATIVE)
            changed["corpusSha256"] = "0" * 64
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "corpus changed"):
                load_inputs(FIXTURE / "cases.json", FIXTURE / "reference.json", path)

    def test_observation_log_corruption_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            shutil.copytree(FIXTURE, directory / "fixture")
            log = directory / "fixture/samples/samples.jsonl"
            log.write_bytes(log.read_bytes() + b"\n")
            with self.assertRaisesRegex(ValueError, "log changed"):
                with RasterSamples(directory / "output", INPUTS, None, observations=directory / "fixture/raster-observations.json"):
                    pass

    def test_missing_or_modified_pixels_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            shutil.copytree(FIXTURE, directory / "fixture")
            observation = directory / "fixture/raster-observations.json"
            entry = json.loads((directory / "fixture/samples/samples.jsonl").read_bytes().splitlines()[0])
            pixel = directory / "fixture/samples" / entry["sample"]["rgba"]
            pixel.write_bytes(b"\0")
            with self.assertRaisesRegex(ValueError, "bytes changed"):
                with RasterSamples(directory / "output", INPUTS, None, observations=observation) as raster:
                    raster.raster(entry["request"]["source"], entry["case"])


if __name__ == "__main__":
    unittest.main()
