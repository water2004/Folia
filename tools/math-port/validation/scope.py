"""Inventory the locked upstream import closure without executing JavaScript."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

EXPECTED_COMMIT = "fb987178c2d279a99b0db5ea02e751691703955e"
FONT_PACKAGES = ("mathjax-newcm-font", "mathjax-bbm-font-extension",
                 "mathjax-bboldx-font-extension", "mathjax-dsfont-font-extension")
IMPORTS = re.compile(r"^\s*import\s+(?:[^;'\"`]*?\bfrom\s*)?['\"]([^'\"]+)['\"]", re.M)
EXPORTS = re.compile(r"^\s*export\s+(?:type\s+)?(?:\{[^;]*?\}|\*)\s+from\s*['\"]([^'\"]+)['\"]", re.M)


def digest(value: bytes | str) -> str:
    return hashlib.sha256(value.encode("utf-8") if isinstance(value, str) else value).hexdigest()


def snake(value: str) -> str:
    if value == "BBox":
        return "bbox"
    return re.sub(r"([A-Z])([A-Z][a-z])", r"\1_\2",
                  re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value)).lower()


def owner(source: str) -> str:
    if source.startswith("ts/input/"):
        return "tex"
    if source == "ts/output/common/FontData.ts" or "/output/svg/Font" in source:
        return "fonts"
    if source.startswith("ts/output/common"):
        return "layout"
    if source.startswith("ts/output/svg"):
        return "scene_svg"
    if source.startswith(("ts/core/MmlTree", "ts/core/Tree", "ts/util/")):
        return "semantics_util"
    return "root_host_adapter"


def manifest(root: Path, source_root: Path, prior: dict | None) -> dict:
    """Preserve reviewed ownership only when the exact upstream bytes match."""
    source_root = source_root.resolve()
    profile = (root / "tools/mathjax-bundle/entry.cjs").read_bytes().decode("utf-8")
    package_array = re.search(r"packages:\s*\[([\s\S]*?)\]", profile)
    if not package_array:
        raise ValueError("No Folia TeX package profile")
    packages = re.findall(r"'([^']+)'", package_array[1])
    package_roots = ["ts/" + name + ".ts" for name in re.findall(
        r"require\('@mathjax/src/js/(input/tex/[^']+)\.js'\)", profile)]
    seeds = ["ts/input/tex.ts", "ts/output/common.ts", "ts/output/svg.ts",
             "ts/util/BBox.ts", "ts/core/MmlTree/MmlFactory.ts", *package_roots]
    pending, files, unresolved = list(seeds), {}, []
    while pending:
        relative = pending.pop()
        if relative in files:
            continue
        absolute = source_root / relative
        if not absolute.is_file():
            raise ValueError("Missing source: " + relative)
        data = absolute.read_bytes()
        text = data.decode("utf-8", errors="replace")
        dependencies, external = [], []
        for match in [*IMPORTS.finditer(text), *EXPORTS.finditer(text)]:
            specifier = match[1]
            if not specifier.startswith("."):
                external.append(specifier)
                continue
            target = Path(os.path.relpath(
                (absolute.parent / re.sub(r"\.js$", ".ts", specifier)).resolve(), source_root)).as_posix()
            if not (source_root / target).exists():
                unresolved.append({"source": relative, "specifier": specifier, "candidate": target})
                continue
            dependencies.append(target)
            pending.append(target)
        parts = [snake(part) for part in re.sub(r"\.ts$", "", relative.removeprefix("ts/")).split("/")]
        files[relative] = {
            "source": relative, "source_sha256": digest(data),
            "proposed_target": "src/math/" + "/".join(parts) + ".ixx",
            "proposed_module": "folia.math." + ".".join(parts),
            "owner": owner(relative), "disposition": "unreviewed", "adaptation_reason": "",
            "port_status": "not_started", "acceptance": "not_verified",
            "dependencies": sorted(set(dependencies)), "external_imports": sorted(set(external))}
    lock_bytes = (root / "tools/mathjax-bundle/package-lock.json").read_bytes()
    lock = json.loads(lock_bytes)
    fonts = []
    for name in FONT_PACKAGES:
        entry = lock["packages"].get("node_modules/@mathjax/" + name)
        if not entry or entry.get("version") != "4.1.3":
            raise ValueError("Unpinned font " + name)
        fonts.append({"package": "@mathjax/" + name, "version": entry["version"],
                      "integrity": entry["integrity"], "disposition": "generate_data_and_port_resolver",
                      "acceptance": "not_verified"})
    previous_files = {item["source"]: item for item in prior["files"]} if prior else {}
    for item in files.values():
        previous = previous_files.get(item["source"])
        if previous and previous["source_sha256"] == item["source_sha256"]:
            for key in ("disposition", "adaptation_reason", "port_status", "acceptance"):
                item[key] = previous[key]
            if previous["port_status"] == "implemented":
                for key in ("proposed_target", "proposed_module", "owner"):
                    item[key] = previous[key]
                if not (root / item["proposed_target"]).exists():
                    raise ValueError("Accepted native target is missing: " + item["proposed_target"])
    reviewed = sum(item["disposition"] != "unreviewed" for item in files.values())
    status = ("scope_reviewed_not_release_ready" if reviewed == len(files) else
              "scope_partially_reviewed_not_release_ready" if reviewed else
              "inventory_unreviewed_not_release_ready")
    return {"schema": 1, "status": status,
            "upstream": {"repository": "https://github.com/mathjax/MathJax-src",
                         "version": "4.1.3", "commit": EXPECTED_COMMIT},
            "profile": {"file": "tools/mathjax-bundle/entry.cjs", "sha256": digest(profile),
                        "tex_packages": packages, "svg_options": {"fontCache": "none", "exFactor": 0.5,
                        "displayOverflow": "linebreak", "linebreaks": {"inline": True}}},
            "dependency_lock_sha256": digest(lock_bytes), "font_packages": fonts,
            "source_roots": sorted(set(seeds)), "unresolved_imports": unresolved,
            "files": [files[name] for name in sorted(files)]}


def generate(root: Path, check: bool = False) -> dict:
    source_root = root / ".reference/MathJax-src"
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source_root, text=True).strip()
    if commit != EXPECTED_COMMIT:
        raise ValueError("Unexpected MathJax source commit: " + commit)
    destination = root / "tools/math-port/profiles/scope.json"
    prior = json.loads(destination.read_bytes()) if destination.exists() else None
    document = manifest(root, source_root, prior)
    if check:
        # Keep the historical ordered-object check, as well as all value checks.
        if not prior or json.dumps(prior, ensure_ascii=False) != json.dumps(document, ensure_ascii=False):
            raise ValueError("Scope inventory differs from pinned sources/profile")
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(document, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return document
