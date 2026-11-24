"""Validate existing CMake File API evidence without configuring or building."""

import json
import re
from pathlib import Path

from .files import relative, same_path, sha, unique, utc


def rejected(status, reason=None, **details):
    return {"status": status, "authoritative": False, **({"reason": reason} if reason is not None else {}), **details}


def configured_codemodel_summary(data):
    result = {"status": data["status"], "authoritative": data["authoritative"], "reason": data.get("reason")}
    for key in ("indexFile", "codemodelFile", "generatedModuleMapPath", "expectedSourceRoot", "reportedSourceRoot",
                "expectedBuildRoot", "reportedBuildRoot", "actualReplyDir", "expectedReplyDir", "badPaths", "missingInputs",
                "newerInputs", "missingFontInputs", "extraFontInputs", "moduleMapCheck", "evidence", "freshness"):
        if key in data:
            result[key] = data[key]
    if data["authoritative"]:
        for key in ("sourceRoot", "buildRoot", "configuration", "foliaMathSourceCount", "foliaMathInterfaceCount",
                    "bridgeSourceMember", "bridgeTargets", "allTargetMathMembership", "foliaMathSources"):
            result[key] = data[key]
        result["targets"] = [{"name": target["name"], "id": target["id"], "type": target.get("type"),
                              "sourcePaths": [source["path"] for source in target["sources"]]} for target in data["targets"]]
    return result


def codemodel_envelope_status(*, root, index=None, indexError=None, codemodel=None, codemodelError=None, build_dir=None):
    if index is None and not indexError:
        return rejected("missing", "no-file-api-index")
    if indexError or codemodelError:
        return rejected("malformed", str(indexError or codemodelError))
    if not isinstance(index, dict) or not index.get("reply", {}).get("client-math-port", {}).get("codemodel-v2"):
        return rejected("malformed", "missing-client-math-port-codemodel-v2")
    paths = codemodel.get("paths", {}) if isinstance(codemodel, dict) else {}
    if (not isinstance(codemodel, dict) or not isinstance(codemodel.get("configurations"), list)
            or any(not isinstance(paths.get(key), str) or not paths[key].strip() for key in ("source", "build"))):
        return rejected("malformed", "invalid-codemodel-shape")
    actual, expected = (Path(root) / paths["source"]).resolve(), Path(root).resolve()
    if not same_path(actual, expected):
        return rejected("wrong-source-root", expectedSourceRoot=str(expected), reportedSourceRoot=str(actual))
    actual_build = (Path(root) / paths["build"]).resolve()
    expected_build = Path(build_dir).resolve() if build_dir else (Path(root) / "build/core").resolve()
    if not same_path(actual_build, expected_build):
        return rejected("incomplete", "codemodel-build-root-does-not-match-local-core-module-map",
                        expectedBuildRoot=str(expected_build), reportedBuildRoot=str(actual_build))
    return rejected("candidate", sourceRoot=str(actual))


