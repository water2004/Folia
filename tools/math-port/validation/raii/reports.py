"""Full source-fact report and compact, explicitly unreviewed decision index."""

import json
import re
import subprocess
import time
from pathlib import Path

from .codemodel import configured_codemodel_summary
from .files import sha, unique, utc
from .provenance import SourceAnnotations, responsibility


def file_rows(state):
    annotation_index = SourceAnnotations(state.repo, state.facts)
    result = []
    for path in state.text_files:
        decision = responsibility(path)
        facts, data = state.facts(path), state.repo.bytes(path)
        annotations = annotation_index.for_file(path, facts)
        exact = {(source, item["sourceSha256"]) for item in annotations if item["sourceSha256"]
                 for source in item["sourcePaths"] if "ts/" in source}
        index_status = "one-note-index-entry" if len(exact) == 1 else "multiple-note-index-entries" if exact else "no-exact-note-index-entry"
        configured = state.configured_targets.get(path, [])
        result.append({"id": state.ids[path], "path": path, "sha256": sha(data), "bytes": len(data), "kind": Path(path).suffix[1:], **facts,
                       "directlyListedByFoliaMath": path in state.configured_math_sources if state.configured["authoritative"] else None,
                       "lexicallyListedByFoliaMath": path in state.math_refs,
                       "activeModuleImportClosure": path in state.active if state.configured["authoritative"] else None,
                       "mentionedByCMake": path in state.cmake_refs, "configuredTargets": configured,
                       "directCMakeTargets": unique([*[target for target, paths in state.target_sources.items() if path in paths], *configured]),
                       "disposition": decision["decision"], "nativeResponsibility": decision["responsibilityGroup"],
                       "responsibilityGroup": decision["responsibilityGroup"], "namedOwner": decision["namedOwner"],
                       "decisionStatus": "license-preservation" if decision["decision"] == "keep-license" else "unreviewed",
                       "sourceAnnotations": annotations, "sourceAnnotationIndexStatus": index_status,
                       "sourceAnnotationNote": "Index heuristic only; not algorithm equivalence, complete Source mapping, or disposition evidence.",
                       "confidence": "responsibility grouping and annotation index only; no file disposition or algorithm-equivalence conclusion"})
    return result


def generated_rows(state):
    return [{"path": unit["path"], "decisionStatus": "unreviewed", "responsibilityGroup": "math.fonts.generated-newcm",
             "namedOwner": "unassigned", "configuredTargetMember": state.configured["authoritative"] and any(source["path"] == unit["path"] for source in state.configured["foliaMathSources"]),
             "availableSourceProvenance": [{"document": state.font_manifest_path, "manifestSha256": state.font_hash,
                                            "sourceLock": (state.font_manifest or {}).get("source_lock"),
                                            "status": "dependency-manifest note only; no per-unit pinned TS mapping or algorithm-equivalence assertion"}]}
            for unit in state.generated_fonts]


def source_scope(state):
    authoritative = state.configured["authoritative"]
    return {"allSrcMathFiles": len(state.text_files), "codeSources": len(state.source_files),
            "moduleUnits": sum(path.endswith(".ixx") for path in state.source_files),
            "implementationUnits": sum(path.endswith(".cpp") for path in state.source_files),
            "otherSourceFiles": sum(not path.endswith((".ixx", ".cpp")) for path in state.source_files),
            "currentCMakeLiteralFoliaMathReferences": len(state.math_refs), "existingCMakeDirectFoliaMathFiles": len(state.existing_math_refs),
            "currentCMakeResolvedFoliaMathSourceFiles": state.configured["foliaMathSourceCount"] if authoritative else len(state.existing_math_refs) + sum(unit["exists"] for unit in state.generated_fonts),
            "unresolvedFoliaMathSourceVariableExpressions": 0 if authoritative else sum(item["target"] == "folia_math" for item in state.unresolved_variables),
            "missingGeneratedFoliaMathSources": len(state.missing_generated), "unresolvedCMakeDirectFoliaMathFiles": 0 if authoritative else None,
            "lexicalCandidateUnresolvedMathReferences": len(state.lexical_unresolved_refs),
            "moduleImportClosureFiles": len(state.active) if authoritative else None,
            "filesOutsideFoliaMathModuleClosure": sum(path not in state.configured_math_sources and path not in state.active for path in state.text_files) if authoritative else None,
            "allTargetCMakeMentions": len(state.cmake_refs), "generatedFoliaMathFontSources": len(state.generated_fonts),
            "lockedButInactiveFontShardCandidates": len(state.font_candidates), "sourceMembershipAuthority": state.configured["status"]}


