"""Contract tests for the shipped, registry-derived math completion data."""
import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("latex_catalog", ROOT / "tools/latex-completion/generate.py")
catalog = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(catalog)


class LatexCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = catalog.build_catalog()
        cls.commands = {c["trigger"]: c for c in cls.data["commands"]}

    def test_shipped_catalog_matches_its_sources(self):
        self.assertEqual(catalog.OUTPUT.read_text(encoding="utf-8"), catalog.render(self.data))

    def test_every_named_registered_command_is_completable(self):
        self.assertGreater(len(self.commands), 1500)
        for entry in catalog.registry():
            name = catalog.command_name(entry)
            if entry["kind"] != "environment" and catalog.eligible(name):
                self.assertIn(name, self.commands, (entry["map"], name))

    def test_every_environment_has_an_open_and_close_completion(self):
        for entry in catalog.registry():
            if entry["kind"] != "environment":
                continue
            name = entry["name"]
            opening = self.commands["begin{" + name + "}"]["snippet"]
            self.assertTrue(opening.startswith("\\begin{" + name + "}"), name)
            self.assertIn("\\end{" + name + "}", opening)
            self.assertEqual(self.commands["end{" + name + "}"]["snippet"], "\\end{" + name + "}$0")

    def test_argument_and_environment_templates(self):
        for name in ["boldsymbol", "mathbfit", "cancelto", "ce", "pu", "dv", "pdv",
                     "DeclarePairedDelimiter", "unicode", "xhookrightarrow"]:
            self.assertIn("$1", self.commands[name]["snippet"].replace("${1:", "$1"), name)
        for name in ["array", "darray", "subarray", "alignat", "alignedat", "empheq", "spreadlines"]:
            self.assertIn("${1:", self.commands["begin{" + name + "}"]["snippet"], name)

    def test_native_meanings_do_not_conflict_with_script_shortcuts(self):
        self.assertEqual(self.commands["sub"]["snippet"], "\\sub$0")
        self.assertEqual(self.commands["sup"]["snippet"], "\\sup$0")
        self.assertEqual(self.commands["subscript"]["snippet"], "_{$1}$0")
        self.assertEqual(self.commands["superscript"]["snippet"], "^{$1}$0")
        self.assertEqual(self.commands["frac"]["id"], "structure.frac")
        self.assertEqual(self.commands["alpha"]["id"], "symbol.alpha")

    def test_ids_triggers_and_final_stops_are_valid(self):
        ids = set()
        for item in self.data["commands"]:
            self.assertNotIn(item["id"], ids)
            ids.add(item["id"])
            self.assertTrue(item["trigger"])
            self.assertTrue(all(ord(c) > 32 and c != "\\" for c in item["trigger"]))
            self.assertTrue(item["snippet"].endswith("$0"), item["trigger"])
        self.assertEqual(len(self.commands), len(ids))
        self.assertEqual(self.commands["$"]["snippet"], "\\$$$0")

    def test_new_parser_methods_require_an_explicit_template(self):
        entry = dict(kind="macro", name="futureCommand", map="future", symbol="",
                     method="FutureParserMethod", args=[])
        with patch.object(catalog, "registry", return_value=[entry]):
            with self.assertRaisesRegex(ValueError, "FutureParserMethod"):
                catalog.build_catalog()


if __name__ == "__main__":
    unittest.main()