def evaluate_configured_target_data(repo_root, *, indexFile, codemodelFile, codemodel, configuration, targetFacts,
                                    moduleMap, generatedFontCpp=(), indexTime=0, freshnessInputs=(), sourceText=None):
    identity = {"indexFile": indexFile, "codemodelFile": codemodelFile}
    math_target = next((target for target in targetFacts if target.get("name") == "folia_math"), None)
    if math_target is None:
        return rejected("incomplete", "folia_math-target-not-present", **identity)
    if not all(isinstance(target.get("sources"), list) for target in targetFacts):
        return rejected("malformed", "target-sources-not-array", **identity)
    if not isinstance(moduleMap, dict) or not isinstance(moduleMap.get("modules"), dict):
        return rejected("malformed", "generated-module-map-has-no-modules-object", **identity)
    bad_paths = [target["name"] + ":" + source["path"] for target in targetFacts for source in target["sources"] if not source["exists"]]
    if bad_paths:
        return rejected("stale-inputs", "configured-target-source-paths-missing", **identity, badPaths=bad_paths)
    newer = [item["path"] for item in freshnessInputs if item["mtimeMs"] > indexTime]
    expected = unique(Path(path).as_posix() for path in generatedFontCpp)
    configured = {source["path"] for source in math_target["sources"]}
    missing = [path for path in expected if path not in configured]
    extra = [source["path"] for source in math_target["sources"] if source["path"].startswith("build/math-port/font-generated-native-raii/")
             and source["path"].endswith(".cpp") and source["path"] not in expected]
    if newer or missing or extra:
        return rejected("stale-inputs", "configured-codemodel-predates-source-list-inputs-or-font-manifest", **identity,
                        newerInputs=newer, missingFontInputs=missing, extraFontInputs=extra)
    sourceText = sourceText or (lambda path: (Path(repo_root) / path).read_bytes().decode("utf-8"))
    rows, providers = [], {}
    for source in math_target["sources"]:
        if not source["path"].endswith(".ixx"):
            continue
        match = re.search(r"^\s*export\s+module\s+([^;]+);", sourceText(source["path"]), re.M)
        name = match[1].strip() if match else None
        rows.append({"path": source["path"], "module": name})
        if name:
            providers.setdefault(name, []).append(source["path"])
    map_names, provider_names = sorted(moduleMap["modules"]), sorted(providers)
    duplicates = [{"module": name, "paths": paths} for name, paths in providers.items() if len(paths) > 1]
    check = {"moduleMapCount": len(map_names), "providerCount": len(provider_names), "interfaceSourceCount": len(rows),
             "undeclaredInterfaceCount": sum(not row["module"] for row in rows),
             "exactMatch": map_names == provider_names and all(row["module"] for row in rows) and not duplicates,
             "missingFromMap": [name for name in provider_names if name not in map_names],
             "missingProviders": [name for name in map_names if name not in providers], "duplicateProviders": duplicates}
    if not check["exactMatch"]:
        return rejected("incomplete", "configured-interface-declarations-do-not-match-generated-module-map", **identity, moduleMapCheck=check)
    all_math = [{"name": target["name"], "id": target["id"],
                 "paths": [source["path"] for source in target["sources"] if source["path"].startswith("src/math/")]} for target in targetFacts]
    all_math = [target for target in all_math if target["paths"]]
    bridges = [target["name"] for target in all_math if "src/math/native_renderer_bridge.cpp" in target["paths"]]
    return {"status": "ready", "authoritative": True, "sourceRoot": str(Path(codemodel["paths"]["source"]).resolve()),
            "buildRoot": str((Path(repo_root) / codemodel["paths"]["build"]).resolve()), "configuration": configuration, **identity,
            "targets": targetFacts, "foliaMathSourceCount": len(math_target["sources"]), "foliaMathSources": math_target["sources"],
            "foliaMathInterfaceCount": len(rows), "interfaceProviders": rows, "moduleMapNames": map_names, "moduleMapCheck": check,
            "allTargetMathMembership": all_math, "bridgeTargets": bridges, "bridgeSourceMember": bool(bridges),
            "freshness": {"status": "mtime-current-at-read", "indexMtimeUtc": utc(indexTime),
                          "checkedCMakeInputs": [item["path"] for item in freshnessInputs], "newerInputs": [],
                          "generatedFontCppCount": len(expected), "missingFontInputs": [], "extraFontInputs": [],
                          "limitation": "Codemodel records the last configure result; it does not attest unchanged non-CMake glob contents or source file content hashes."}}


