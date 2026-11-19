#!/usr/bin/env python3
"""Check Mermaid's source organization without a compiler or platform SDK.

This checks file ownership and resolvable dependencies, not C++ semantics or a
fictional strictly layered import graph. Compile and run the native tests too.
"""

import argparse
from collections import defaultdict
from pathlib import Path
import re
import sys


SOURCE_SUFFIXES = {".ixx", ".cpp", ".inc"}
AREAS = {
    "api", "frontend", "config", "themes", "support", "text", "layout",
    "rendering", "diagrams",
}
MODULE_NAME = r"[A-Za-z_]\w*(?:[.:][A-Za-z_]\w*)*"
DECLARATION = re.compile(
    rf"^\s*(?P<export>export\s+)?module\s+(?P<name>{MODULE_NAME})\s*;", re.M)
IMPORT = re.compile(
    rf"^\s*(?:export\s+)?import\s+(?P<name>:?{MODULE_NAME})\s*;", re.M)
INCLUDE = re.compile(r'^\s*#\s*include\s*(?P<open>[<"])(?P<name>[^>"\n]+)[>"]', re.M)
HEADER_IMPORT = re.compile(r'^\s*(?:export\s+)?import\s*[<"]([^>"\n]+)[>"]\s*;', re.M)
# Preserve strings while removing comments; a second pass masks strings when
# checking declarations so example text inside raw literals is not parsed as C++.
TOKEN = re.compile(
    r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delimiter)"'
    r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    r'|//[^\n]*|/\*.*?\*/', re.S)


def mask(text, literals=False):
    def replace(match):
        token = match.group()
        if not literals and not token.startswith(("//", "/*", 'R"')):
            return token
        return re.sub(r"[^\n]", " ", token)
    return TOKEN.sub(replace, text)


def platform_header(name):
    name = name.replace("\\", "/").lower()
    leaf = name.rsplit("/", 1)[-1]
    return (
        name.startswith(("winrt/", "wrl/"))
        or leaf in {
            "windows.h", "windowsx.h", "winnt.h", "windef.h", "winbase.h",
            "unknwn.h", "objbase.h", "combaseapi.h", "roapi.h", "wrl.h",
            "dcomp.h", "shellapi.h", "shlwapi.h", "windowsapp.h",
        }
        or re.fullmatch(r"(?:d2d|dwrite|d3d|dxgi)[\w.]*\.h", leaf) is not None
    )


def check(repository):
    root = repository / "src/mermaid"
    errors = []
    interfaces = defaultdict(list)
    units = []
    sources = sorted(p for p in root.rglob("*") if p.suffix in SOURCE_SUFFIXES)

    def fail(path, message):
        errors.append(f"{path.relative_to(repository).as_posix()}: {message}")

    if not sources:
        fail(root, "no Mermaid sources found")

    for path in sources:
        relative = path.relative_to(root)
        if len(relative.parts) == 1:
            fail(path, "source belongs in a responsibility directory, not the Mermaid root")
        elif relative.parts[0] not in AREAS:
            fail(path, f"unrecognized source area {relative.parts[0]!r}")

        text = path.read_text(encoding="utf-8-sig")
        comments_removed = mask(text)
        for include in INCLUDE.finditer(comments_removed):
            name = include["name"]
            if platform_header(name):
                fail(path, f"platform header {name!r} belongs in src/platform")
            if include["open"] == '"':
                target = (path.parent / name).resolve()
                if not target.is_file():
                    fail(path, f"local include {name!r} does not resolve beside its owner")
        for include in HEADER_IMPORT.finditer(comments_removed):
            if platform_header(include[1]):
                fail(path, f"platform header import {include[1]!r} belongs in src/platform")

        if path.suffix == ".inc":
            continue
        code = mask(text, literals=True)
        declarations = list(DECLARATION.finditer(code))
        if len(declarations) != 1:
            fail(path, f"expected one named module declaration, found {len(declarations)}")
            continue
        declaration = declarations[0]
        name = declaration["name"]
        if name != "folia.mermaid" and not name.startswith("folia.mermaid."):
            fail(path, f"module {name!r} is outside the folia.mermaid namespace")
        if path.suffix == ".ixx":
            if not declaration["export"]:
                fail(path, "module interface must use export module")
            interfaces[name].append(path)
        elif declaration["export"]:
            fail(path, "put the exported module interface in an .ixx file")
        imports = [match["name"] for match in IMPORT.finditer(code)]
        if "std" not in imports:
            fail(path, "C++23 module must import std")
        units.append((path, name, imports))

    for name, paths in interfaces.items():
        if len(paths) > 1:
            fail(paths[0], f"module {name!r} has multiple interfaces: " +
                 ", ".join(p.relative_to(root).as_posix() for p in paths))
    for path, name, imports in units:
        if name not in interfaces:
            fail(path, f"module {name!r} has no interface")
        for imported in imports:
            if imported in {"std", "std.compat"}:
                continue
            if imported.startswith(":"):
                imported = name.split(":", 1)[0] + imported
            if imported not in interfaces:
                fail(path, f"import {imported!r} has no Mermaid module interface")

    # These files are checked-in outputs. Keep their generators pointed at the
    # owning directory, so a later --write cannot recreate the old flat layout.
    generated = {
        "config-oracle.mjs": ["config/generated/config_schema_data.inc"],
        "session-oracle.mjs": [
            "config/generated/config_defaults.inc",
            "config/generated/config_callbacks.inc",
        ],
        "theme-oracle.mjs": ["themes/generated/theme_color_names.inc"],
    }
    for script_name, destinations in generated.items():
        script = repository / "tools/mermaid-cpp" / script_name
        script_text = script.read_text(encoding="utf-8")
        for destination in destinations:
            output = root / destination
            if not output.is_file():
                fail(output, "missing checked-in generated data")
            if f"src/mermaid/{destination}" not in script_text:
                fail(script, f"generator must target src/mermaid/{destination}")
    presets = root / "themes/presets"
    if not any(presets.glob("theme_*.inc")):
        fail(presets, "no checked-in theme presets found")
    generator = repository / "tools/mermaid-cpp/generate-themes.mjs"
    if "src/mermaid/themes/presets/theme_${identifier}.inc" not in generator.read_text(encoding="utf-8"):
        fail(generator, "theme generator must target themes/presets")

    return errors, len(sources), len(interfaces)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[2],
                        help="Folia repository root (default: this script's repository)")
    args = parser.parse_args()
    try:
        errors, source_count, module_count = check(args.root.resolve())
    except (OSError, UnicodeError) as error:
        print(f"Mermaid structure check could not read its inputs: {error}", file=sys.stderr)
        return 1
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        print(f"Mermaid structure check failed: {len(errors)} issue(s)", file=sys.stderr)
        return 1
    print(f"Mermaid structure check passed: {source_count} sources, {module_count} module interfaces")
    return 0


if __name__ == "__main__":
    sys.exit(main())