def graph_report(state):
    authoritative = state.configured["authoritative"]
    file_id = lambda path: state.ids.get(path, path)
    resolved_units = [{key: unit[key] for key in ("path", "exists", "sha256", "bytes", "declaration", "isInterface", "isImplementationUnit",
                      "primaryModule", "partition", "imports", "resolvedImports", "unresolvedImports") if key in unit} for unit in state.generated_fonts]
    module_edges, implicit = [], []
    for path in state.module_paths:
        for item in state.facts(path)["imports"]:
            resolved = next((edge for edge in state.edges[path] if edge["module"] == item["module"]), None)
            module_edges.append([file_id(path), item["module"], file_id(resolved["file"]) if resolved else None, item["kind"], item["exported"]])
        if path in state.active_sources and state.facts(path)["isImplementationUnit"]:
            implicit.extend([file_id(path), file_id(edge["file"])] for edge in state.edges.get(path, []) if edge["kind"] == "implicit-primary-interface")
    return {"cmakeFiles": state.cmake_files, "cmakeMathSources": [state.ids[path] for path in sorted(state.existing_math_refs)],
            "unresolvedCMakeMathSources": [] if authoritative else None, "lexicalCandidateUnresolvedMathReferences": state.lexical_unresolved_refs,
            "cmakeMathSourceCountBasis": f"Configured File API {state.configured['configuration']} folia_math target sources; generated font source membership cross-checked against dependency manifest." if authoritative else f"Incomplete lexical candidate only ({state.configured['status']}); no configured-target or unresolved-import conclusion.",
            "fullResolvedCMakeMathSources": [state.ids[path] for path in sorted(state.existing_math_refs)] + [unit["path"] for unit in state.generated_fonts if unit["exists"]],
            "resolvedSourceVariableInputs": [{**item, "generatedCpp": resolved_units} for item in state.resolved_variables],
            "unresolvedSourceVariableExpressions": [] if authoritative else None,
            "lexicalCandidateUnresolvedSourceVariableExpressions": state.unresolved_variables, "missingGeneratedFoliaMathSources": state.missing_generated,
            "lexicalCandidateMathSourceIdsByTarget": {target: [state.ids[path] for path in sorted(paths) if path in state.ids] for target, paths in state.target_sources.items()},
            "configuredCodemodel": configured_codemodel_summary(state.configured),
            "lexicalCandidatePathsOutsideMathInventoryByTarget": {target: [path for path in paths if path not in state.ids] for target, paths in state.target_sources.items() if any(path not in state.ids for path in paths)},
            "generatedSources": state.generated_fonts, "priorFontSourceLockCandidatesWhenInactive": [{key: unit[key] for key in ("id", "path", "sha256", "bytes", "lock")} for unit in state.font_candidates],
            "dynamicCMakeSourceExpressions": {"resolved": [{"target": item["target"], "expression": item["expression"], "manifest": item["origin"],
                                                           "manifestSha256": item["manifestSha256"], "cppCount": len(item["generatedCpp"])} for item in state.resolved_variables],
                                              "unresolved": state.unresolved_variables},
            "edgeTupleColumns": ["fromFileId", "writtenImport", "resolvedFileIdOrNull", "kind", "exported"],
            "activeModuleInterfaces": [file_id(path) for path in sorted(state.active_interfaces)],
            "activeInterfaceProviders": {name: [state.ids[path] for path in paths] for name, paths in state.providers.items()},
            "duplicateActiveInterfaceProviders": state.duplicate_providers,
            "candidateInterfaceProviders": {name: [state.ids[path] for path in paths] for name, paths in state.candidate_providers.items()},
            "implicitPrimaryInterfaceEdges": implicit, "unresolvedActiveImports": state.unresolved_imports if authoritative else [],
            "lexicalCandidateUnresolvedImports": state.unresolved_imports, "importResolutionAuthority": state.configured["status"],
            "cmakeModuleMapPath": state.module_map_path, "cmakeModuleMapEvidence": state.module_map, "interfaceMapCheck": state.interface_check,
            "moduleImportEdges": module_edges, "forwardDeclarationTupleColumns": ["fileId", "symbol", "importedModule", "importedFileId"],
            "forwardDeclarationRisks": state.forward_risks, "fileIdNote": "Each tuple references the id field in files; unresolved imports have null target ids."}