def read_configured_codemodel(repo_root, *, generatedFontCpp=(), cmakeFiles=(), replyDir=None, build_dir=None, configuration_name="Debug"):
    root = Path(repo_root).resolve()
    build = Path(build_dir).resolve() if build_dir else root / "build/core"
    reply = Path(replyDir) if replyDir else build / ".cmake/api/v1/reply"
    try:
        indexes = sorted((path for path in reply.iterdir() if re.fullmatch(r"index-.*\.json", path.name)),
                         key=lambda path: path.stat().st_mtime_ns, reverse=True)
    except OSError as error:
        return rejected("missing", str(error))
    if not indexes:
        return rejected("missing", "no-file-api-index")
    ref, index_path, index = None, None, None
    for candidate in indexes:
        try:
            index = json.loads(candidate.read_bytes())
            ref = index.get("reply", {}).get("client-math-port", {}).get("codemodel-v2")
        except (OSError, ValueError, AttributeError) as error:
            return rejected("malformed", f"index-invalid:{error}", indexFile=candidate.name)
        if isinstance(ref, dict) and ref.get("jsonFile"):
            index_path = candidate
            break
    if index_path is None:
        return rejected("missing", "no-index-contains-client-math-port-codemodel-v2")
    identity = {"indexFile": index_path.name, "codemodelFile": ref["jsonFile"]}
    model_path = reply / ref["jsonFile"]
    try:
        model = json.loads(model_path.read_bytes())
    except (OSError, ValueError) as error:
        return rejected("malformed", f"codemodel-invalid:{error}", **identity)
    envelope = codemodel_envelope_status(root=root, index=index, codemodel=model, build_dir=build)
    if envelope["status"] != "candidate":
        return {**envelope, **identity}
    configuration = next((item for item in model["configurations"] if item.get("name") == configuration_name), None)
    if configuration is None:
        return rejected("incomplete", configuration_name + "-configuration-not-present", **identity)
    if not isinstance(configuration.get("targets"), list):
        return rejected("malformed", "configuration-targets-not-array", **identity)
    targets = []
    try:
        for reference in configuration["targets"]:
            if not isinstance(reference, dict) or not isinstance(reference.get("jsonFile"), str) or not reference["jsonFile"].strip():
                return rejected("malformed", "invalid-target-reference", **identity)
            target_path = reply / reference["jsonFile"]
            target = json.loads(target_path.read_bytes())
            if not isinstance(target, dict) or not isinstance(target.get("sources"), list):
                return rejected("malformed", f"target-{reference.get('name')}-sources-not-array", **identity)
            directory = (root / target.get("paths", {}).get("source", ".")).resolve()
            if not directory.is_relative_to(root):
                return rejected("wrong-source-root", f"target-{target['name']}-source-root-outside-codemodel-root", **identity)
            sources = []
            for source in target["sources"]:
                absolute = (root / source["path"]).resolve()
                sources.append({"path": relative(root, absolute), "exists": absolute.exists(),
                                "isGenerated": bool(source.get("isGenerated")), "compileGroupIndex": source.get("compileGroupIndex")})
            targets.append({"name": target["name"], "id": target["id"], "type": target.get("type"),
                            "reply": reference["jsonFile"], "sha256": sha(target_path.read_bytes()),
                            "mtimeUtc": utc(target_path.stat().st_mtime * 1000), "sources": sources})
    except (OSError, ValueError, KeyError, TypeError) as error:
        return rejected("malformed", f"target-reply-invalid:{error}", **identity)
    if not isinstance(configuration.get("directories"), list) or not all(isinstance(item.get("source"), str) for item in configuration["directories"]):
        return rejected("malformed", "configuration-directories-invalid", **identity)
    freshness_paths = unique([*(root / item["source"] / "CMakeLists.txt" for item in configuration["directories"]),
                              *(root / path for path in cmakeFiles), root / "build/math-port/font-generated-native-raii/font-data-source-dependencies.json"])
    def evidence(path):
        return {"path": relative(root, path), "sha256": sha(path.read_bytes()), "mtimeUtc": utc(path.stat().st_mtime * 1000)}
    partial = {"index": evidence(index_path), "codemodel": evidence(model_path),
               "targets": [{"name": target["name"], "path": relative(root, reply / target["reply"]),
                            "sha256": target["sha256"], "mtimeUtc": target["mtimeUtc"]} for target in targets]}
    missing = [relative(root, path) for path in freshness_paths if not path.exists()]
    if missing:
        return rejected("incomplete", "codemodel-configure-inputs-missing", **identity, missingInputs=missing, evidence=partial)
    freshness = [{"path": relative(root, path), "mtimeMs": path.stat().st_mtime * 1000} for path in freshness_paths]
    build_root = (root / model["paths"]["build"]).resolve()
    expected_reply = build_root / ".cmake/api/v1/reply"
    if not same_path(reply, expected_reply):
        return rejected("incomplete", "file-api-reply-directory-does-not-match-codemodel-build-root", **identity,
                        expectedReplyDir=str(expected_reply), actualReplyDir=str(reply.resolve()))
    module_map = build_root / "CMakeFiles/folia_math.dir/CXXModules.json"
    try:
        mapping = json.loads(module_map.read_bytes())
    except (OSError, ValueError) as error:
        return rejected("incomplete", f"generated-module-map-invalid-or-missing:{error}", **identity)
    observed = {**partial, "generatedModuleMap": evidence(module_map)}
    result = evaluate_configured_target_data(root, **identity, codemodel=model, configuration=configuration["name"],
                                             targetFacts=targets, moduleMap=mapping, generatedFontCpp=generatedFontCpp,
                                             indexTime=index_path.stat().st_mtime * 1000, freshnessInputs=freshness)
    return {**result, "evidence": observed, "generatedModuleMapPath": relative(root, module_map),
            "freshness": result.get("freshness", {"status": result["status"], "newerInputs": result.get("newerInputs", []),
                                                    "limitation": "A rejected snapshot is not authoritative."})}
