"""Literal CMake candidates; these do not substitute for a configured File API."""

import posixpath
import re

from .files import unique


SOURCE_PATTERN = r"(?:src/math|tests/math)/[A-Za-z0-9_./-]+\.(?:ixx|cpp|hpp|h|c|cc)"


def cmake_closure(repo):
    visited, todo = set(), ["CMakeLists.txt"]
    while todo:
        file = todo.pop()
        if file in visited or not repo.exists(file):
            continue
        visited.add(file)
        body, base = repo.read(file), posixpath.dirname(file)
        for match in re.finditer(r'^\s*add_subdirectory\s*\(\s*([^\s)]+)(?:\s+[^)]*)?\)', body, re.M):
            directory = posixpath.normpath(posixpath.join(base, match[1].strip('"')))
            if not re.search(r"[${}]", directory):
                todo.append(posixpath.join(directory, "CMakeLists.txt"))
        for match in re.finditer(r'^\s*include\s*\(\s*("[^"\n]+"|[^\s)]+)', body, re.M):
            path = match[1].strip('"')
            if path.startswith("${FOLIA_SOURCE_ROOT}/"):
                path = path[len("${FOLIA_SOURCE_ROOT}/"):]
            elif path.startswith("${CMAKE_CURRENT_LIST_DIR}/"):
                path = posixpath.join(base, path[len("${CMAKE_CURRENT_LIST_DIR}/"):])
            else:
                path = posixpath.join(base, path)
            # CMake module-name lookup and arbitrary variables remain unresolved.
            if path.endswith(".cmake") and not re.search(r"[${}]", path):
                todo.append(posixpath.normpath(path))
    return sorted(visited)


def target_bodies(text, target_name=None):
    result = []
    for match in re.finditer(r"(?:target_sources|add_library|add_executable)\s*\(\s*([A-Za-z0-9_]+)", text):
        if target_name is not None and match[1] != target_name:
            continue
        rest, depth, end = text[match.start():], 0, 0
        for end, char in enumerate(rest):
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    break
        result.append((match[1], rest[:end + 1]))
    return result


def target_sources(text):
    result = {}
    for target, body in target_bodies(text):
        result[target] = unique([*result.get(target, []), *re.findall(SOURCE_PATTERN, body)])
    return result


def active_interfaces(text):
    result = []
    for _, body in target_bodies(text, "folia_math"):
        if re.search(r"FILE_SET\s+CXX_MODULES\b[^)]*\bFILES\b", body):
            result.extend(re.findall(r"src/math/[A-Za-z0-9_./-]+\.ixx", body))
    return unique(result)


def resolve_target_source_variables(bodies, target_name, manifests):
    variables = unique(name for body in bodies for name in re.findall(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", body))
    resolved, unresolved = [], []
    for variable in variables:
        if variable in ("CMAKE_CURRENT_SOURCE_DIR", "CMAKE_CURRENT_BINARY_DIR", "CMAKE_CURRENT_LIST_DIR", "FOLIA_SOURCE_ROOT"):
            continue
        item = {"target": target_name, "variable": variable, "expression": "${" + variable + "}"}
        manifest = manifests.get("fontDependencies")
        if target_name == "folia_math" and variable == "FOLIA_FONT_GENERATED_CPP" and manifest is not None:
            if not isinstance(manifest.get("generated_cpp"), list):
                unresolved.append({**item, "reason": "known-font-dependency-manifest-has-no-generated_cpp-array"})
                continue
            resolved.append({**item, "origin": manifests["fontDependenciesPath"], "sourceLock": manifest.get("source_lock"),
                             "manifestSha256": manifests.get("fontDependenciesSha256"),
                             "sourceDependencyCount": len(manifest.get("source_dependencies", [])),
                             "manifestGeneratedCppCount": len(manifest["generated_cpp"]),
                             "excludedNonCppOutputs": [path for path in manifest["generated_cpp"] if not path.endswith(".cpp")],
                             "generatedCpp": [path for path in manifest["generated_cpp"] if path.endswith(".cpp")]})
        else:
            unresolved.append({**item, "reason": "source-list-variable-not-resolved-by-this-static-inventory"})
    return {"resolved": resolved, "unresolved": unresolved}
