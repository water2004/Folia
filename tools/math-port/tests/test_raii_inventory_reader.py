"""Real temporary File API replies, staleness failures and source graph facts."""

import json
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.raii.cmake import active_interfaces, cmake_closure, resolve_target_source_variables
from validation.raii.codemodel import read_configured_codemodel
from validation.raii.files import Repository, discover_root
from validation.raii.graph import Inventory
from validation.raii.modules import module_facts, resolve_module_edges
from validation.raii.provenance import SourceAnnotations, responsibility
from validation.raii.reports import file_rows, graph_report


FIXTURE = Path(__file__).with_name("fixtures") / "raii-file-api.json"
STAMP = 1_700_000_000


class RaiiFileApiChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        files = json.loads(FIXTURE.read_text().replace("$ROOT", self.root.as_posix()))
        for path, value in files.items():
            self.write(path, value)
        (self.root / "tools/math-port").mkdir(parents=True)
        (self.root / "tests").mkdir()
        self.reply = self.root / "build/core/.cmake/api/v1/reply"

    def write(self, path, value, stamp=STAMP):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(value if isinstance(value, str) else json.dumps(value), encoding="utf-8")
        os.utime(target, (stamp, stamp))

    def change_reply(self, name, change):
        path = self.reply / name
        value = json.loads(path.read_bytes())
        change(value)
        self.write(path, value)

    def read(self, **options):
        return read_configured_codemodel(self.root, cmakeFiles=["cmake/FoliaMath.cmake"], **options)

    def test_complete_file_api_reply_is_authoritative(self):
        result = self.read()
        self.assertTrue(result["authoritative"])
        self.assertEqual(result["foliaMathSourceCount"], 3)
        self.assertTrue(result["moduleMapCheck"]["exactMatch"])
        self.assertEqual(result["bridgeTargets"], ["FoliaNativeMath"])
        self.assertEqual(len(result["evidence"]["targets"]), 2)

    def test_included_cmake_module_retains_registered_interfaces(self):
        state = Inventory(self.root)
        self.assertEqual(state.cmake_files, ["CMakeLists.txt", "cmake/FoliaMath.cmake"])
        self.assertEqual(state.active_interfaces, ["src/math/a.ixx", "src/math/b.ixx"])
        self.assertEqual(graph_report(state)["unresolvedActiveImports"], [])
        self.assertEqual(graph_report(state)["implicitPrimaryInterfaceEdges"], [["F001", "F002"]])
        self.assertTrue(all(row["disposition"] == "unreviewed" for row in file_rows(state)))

    def test_invalid_newest_index_does_not_fall_back(self):
        self.write(self.reply / "index-newer.json", "{invalid", STAMP + 1)
        result = self.read()
        self.assertEqual((result["status"], result["authoritative"]), ("malformed", False))
        self.assertEqual(result["indexFile"], "index-newer.json")

    def test_new_index_without_query_uses_last_matching_reply(self):
        self.write(self.reply / "index-other.json", {"reply": {}}, STAMP + 1)
        self.assertTrue(self.read()["authoritative"])

    def test_no_matching_query_is_missing(self):
        self.change_reply("index-fixture.json", lambda value: value.clear())
        self.assertEqual(self.read()["status"], "missing")

    def test_invalid_codemodel(self):
        self.write(self.reply / "codemodel.json", "not json")
        self.assertEqual(self.read()["status"], "malformed")

    def test_wrong_codemodel_source_root(self):
        self.change_reply("codemodel.json", lambda value: value["paths"].update(source=str(self.root / "other")))
        self.assertEqual(self.read()["status"], "wrong-source-root")

    def test_wrong_codemodel_build_root(self):
        self.change_reply("codemodel.json", lambda value: value["paths"].update(build=str(self.root / "other")))
        self.assertEqual(self.read()["status"], "incomplete")

    def test_missing_debug_configuration(self):
        self.change_reply("codemodel.json", lambda value: value["configurations"][0].update(name="Release"))
        self.assertEqual(self.read()["reason"], "Debug-configuration-not-present")
        self.assertTrue(self.read(configuration_name="Release")["authoritative"])

    def test_malformed_target_list(self):
        self.change_reply("codemodel.json", lambda value: value["configurations"][0].update(targets={}))
        self.assertEqual(self.read()["reason"], "configuration-targets-not-array")

    def test_empty_target_reference(self):
        self.change_reply("codemodel.json", lambda value: value["configurations"][0]["targets"][0].update(jsonFile=""))
        self.assertEqual(self.read()["reason"], "invalid-target-reference")

    def test_missing_target_reply(self):
        (self.reply / "math.json").unlink()
        self.assertEqual(self.read()["status"], "malformed")

    def test_target_source_list_malformed(self):
        self.change_reply("math.json", lambda value: value.update(sources=None))
        self.assertEqual(self.read()["reason"], "target-folia_math-sources-not-array")

    def test_target_source_root_outside_repository(self):
        self.change_reply("math.json", lambda value: value["paths"].update(source=".."))
        self.assertEqual(self.read()["status"], "wrong-source-root")

    def test_malformed_directory_list(self):
        self.change_reply("codemodel.json", lambda value: value["configurations"][0].update(directories=[{}]))
        self.assertEqual(self.read()["reason"], "configuration-directories-invalid")

    def test_missing_configure_input(self):
        (self.root / "cmake/FoliaMath.cmake").unlink()
        result = self.read()
        self.assertEqual(result["status"], "incomplete")
        self.assertEqual(result["missingInputs"], ["cmake/FoliaMath.cmake"])
        self.assertIn("evidence", result)

    def test_newer_shared_cmake_input(self):
        path = self.root / "cmake/FoliaMath.cmake"
        os.utime(path, (STAMP + 1, STAMP + 1))
        result = self.read()
        self.assertEqual(result["status"], "stale-inputs")
        self.assertEqual(result["newerInputs"], ["cmake/FoliaMath.cmake"])

    def test_missing_generated_module_map(self):
        (self.root / "build/core/CMakeFiles/folia_math.dir/CXXModules.json").unlink()
        self.assertEqual(self.read()["status"], "incomplete")

    def test_malformed_generated_module_map(self):
        self.write("build/core/CMakeFiles/folia_math.dir/CXXModules.json", {"modules": []})
        self.assertEqual(self.read()["status"], "malformed")

    def test_missing_source_file(self):
        (self.root / "src/math/a.cpp").unlink()
        self.assertEqual(self.read()["badPaths"], ["folia_math:src/math/a.cpp"])

    def test_generated_font_membership_required(self):
        result = self.read(generatedFontCpp=["build/font/a.cpp"])
        self.assertEqual(result["status"], "stale-inputs")
        self.assertEqual(result["missingFontInputs"], ["build/font/a.cpp"])

    def test_unlisted_generated_font_input_rejected(self):
        path = "build/math-port/font-generated-native-raii/extra.cpp"
        self.write(path, "module fixture.a;")
        self.change_reply("math.json", lambda value: value["sources"].append({"path": path, "isGenerated": True}))
        self.assertEqual(self.read()["extraFontInputs"], [path])

    def test_copied_reply_directory_rejected(self):
        other = self.root / "other-replies"
        shutil.copytree(self.reply, other)
        self.assertEqual(self.read(replyDir=other)["reason"], "file-api-reply-directory-does-not-match-codemodel-build-root")

    def test_explicit_existing_build_directory(self):
        original = self.root / "build/core"
        target = self.root / "build/portable"
        shutil.copytree(original, target)
        model = target / ".cmake/api/v1/reply/codemodel.json"
        value = json.loads(model.read_bytes())
        value["paths"]["build"] = str(target)
        self.write(model, value)
        result = self.read(build_dir=target)
        self.assertTrue(result["authoritative"])
        self.assertEqual(result["buildRoot"], str(target))

    def test_discover_root_from_new_subdirectory(self):
        self.assertEqual(discover_root(self.root / "src/math"), self.root)

    def test_cmake_include_cycle_is_bounded(self):
        self.write("cmake/FoliaMath.cmake", 'include("${CMAKE_CURRENT_LIST_DIR}/FoliaMath.cmake")\n')
        self.assertEqual(cmake_closure(Repository(self.root)), ["CMakeLists.txt", "cmake/FoliaMath.cmake"])

    def test_file_set_base_dirs_is_a_path_prefix(self):
        text = (self.root / "cmake/FoliaMath.cmake").read_text()
        self.assertEqual(active_interfaces(text), ["src/math/a.ixx", "src/math/b.ixx"])
        result = resolve_target_source_variables(['target_sources(t PRIVATE ${FOLIA_SOURCE_ROOT}/src/math/a.cpp ${UNKNOWN})'], "t", {})
        self.assertEqual([item["variable"] for item in result["unresolved"]], ["UNKNOWN"])

    def test_implementation_unit_never_supplies_a_provider(self):
        facts = module_facts("a.cpp", lambda _: "module fixture.a;\nexport import fixture.b;\nstruct B;\n")
        self.assertFalse(facts["isInterface"])
        self.assertEqual(facts["forwardDeclarations"], ["B"])
        result = resolve_module_edges("a.cpp", facts, {"fixture.a": ["a.ixx", "duplicate.ixx"]}, {"a.cpp"})
        self.assertEqual([item["reason"] for item in result["unresolved"]], ["no-registered-interface-provider", "duplicate-active-primary-interface-providers"])

    def test_annotations_remain_notes_and_licenses_are_preserved(self):
        hash_value = "a" * 64
        self.write("docs/math-port/mapping.md", f"| src/math/a.ixx | ts/fixture/A.ts | {hash_value} |\n")
        state = Inventory(self.root)
        rows = SourceAnnotations(state.repo, state.facts).for_file("src/math/a.ixx", state.facts("src/math/a.ixx"))
        self.assertEqual(rows[0]["sourceSha256"], hash_value)
        self.assertEqual(rows[0]["hashEvidence"], "adjacent table cell")
        self.assertEqual(responsibility("src/math/dependencies/NOTICE.txt")["decision"], "keep-license")
        self.assertEqual(responsibility("src/math/a.ixx")["decision"], "unreviewed")
