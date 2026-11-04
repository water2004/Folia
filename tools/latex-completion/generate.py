"""Build the shipped catalog from the locked TeX registry and editor templates.

The app reads only the resulting JSON. No MathJax runtime is used by completion.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUTPUT = ROOT / "src/app-winui/Assets/latex/commands.json"


def load(name):
    return json.loads((HERE / name).read_text(encoding="utf-8"))


def registry():
    data = load("registry.json")
    source = (ROOT / "tests/math/fixtures/tex_package_registry_source.hpp").read_text(encoding="utf-8")
    digest = hashlib.sha256(source.replace("\r\n", "\n").encode()).hexdigest()
    if digest != data["registrySha256"]:
        raise ValueError("TeX registry changed: regenerate its metadata with tex-package-conformance.cjs --registry-only")
    return data["entries"]


def command_name(entry):
    name = entry["name"]
    return name.removeprefix("\\") if entry["kind"] == "delimiter" else name


def eligible(name):
    # A control-space or row separator is entered directly, not as a word.
    return bool(name) and not any(c.isspace() or c == "\\" for c in name)


def argument_suffix(count):
    return "".join("{$" + str(i) + "}" for i in range(1, count + 1)) + "$0"


def macro_template(entry, profiles):
    name, method, args = command_name(entry), entry["method"], entry["args"]
    name = name.replace("$", "$$")
    if entry["kind"] == "delimiter" or entry["symbol"]:
        return "\\" + name + "$0", "symbol"
    if method == "Macro":
        count = int(args[1]) if len(args) > 1 and isinstance(args[1], (int, float)) else 0
        optional = len(args) > 2 and args[2] is not None
        suffix = argument_suffix(count)
        if optional and count:
            default = str(args[2]).replace("$", "$$")
            suffix = "[${1:" + default + "}]" + "".join("{$" + str(i) + "}" for i in range(2, count + 1)) + "$0"
        return "\\" + name + suffix, "structure" if count else "symbol"
    spec = profiles[method]  # Unknown methods must acquire an explicit recipe.
    if isinstance(spec, int) or "arguments" in spec:
        count = spec if isinstance(spec, int) else spec["arguments"]
        if method == "Genfrac" and not args:
            count = 6
        if method == "Qqtext" and args:
            count = 0
        category = spec.get("category") if isinstance(spec, dict) else None
        return "\\" + name + argument_suffix(count), category or ("structure" if count else "symbol")
    return spec["template"].replace("COMMAND", name), spec.get("category", "structure")


def build_catalog():
    curated = load("templates.json")["commands"]
    profiles = load("methods.json")
    environments = load("environments.json")
    commands, used = [], set()
    for item in curated:
        if item["trigger"] in used:
            raise ValueError("Duplicate editor template: " + item["trigger"])
        commands.append(item.copy())
        used.add(item["trigger"])
    def append(trigger, snippet, category, identifier, description=None):
        if trigger in used:
            return
        used.add(trigger)
        commands.append(dict(id=identifier, trigger=trigger, snippet=snippet,
            description=description or "LatexDescription" + {"symbol": "Symbol", "structure": "Structure",
                "operator": "Operator", "accent": "Accent", "environment": "Environment"}[category] + "Generic",
            category=category))
    entries = registry()
    missing = set()
    for entry in entries:
        name = command_name(entry)
        if entry["kind"] == "environment" or not eligible(name) or name in used:
            continue
        try:
            snippet, category = macro_template(entry, profiles)
        except KeyError:
            missing.add(entry["method"])
            continue
        append(name, snippet, category, "mathjax.command." + name)
    if missing:
        raise ValueError("Missing parser method templates: " + ", ".join(sorted(missing)))
    for name in sorted({e["name"] for e in entries if e["kind"] == "environment"}):
        spec = environments.get(name, {})
        suffix = spec.get("arguments", "")
        body = spec.get("body", "$1")
        snippet = "\\begin{" + name + "}" + suffix + "\n" + body + "\n\\end{" + name + "}$0"
        ids = spec.get("ids", {})
        append(name, snippet, "environment", ids.get("short", "mathjax.environment." + name), spec.get("description"))
        append("begin{" + name + "}", snippet, "environment", ids.get("begin", "mathjax.begin." + name), spec.get("description"))
        append("end{" + name + "}", "\\end{" + name + "}$0", "environment", "mathjax.end." + name)
    if len({item["id"] for item in commands}) != len(commands):
        raise ValueError("Duplicate completion ids")
    return dict(schemaVersion=1, commands=commands)


def render(catalog):
    # One record per line keeps generated data reviewable without a huge diff.
    return '{\n  "schemaVersion": 1,\n  "commands": [\n' + ",\n".join(
        "    " + json.dumps(item, ensure_ascii=False) for item in catalog["commands"]) + '\n  ]\n}\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    catalog = build_catalog()
    result = render(catalog)
    if args.check:
        if OUTPUT.read_text(encoding="utf-8") != result:
            raise SystemExit("Shipped LaTeX catalog is stale: run tools/latex-completion/generate.py")
    else:
        OUTPUT.write_text(result, encoding="utf-8", newline="\n")
    print(f"LaTeX catalog: {len(catalog['commands'])} commands/templates")


if __name__ == "__main__":
    main()
