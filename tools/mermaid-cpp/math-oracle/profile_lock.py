"""Verify the committed MathJax source/profile lock without executing JavaScript."""

import hashlib
import json
from pathlib import Path


DIRECTORY = Path(__file__).resolve().parent
REPOSITORY = DIRECTORY.parents[2]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module_tree(directory):
    rows = []

    def walk(path, prefix=""):
        for item in sorted(path.iterdir(), key=lambda path: path.name):
            name = prefix + item.name
            if item.is_dir():
                walk(item, name + "/")
            elif item.suffix in (".js", ".cjs", ".mjs", ".json"):
                rows.append([name, file_hash(item)])

    walk(directory)
    encoded = json.dumps(rows, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return {"files": len(rows), "sha256": hashlib.sha256(encoded).hexdigest()}


def verify_lock():
    lock = json.loads((DIRECTORY / "source-lock.json").read_text(encoding="utf-8"))
    require(type(lock["schema"]) is int and lock["schema"] == 1, "Wrong MathJax lock schema")
    for file in lock["files"]:
        require(file_hash(REPOSITORY / file["path"]) == file["sha256"], "MathJax source lock mismatch: " + file["path"])
    for dependency in lock["mathjax"]["dependencies"]:
        package = REPOSITORY / "tools/mathjax-bundle/node_modules" / dependency["name"]
        metadata = json.loads((package / "package.json").read_text(encoding="utf-8"))
        require(metadata["version"] == dependency["version"], "Wrong MathJax dependency: " + dependency["name"])
        require(module_tree(package / dependency["modules"]) == dependency["tree"], "Changed MathJax modules: " + dependency["name"])
    return lock
