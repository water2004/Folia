"""Textual C++ module facts and conservative provider relationships."""

import re

from .files import unique


def is_source_file(path):
    return re.search(r"\.(ixx|cpp|hpp|h|cc|c|inc)$", path) is not None


def unresolved_refs(refs, known):
    return sorted(path for path in refs if path not in known)


def module_scan_candidates(paths, exists):
    paths = unique(paths)
    return {"paths": [path for path in paths if exists(path)], "missing": [path for path in paths if not exists(path)]}


def module_facts(path, source_text):
    if not re.search(r"\.(ixx|cpp)$", path):
        return {"declaration": None, "isInterface": False, "isImplementationUnit": False, "primaryModule": None,
                "partition": None, "imports": [], "forwardDeclarations": []}
    body = source_text(path)
    imports = []
    for match in re.finditer(r"^\s*(export\s+)?import\s+([^;]+);", body, re.M):
        name = match[2].strip()
        imports.append({"module": name, "exported": bool(match[1]),
                        "kind": "partition" if name.startswith(":") else "standard" if name.startswith("std") else "named-module"})
    interface = re.search(r"^\s*export\s+module\s+([^;]+);", body, re.M)
    declaration = interface or re.search(r"^\s*module\s+([^;]+);", body, re.M)
    name = declaration[1].strip() if declaration else None
    return {"declaration": name, "isInterface": bool(interface), "isImplementationUnit": bool(name and not interface),
            "primaryModule": name.split(":", 1)[0] if name else None,
            "partition": name.split(":", 1)[1] if name and ":" in name else None, "imports": imports,
            "forwardDeclarations": re.findall(r"^\s*(?:export\s+)?(?:class|struct|union)\s+([A-Za-z_][A-Za-z0-9_]*)\s*;", body, re.M)}


def resolve_module_edges(path, facts, providers, active_sources):
    found, unresolved = [], []
    for item in facts["imports"]:
        if item["kind"] == "standard":
            continue
        name = (facts["primaryModule"] or "null") + item["module"] if item["kind"] == "partition" else item["module"]
        candidates = providers.get(name, [])
        if len(candidates) == 1:
            found.append({"module": item["module"], "resolvedModule": name, "file": candidates[0],
                          "exported": item["exported"], "kind": item["kind"]})
        elif path in active_sources:
            unresolved.append({"file": path, "module": item["module"], "resolvedModule": name,
                               "reason": "duplicate-active-interface-providers" if candidates else "no-registered-interface-provider"})
    if facts["isImplementationUnit"] and facts["primaryModule"]:
        name = facts["primaryModule"]
        candidates = providers.get(name, [])
        if len(candidates) == 1:
            found.append({"module": name, "resolvedModule": name, "file": candidates[0], "exported": False,
                          "kind": "implicit-primary-interface"})
        elif path in active_sources:
            unresolved.append({"file": path, "module": name, "resolvedModule": name,
                               "reason": "duplicate-active-primary-interface-providers" if candidates else "implementation-unit-missing-primary-interface"})
    return {"edges": found, "unresolved": unresolved}
