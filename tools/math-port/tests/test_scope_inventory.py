"""Scope inventory preserves pins, import order and reviewed architectural names."""

import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation import scope


class ScopeInventoryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.sources = self.root / ".reference/MathJax-src"
        self.fixture = json.loads(Path(__file__).with_name("fixtures").joinpath("scope-imports.json").read_bytes())
        for path, text in self.fixture["sources"].items():
            self.write(self.sources / path, text)
        self.write(self.root / "tools/mathjax-bundle/entry.cjs", self.fixture["profile"])
        self.lock = {"packages": {"node_modules/@mathjax/" + name: {
            "version": "4.1.3", "integrity": "fixture-integrity-" + name} for name in scope.FONT_PACKAGES}}
        self.write(self.root / "tools/mathjax-bundle/package-lock.json", json.dumps(self.lock))

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode("utf-8"))

    def inventory(self, prior=None):
        return scope.manifest(self.root, self.sources, prior)

    def test_import_closure_deduplicates_and_preserves_static_declarations(self):
        result = self.inventory()
        self.assertEqual([row["source"] for row in result["files"]], sorted(self.fixture["sources"]))
        self.assertEqual(result["profile"]["tex_packages"], ["base", "ams"])
        self.assertEqual(result["unresolved_imports"], [])
        rows = {row["source"]: row for row in result["files"]}
        for path, dependencies in self.fixture["dependencies"].items():
            self.assertEqual(rows[path]["dependencies"], dependencies)
        self.assertEqual(rows["ts/input/tex.ts"]["external_imports"], [])
        self.assertEqual(rows["ts/output/svg.ts"]["external_imports"], ["external-package"])
        for path, text in self.fixture["sources"].items():
            self.assertEqual(rows[path]["source_sha256"], scope.digest(text))
        for source, expected in self.fixture["snake"].items():
            self.assertEqual(scope.snake(source), expected)

    def test_unresolved_imports_are_retained_with_source_and_candidate(self):
        path = self.sources / "ts/util/BBox.ts"
        self.write(path, "import './missing.js';\n")
        self.assertEqual(self.inventory()["unresolved_imports"], [{
            "source": "ts/util/BBox.ts", "specifier": "./missing.js", "candidate": "ts/util/missing.ts"}])

    def test_reviewed_names_survive_only_exact_upstream_bytes(self):
        prior = self.inventory()
        row = prior["files"][0]
        row.update(disposition="native", adaptation_reason="reviewed architecture", port_status="implemented",
                   acceptance="accepted", proposed_target="src/math/reviewed.ixx",
                   proposed_module="reviewed.module", owner="reviewed-owner")
        self.write(self.root / row["proposed_target"], "export module reviewed.module;\n")
        self.assertEqual(self.inventory(prior)["files"][0], row)
        path = self.sources / row["source"]
        path.write_bytes(path.read_bytes() + b"\n")
        changed = self.inventory(prior)["files"][0]
        self.assertEqual(changed["disposition"], "unreviewed")
        self.assertNotEqual(changed["proposed_module"], "reviewed.module")

    def test_missing_accepted_native_target_fails_closed(self):
        prior = self.inventory()
        prior["files"][0]["port_status"] = "implemented"
        with self.assertRaisesRegex(ValueError, "Accepted native target is missing"):
            self.inventory(prior)

    def test_missing_source_and_unpinned_font_are_not_silently_skipped(self):
        path = self.sources / "ts/input/tex.ts"
        data = path.read_bytes()
        path.unlink()
        with self.assertRaisesRegex(ValueError, "Missing source"):
            self.inventory()
        path.write_bytes(data)
        self.lock["packages"]["node_modules/@mathjax/" + scope.FONT_PACKAGES[0]]["version"] = "latest"
        self.write(self.root / "tools/mathjax-bundle/package-lock.json", json.dumps(self.lock))
        with self.assertRaisesRegex(ValueError, "Unpinned font"):
            self.inventory()

    def test_commit_and_ordered_manifest_checks(self):
        with patch.object(scope.subprocess, "check_output", return_value="wrong-commit\n"):
            with self.assertRaisesRegex(ValueError, "Unexpected MathJax source commit"):
                scope.generate(self.root)
        with patch.object(scope.subprocess, "check_output", return_value=scope.EXPECTED_COMMIT + "\n"):
            original = scope.generate(self.root)
            self.assertEqual(scope.generate(self.root, check=True), original)
            changed = copy.deepcopy(original)
            changed["profile"]["tex_packages"].reverse()
            self.write(self.root / "docs/math-port/scope.json", json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "Scope inventory differs"):
                scope.generate(self.root, check=True)


if __name__ == "__main__":
    unittest.main()
