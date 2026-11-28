"""Keep all seven independent Source packets and their original checks intact."""
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ROOT, ComparisonError, read
from validation.observer_packets import CONFIG, compare_packet, encode_original, legacy_inputs, replay


def entries():
    return {packet: entry for group in read(CONFIG).values() for packet, entry in group.items()}


def retained_observations(entry, expected, legacy):
    """Re-express the frozen raw samples, never regenerate expected values."""
    kind = entry["comparison"]
    if kind == "leaf":
        return {"profile": copy.deepcopy(expected["profile"]), "sources": 10}
    if kind == "mml-node":
        return copy.deepcopy(expected["profile"])
    if kind == "mathchoice":
        return {"records": copy.deepcopy(expected["records"]),
                "sourceMetadata": [row[:-1] for row in legacy["sourceProfile"]]}
    return copy.deepcopy(expected)


class ObserverPacketTests(unittest.TestCase):
    def test_leaf_and_mml_node_open_manifest_paths_and_preserve_historical_records(self):
        catalog = entries()
        for packet in ("leaf", "mml-node"):
            entry = catalog[packet]
            expected = read(ROOT / entry["profile"])
            with self.subTest(packet=packet):
                historical = expected["legacy" if packet == "leaf" else "originalProfile"]
                self.assertEqual(historical["path"].split("/")[:2], ["docs", "math-port"])
                self.assertEqual(entry["legacy"]["path"].split("/")[:3],
                                 ["tools", "math-port", "profiles"])
                live = legacy_inputs(entry, expected)
                self.assertEqual(entry["legacy"]["sha256"], historical["sha256"])
                self.assertEqual(entry["legacy"]["bytes"], historical["bytes"])
                self.assertEqual(live, read(ROOT / entry["legacy"]["path"]))

    def test_all_seven_packets_retain_full_observations_and_summary(self):
        catalog = entries()
        self.assertEqual(len(catalog), 7)
        for packet, entry in catalog.items():
            with self.subTest(packet=packet):
                expected = read(ROOT / entry["profile"])
                legacy = legacy_inputs(entry, expected)
                actual = retained_observations(entry, expected, legacy)
                report = compare_packet(packet, entry, actual, expected, legacy=legacy)
                self.assertEqual(report["mode"], "check")
                # Changing any list's membership/order, even far past the first
                # sample, must fail instead of validating only summary counts.
                profile = actual.get("profile", actual)
                for field, rows in profile.items():
                    if not isinstance(rows, list) or len(rows) < 2:
                        continue
                    for mutation in ("missing", "extra", "order", "value"):
                        with self.subTest(field=field, mutation=mutation):
                            changed = copy.deepcopy(actual)
                            target = changed.get("profile", changed)[field]
                            if mutation == "missing":
                                target.pop()
                            elif mutation == "extra":
                                target.append(copy.deepcopy(target[-1]))
                            elif mutation == "order":
                                target[0], target[-1] = target[-1], target[0]
                            else:
                                target[-1] = {"corrupted-observation": True}
                            with self.assertRaises((ComparisonError, TypeError, KeyError, IndexError)):
                                compare_packet(packet, entry, changed, expected, legacy=legacy)

    def test_original_mathchoice_encoding_matches_all_409_independent_records(self):
        entry = entries()["mathchoice"]
        canonical = read(ROOT / entry["profile"])
        original = read(ROOT / entry["legacy"]["path"])
        # These files were frozen separately in different formats before this
        # migration. The entire original binary64/object encoding must survive.
        self.assertEqual(len(original["sourceProfile"]), 409)
        for record, source in zip(canonical["records"], original["sourceProfile"]):
            with self.subTest(case=record["name"]):
                self.assertEqual(encode_original(record["observations"][0]), source[-1])

    def test_mathchoice_special_numbers_undefined_and_array_index_keys(self):
        cases = [
            ({"$type": "undefined"}, ["undefined"]),
            ({"$type": "number", "value": "-0"}, ["number", "0000000000000080"]),
            ({"$type": "number", "value": "NaN"}, ["number", "000000000000f87f"]),
            ({"$type": "number", "value": "Infinity"}, ["number", "000000000000f07f"]),
            ({"$type": "number", "value": "-Infinity"}, ["number", "000000000000f0ff"]),
            (True, ["boolean", True]),
            (None, ["null"]),
            ([False, "x"], ["array", 2, [["0", ["boolean", False]], ["1", ["string", "x"]]]]),
        ]
        for actual, expected in cases:
            with self.subTest(actual=actual):
                self.assertEqual(encode_original(actual), expected)
        encoded = encode_original({"name": None, "10": None, "2": None, "01": None})
        self.assertEqual([row[0] for row in encoded[2]], ["2", "10", "name", "01"])

    def test_mathchoice_original_names_counts_and_metadata_are_independent_gates(self):
        entry = entries()["mathchoice"]
        expected = read(ROOT / entry["profile"])
        original = legacy_inputs(entry, expected)
        actual = retained_observations(entry, expected, original)
        for field in ("names", "assertions", "cases", "sourceProfile"):
            with self.subTest(field=field):
                changed = copy.deepcopy(original)
                if isinstance(changed[field], list):
                    changed[field].pop()
                else:
                    changed[field] -= 1
                with self.assertRaises(ComparisonError):
                    compare_packet("mathchoice", entry, actual, expected, legacy=changed)
        changed = copy.deepcopy(actual)
        changed["sourceMetadata"][0][1] = "changed-call-name"
        with self.assertRaisesRegex(ComparisonError, "original Source profile"):
            compare_packet("mathchoice", entry, changed, expected, legacy=original)

    def test_ordered_records_reject_observation_key_reordering(self):
        entry = entries()["mathchoice"]
        expected = read(ROOT / entry["profile"])
        original = legacy_inputs(entry, expected)
        actual = retained_observations(entry, expected, original)
        first = actual["records"][0]["observations"][0]
        self.assertGreater(len(first), 1)
        actual["records"][0]["observations"][0] = dict(reversed(list(first.items())))
        with self.assertRaisesRegex(ComparisonError, "keys"):
            compare_packet("mathchoice", entry, actual, expected, legacy=original)

    def test_canonical_and_legacy_integrity_fail_before_observation_process(self):
        entry = entries()["leaf"]
        expected = read(ROOT / entry["profile"])
        runner = Mock()
        changed = dict(entry, sha256="0" * 64)
        with self.assertRaisesRegex(ComparisonError, "Frozen Source profile"):
            replay("leaf", changed, runner=runner)
        runner.assert_not_called()
        changed = dict(entry, legacy=dict(entry["legacy"], sha256="0" * 64))
        with self.assertRaisesRegex(ComparisonError, "Original observer profile"):
            replay("leaf", changed, runner=runner)
        runner.assert_not_called()
        changed = copy.deepcopy(expected)
        changed["legacy"]["bytes"] += 1
        with self.assertRaisesRegex(ComparisonError, "size"):
            legacy_inputs(entry, changed)
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(FileNotFoundError):
                legacy_inputs(entry, expected, root=Path(folder))

    def test_source_failure_timeout_invalid_json_and_oversize_never_pass(self):
        entry = entries()["leaf"]
        failures = [
            subprocess.CompletedProcess([], 3, b"{}", b"upstream source lock changed"),
            subprocess.CompletedProcess([], 0, b"not JSON", b""),
            subprocess.CompletedProcess([], 0, b" " * (64 * 1024 * 1024 + 1), b""),
        ]
        for failure in failures:
            with self.subTest(status=failure.returncode, bytes=len(failure.stdout)):
                runner = Mock(return_value=failure)
                with self.assertRaises((ComparisonError, ValueError)):
                    replay("leaf", entry, runner=runner)
                self.assertEqual(runner.call_args.kwargs["timeout"], 60)
                self.assertNotIn("--check", runner.call_args.args[0])
        runner = Mock(side_effect=subprocess.TimeoutExpired(["node"], 60))
        with self.assertRaises(subprocess.TimeoutExpired):
            replay("leaf", entry, runner=runner)


if __name__ == "__main__":
    unittest.main()
