"""The preservation gate checks duplicate registrations as well as missing names."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import check_inventory


class InventoryTests(unittest.TestCase):
    def test_all_original_registrations_remain(self):
        report = check_inventory.check()
        self.assertTrue(report["success"], "\n".join(report["failures"]))

    def test_missing_duplicate_is_detected_but_new_tests_are_allowed(self):
        baseline = [{"name": "original", "count": 2}]
        self.assertEqual(check_inventory.check_names(Counter(original=2, additional=3), baseline), [])
        self.assertEqual(len(check_inventory.check_names(Counter(original=1), baseline)), 1)
        self.assertEqual(len(check_inventory.check_names(Counter(), baseline)), 1)

    def test_replaced_registration_requires_the_current_test(self):
        baseline = [{"name": "old scheduler", "count": 1, "replaced_by": "WinUI scheduler"}]
        self.assertEqual(check_inventory.check_names(Counter({"WinUI scheduler": 1}), baseline), [])
        self.assertEqual(check_inventory.check_names(Counter({"old scheduler": 1}), baseline),
                         ["Missing test registration: WinUI scheduler (0/1)"])

    def test_split_replacement_requires_every_current_test_with_the_recorded_count(self):
        baseline = [{"name": "old transfer", "count": 2, "replaced_by": ["session", "viewport"]}]
        self.assertEqual(check_inventory.check_names(Counter(session=2, viewport=2), baseline), [])
        self.assertEqual(check_inventory.check_names(Counter(session=2, viewport=1), baseline),
                         ["Missing test registration: viewport (1/2)"])
        self.assertEqual(check_inventory.check_names(Counter(session=0, viewport=2), baseline),
                         ["Missing test registration: session (0/2)"])

    def test_empty_or_invalid_replacements_do_not_waive_a_registration(self):
        for replacement in ([], "", None, ["session", "session"], ["session", 1]):
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                check_inventory.check_names(Counter(), [
                    {"name": "old transfer", "count": 1, "replaced_by": replacement}])

    def import_report(self, current: bytes):
        baseline = b'"retained"_test = [] { expect(value == 42); };\n'
        relative = "tests/platform/policy.cpp"
        manifest = {
            "schema": 1, "baseline_commit": "original",
            "cases": [{"name": "retained", "count": 1}],
            "windows_sources": {relative: hashlib.sha256(baseline).hexdigest()},
            "portable_import_additions": {relative: "import folia.markdown.selection;"},
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / relative
            path.parent.mkdir(parents=True)
            path.write_bytes(current)
            inventory = root / check_inventory.MANIFEST.relative_to(check_inventory.ROOT)
            inventory.parent.mkdir(parents=True)
            inventory.write_text(json.dumps(manifest), encoding="utf-8")
            return check_inventory.check(root, windows_baseline=True)

    def test_recorded_import_fix_cannot_hide_changed_or_removed_assertions(self):
        source = (b'import folia.markdown.selection;\n'
                  b'"retained"_test = [] { expect(value == 42); };\n')
        for current in (source, source.replace(b"\n", b"\r\n")):
            report = self.import_report(current)
            self.assertTrue(report["success"], report["failures"])
            self.assertEqual(report["portable_import_fixes"], 1)
            self.assertEqual(report["unchanged_windows_sources"], 0)
        for current in (source.replace(b"42", b"43"),
                        source.replace(b"expect(value == 42);", b"")):
            report = self.import_report(current)
            self.assertEqual(report["current_registrations"], 1)
            self.assertFalse(report["success"])
            self.assertEqual(report["portable_import_fixes"], 0)

    def test_import_exception_rejects_duplicates_other_imports_and_partial_lines(self):
        declaration = b"import folia.markdown.selection;\n"
        source = declaration + b'"retained"_test = [] { expect(value == 42); };\n'
        for current in (declaration + source, b"import std;\n" + source,
                        source.replace(declaration, b"// " + declaration)):
            with self.subTest(source=current):
                report = self.import_report(current)
                self.assertFalse(report["success"])
                self.assertEqual(report["verified_windows_sources"], 0)


if __name__ == "__main__":
    unittest.main()
