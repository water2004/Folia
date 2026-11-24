"""Every actual assertion from the former native inventory self-test."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.raii.cmake import resolve_target_source_variables
from validation.raii.codemodel import (codemodel_envelope_status, configured_codemodel_summary,
                                      evaluate_configured_target_data, read_configured_codemodel)
from validation.raii.modules import is_source_file, module_scan_candidates, resolve_module_edges, unresolved_refs


FIXTURES = json.loads((Path(__file__).with_name("fixtures") / "raii-original-checks.json").read_bytes())


def evaluate(name):
    operation = FIXTURES["operations"][name]
    args, kwargs = operation["args"], dict(operation["kwargs"])
    function = operation["function"]
    if function == "evaluate":
        texts = kwargs.pop("texts")
        return evaluate_configured_target_data(*args, **kwargs, sourceText=texts.__getitem__)
    if function == "summary":
        return configured_codemodel_summary({**evaluate(args[0]), **args[1]})
    if function == "scan":
        return module_scan_candidates(args[0], lambda path: path in args[1])
    if function == "reader":
        with tempfile.TemporaryDirectory() as directory:
            return read_configured_codemodel(directory, replyDir=Path(directory) / "no-file-api-replies")
    return {"source": is_source_file, "refs": unresolved_refs, "edges": resolve_module_edges,
            "variables": resolve_target_source_variables, "envelope": codemodel_envelope_status}[function](*args, **kwargs)


class OriginalRaiiInventoryChecks(unittest.TestCase):
    """The old banner said 55; the executable source contained 53 assert calls."""


def register(case):
    def check(self):
        result = evaluate(case["operation"])
        for key in case["path"]:
            result = result.get(key, {"$undefined": True}) if isinstance(result, dict) else result[key]
        projection = case.get("projection", {})
        if "length" in projection:
            result = len(result)
        elif "contains" in projection:
            result = projection["contains"] in result
        elif "keys" in projection:
            result = [[item[key] for key in projection["keys"]] for item in result]
        elif "prepend" in projection:
            result = projection["prepend"] + result
        elif "membership" in projection:
            wanted = projection["membership"]
            result = any(item["name"] == wanted["name"] and wanted["path"] in item["paths"] for item in result)
        self.assertEqual(result, case["expected"])
    setattr(OriginalRaiiInventoryChecks, "test_" + case["id"].replace("-", "_"), check)


for case in FIXTURES["checks"]:
    register(case)
