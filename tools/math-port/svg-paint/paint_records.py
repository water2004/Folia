"""Complete, ordered corpus input and direct-MathML capture provenance."""

import json
import re
from pathlib import Path

from comparison.corpus import digest
from paint_dom import require


FAMILIES = {
    "table": {"count": 8, "fixture": "tests/math/fixtures/scenes/table/observations.tsv"},
    "table-profile": {"count": 14, "fixture": "tests/math/fixtures/scenes/table-profile/observations.tsv"},
}


def file_hash(path):
    return digest(Path(path).read_bytes())


def load_inputs(corpus_path, reference_path, native_path):
    corpus, reference, native = (json.loads(Path(path).read_bytes()) for path in (corpus_path, reference_path, native_path))
    require(all(type(report.get("schema")) is int and report["schema"] == 1 for report in (corpus, reference, native)), "Wrong paint corpus/report schema")
    require(bool(corpus["cases"]) and bool(corpus["modes"]), "Empty paint corpus")
    require(all(mode in ("inline", "display", "fragment") for mode in corpus["modes"]), "Unknown paint render mode")
    require(all(re.fullmatch(r"[A-Za-z0-9_-]+", item["name"]) for item in corpus["cases"]), "Invalid paint case name")
    names = [item["name"] + "--" + mode for item in corpus["cases"] for mode in corpus["modes"]]
    require(len(set(names)) == len(names), "Duplicate paint corpus records")
    corpus_hash = file_hash(corpus_path)
    for label, report in (("Official", reference), ("Native", native)):
        require(report["corpusSha256"] == corpus_hash, label + " paint corpus changed")
        require([row["name"] for row in report["records"]] == names, label + " paint coverage/order mismatch")
    inputs = {"corpusSha256": corpus_hash, "referenceSha256": file_hash(reference_path), "nativeSha256": file_hash(native_path)}
    return corpus, reference, native, inputs


def verify_source_lock(reference, root):
    lock = json.loads((root / "tools/mermaid-cpp/math-oracle/source-lock.json").read_bytes())
    require(reference["sourceLock"] == lock, "Paint reference source/profile lock changed")
    for item in lock["files"]:
        require(file_hash(root / item["path"]) == item["sha256"], "MathJax source lock mismatch: " + item["path"])
    # The source capture verifies the complete package module trees. Their
    # immutable lock is bound to this saved report; replay needs no Node install.
    return lock


def load_mathml(directory, binary, root):
    if directory is None:
        require(binary is None, "--mathml-binary requires --mathml")
        return None, []
    require(binary is not None, "--mathml requires --mathml-binary for capture provenance")
    binary = Path(binary).resolve()
    metadata = {"binary": {"path": str(binary), "sha256": file_hash(binary)}, "families": {},
                "input": "Existing compiled direct MathML Source fixture nodes through Common LayoutJob / SVG writer",
                "reference": "Unmodified committed MathJax 4.1.3 / NewCM SVG fixture bytes; no TeX input claim"}
    records, names = [], set()
    for family, info in FAMILIES.items():
        location = Path(directory) / family
        files = sorted(location.glob("*.json"))
        require(len(files) == info["count"], family + " capture is incomplete or contains stale cases")
        metadata["families"][family] = {**info, "fixtureSha256": file_hash(root / info["fixture"])}
        for path in files:
            item = json.loads(path.read_bytes())
            require(type(item.get("schema")) is int and item["schema"] == 1 and item["family"] == family, "Wrong direct MathML capture schema/family")
            require(re.fullmatch(r"[A-Za-z0-9_-]+", item["name"]) is not None, "Invalid direct MathML case name")
            require(item["name"] not in names and path.name == item["name"] + ".json", "Duplicate or mismatched direct MathML capture")
            names.add(item["name"])
            source_file, native_file = (location / (item["name"] + suffix) for suffix in (".source.svg", ".native.svg"))
            records.append({"item": item, "family": family, "source": source_file.read_text(encoding="utf-8"),
                            "native": native_file.read_text(encoding="utf-8"),
                            "capture": {"sourceSha256": file_hash(source_file), "nativeSha256": file_hash(native_file),
                                        "manifestSha256": file_hash(path)}})
    return metadata, records