def reports(state, entrypoint):
    policy = json.loads(Path(__file__).with_name("policy.json").read_bytes())
    files = file_rows(state)
    candidates = [path for path in state.repo.files("tools/math-port") if re.search(
        r"(?:mutation-test|\b(?:native-identities|option-array|option-filter|property-symbol|sparse-(?:tree|node)|callable-receiver)|(?:prototype|descriptor|identity|context-property)).*\.(?:cjs|py)$", path, re.I)]
    targets = [target["name"] for target in state.configured["targets"]] if state.configured["authoritative"] else re.findall(r"add_(?:library|executable)\s*\(\s*([A-Za-z0-9_]+)", state.cmake_text)
    tests = {"currentCMakeMathTestSources": [{"target": target["name"], "path": source["path"]}
              for target in state.configured["targets"] for source in target["sources"] if source["path"].startswith("tests/math/") and source["path"].endswith(".cpp")] if state.configured["authoritative"] else None,
             "lexicalCandidateMathTestSources": state.test_refs, "javascriptContractReviewCandidates": candidates,
             "retirementPlan": policy["retirementPlan"], "observedCMakeTargets": unique(targets)}
    revision = None
    try:
        revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=state.root, check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        pass
    generator = {"path": str(Path(entrypoint).resolve().relative_to(state.root).as_posix()), "sha256": sha(Path(entrypoint).read_bytes()),
                 "revisionNote": "Revision identifies HEAD only; file hashes describe the observed working tree, which may contain uncommitted changes.",
                 "modules": [{"path": path.relative_to(state.root).as_posix(), "sha256": sha(path.read_bytes())}
                             for path in sorted(Path(__file__).parent.iterdir()) if path.suffix in (".py", ".json")]}
    unindexed = [row["path"] for row in files if row["path"] in state.source_files and row["sourceAnnotationIndexStatus"] == "no-exact-note-index-entry"]
    provenance = {"status": "note-index-only", "indexedSourceFiles": len(state.source_files) - len(unindexed), "totalSourceFiles": len(state.source_files),
                  "unindexedSourceFiles": unindexed, "limitation": "No completeness or algorithm-equivalence claim; this is not an acceptance gate."}
    report = {"schema": "folia.math.native-raii.inventory.v1", "generatedAt": utc(time.time() * 1000), "root": ".", "baseRevision": revision,
              "generator": generator, "scope": source_scope(state), "graph": graph_report(state), "tests": tests, "files": files,
              "limitations": [*policy["limitations"], f"The {len(candidates)} JavaScript-contract-named scripts are review candidates only; names do not establish their assertions or disposition."],
              "sourceProvenanceGate": provenance}
    summary_variables = [{**{key: item[key] for key in ("target", "variable", "expression", "origin", "sourceLock", "manifestSha256",
                                "sourceDependencyCount", "manifestGeneratedCppCount", "excludedNonCppOutputs")},
                          "generatedCppCount": len(item["generatedCpp"])} for item in state.resolved_variables]
    compact = {"schema": "folia.math.native-raii.decision-manifest.v1", "snapshotRevision": revision, "generator": generator,
               "scope": report["scope"], "limitations": [*report["limitations"], "The inventory reader did not configure or build; it reads existing File API and module-map replies."],
               "sourceProvenanceGate": {key: value for key, value in provenance.items() if key != "unindexedSourceFiles"},
               "cmakeModuleMapEvidence": state.module_map, "interfaceMapCheck": state.interface_check, "generatedSourceUnits": generated_rows(state),
               "configuredCodemodel": report["graph"]["configuredCodemodel"], "resolvedSourceVariableInputs": summary_variables,
               "unresolvedSourceVariableExpressions": report["graph"]["unresolvedSourceVariableExpressions"],
               "lexicalCandidateUnresolvedSourceVariableExpressions": state.unresolved_variables,
               "files": [{**{key: row[key] for key in ("path", "decisionStatus", "responsibilityGroup", "namedOwner", "configuredTargets", "directlyListedByFoliaMath", "sourceAnnotationIndexStatus")},
                          "sourceAnnotationIndex": [{key: item[key] for key in ("document", "line", "sourcePaths", "sourceSha256", "status")} for item in row["sourceAnnotations"]]} for row in files]}
    return report, compact
