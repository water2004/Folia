"""Join source membership candidates, configured evidence and module edges."""

import json
import re
from functools import lru_cache

from .cmake import SOURCE_PATTERN, active_interfaces, cmake_closure, resolve_target_source_variables, target_bodies, target_sources
from .codemodel import read_configured_codemodel
from .files import Repository, relative, sha, unique
from .modules import is_source_file, module_facts, module_scan_candidates, resolve_module_edges, unresolved_refs


class Inventory:
    def __init__(self, root, *, build_dir=None, configuration="Debug"):
        self.repo = Repository(root)
        self.root = self.repo.root
        self.text_files = self.repo.files("src/math")
        self.source_files = [path for path in self.text_files if is_source_file(path)]
        self.ids = {path: f"F{index + 1:03d}" for index, path in enumerate(self.text_files)}
        self.facts = lru_cache(maxsize=None)(lambda path: module_facts(path, self.repo.read))
        self.cmake_files = cmake_closure(self.repo)
        self.cmake_text = "\n".join(self.repo.read(path) for path in self.cmake_files)
        self.cmake_refs = set(re.findall(SOURCE_PATTERN, self.cmake_text))
        self.target_sources = target_sources(self.cmake_text)
        self.math_refs = set(path for path in self.target_sources.get("folia_math", []) if path.startswith("src/math/"))
        self.lexical_unresolved_refs = unresolved_refs(self.math_refs, self.ids)
        self.existing_math_refs = sorted(path for path in self.math_refs if path in self.ids)
        self.test_refs = sorted(path for path in self.cmake_refs if path.startswith("tests/math/"))
        # This is the original conservative lexical scan, not configured membership.
        test_glob = [path for path in self.repo.files("tests") if path.endswith(".cpp")
                     and not path.startswith(("tests/platform/", "tests/encoding/", "tests/math/"))]
        self.target_sources["FoliaTests"] = unique([*self.target_sources.get("FoliaTests", []), *test_glob])
        self.active_interfaces = active_interfaces(self.cmake_text)
        self.font_manifest_path = "build/math-port/font-generated-native-raii/font-data-source-dependencies.json"
        self.font_manifest, self.font_hash = None, None
        if self.repo.exists(self.font_manifest_path):
            data = self.repo.bytes(self.font_manifest_path)
            try:
                self.font_manifest, self.font_hash = json.loads(data), sha(data)
            except ValueError as error:
                self.font_manifest = {"manifestError": str(error)}
        resolutions = [resolve_target_source_variables([body for _, body in target_bodies(self.cmake_text, target)], target,
                        {"fontDependencies": self.font_manifest, "fontDependenciesPath": self.font_manifest_path,
                         "fontDependenciesSha256": self.font_hash}) for target in self.target_sources]
        self.resolved_variables = [item for result in resolutions for item in result["resolved"]]
        self.unresolved_variables = [item for result in resolutions for item in result["unresolved"]]
        self.generated_font_paths = [path for item in self.resolved_variables for path in item["generatedCpp"]]
        self.generated_fonts = []
        for index, path in enumerate(self.generated_font_paths):
            unit = {"id": f"GF{index + 1:03d}", "path": path, "exists": self.repo.exists(path),
                    "origin": self.font_manifest_path, "manifestSha256": self.font_hash}
            if unit["exists"]:
                data = self.repo.bytes(path)
                unit.update(sha256=sha(data), bytes=len(data), **self.facts(path))
            self.generated_fonts.append(unit)
        self.missing_generated = [unit["path"] for unit in self.generated_fonts if not unit["exists"]]
        self.configured = read_configured_codemodel(self.root, generatedFontCpp=self.generated_font_paths,
                                                    cmakeFiles=self.cmake_files, build_dir=build_dir,
                                                    configuration_name=configuration)
        self.configured_targets, self.configured_math_sources = {}, set()
        if self.configured["authoritative"]:
            for target in self.configured["targets"]:
                for source in target["sources"]:
                    if source["path"].startswith("src/math/"):
                        self.configured_targets.setdefault(source["path"], []).append(target["name"])
            self.configured_math_sources = {source["path"] for source in self.configured["foliaMathSources"] if source["path"].startswith("src/math/")}
            self.existing_math_refs = sorted(self.configured_math_sources)
            self.active_interfaces = unique(source["path"] for source in self.configured["foliaMathSources"] if source["path"].endswith(".ixx"))
        self.providers, self.candidate_providers = {}, {}
        for path in self.active_interfaces:
            if path in self.ids:
                facts = self.facts(path)
                if facts["isInterface"] and facts["declaration"]:
                    self.providers.setdefault(facts["declaration"], []).append(path)
        for path in self.source_files:
            facts = self.facts(path)
            if facts["isInterface"] and facts["declaration"] and path not in self.active_interfaces:
                self.candidate_providers.setdefault(facts["declaration"], []).append(path)
        self.duplicate_providers = [{"module": name, "files": paths} for name, paths in self.providers.items() if len(paths) > 1]
        self.module_map_path = relative(self.root, (build_dir or self.root / "build/core") / "CMakeFiles/folia_math.dir/CXXModules.json")
        self.module_map = None
        if self.repo.exists(self.module_map_path):
            try:
                data = self.repo.bytes(self.module_map_path)
                parsed = json.loads(data)
                self.module_map = {"path": self.module_map_path, "sha256": sha(data), "bytes": len(data), "modules": sorted(parsed.get("modules", {}))}
            except ValueError as error:
                self.module_map = {"path": self.module_map_path, "error": str(error)}
        self.interface_check = self.make_interface_check()
        self.active_sources = set([*self.existing_math_refs, *self.generated_font_paths])
        self.module_paths = module_scan_candidates([*self.source_files, *self.generated_font_paths], self.repo.exists)["paths"]
        self.edges, self.unresolved_imports = {}, []
        for path in self.module_paths:
            result = resolve_module_edges(path, self.facts(path), self.providers, self.active_sources)
            self.edges[path] = result["edges"]
            self.unresolved_imports.extend(result["unresolved"])
        self.forward_risks = []
        for path in self.source_files:
            for symbol in self.facts(path)["forwardDeclarations"]:
                for edge in self.edges.get(path, []):
                    if re.search(r"\b" + re.escape(symbol) + r"\b", self.repo.read(edge["file"])):
                        self.forward_risks.append([self.ids[path], symbol, edge["module"], self.ids.get(edge["file"])])
        self.active = set(self.active_sources)
        queue = list(self.active)
        for path in queue:
            for edge in self.edges.get(path, []):
                if edge["file"] not in self.active:
                    self.active.add(edge["file"])
                    queue.append(edge["file"])
        self.font_lock_path = "docs/math-port/evidence/font-data-source-lock.json"
        font_lock = json.loads(self.repo.read(self.font_lock_path))
        outputs = [item for item in font_lock["outputs"] if re.fullmatch(r"build/math-port/font-generated/newcm_dynamic_setup_[A-Za-z0-9_]+\.cpp", item["path"])]
        self.font_candidates = [{"id": f"G{index + 1:02d}", "target": "folia_math", **{key: unit[key] for key in ("path", "sha256", "bytes")},
                                 "lock": self.font_lock_path} for index, unit in enumerate(outputs)]
        for unit in self.generated_fonts:
            unit["resolvedImports"] = [{"writtenImport": edge["module"], **{key: edge[key] for key in ("resolvedModule", "file", "kind")}}
                                       for edge in self.edges.get(unit["path"], [])]
            unit["unresolvedImports"] = [edge for edge in self.unresolved_imports if edge["file"] == unit["path"]]

    def make_interface_check(self):
        if self.configured["authoritative"]:
            check = self.configured["moduleMapCheck"]
            return {"available": True, "authoritative": True, "status": "configured-target-providers-compared-to-generated-module-map",
                    "generatedModuleCount": check["moduleMapCount"], "registeredInterfaceCount": check["providerCount"],
                    "matches": check["exactMatch"], "missingFromGenerated": check["missingFromMap"],
                    "unexpectedInGenerated": check["missingProviders"], "duplicateProviders": check["duplicateProviders"]}
        names = sorted(self.providers)
        generated = (self.module_map or {}).get("modules", [])
        return {"available": self.module_map is not None and "error" not in self.module_map, "authoritative": False,
                "status": "lexical-candidate-only-no-configured-codemodel", "generatedModuleCount": len(generated),
                "registeredInterfaceCount": len(names), "matches": None,
                "missingFromGenerated": [name for name in names if name not in generated],
                "unexpectedInGenerated": [name for name in generated if name not in names]}
