"""The checked-in full-diagram corpus and its immutable evidence identifiers."""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[4]
UPSTREAM = {"version": "12.2.0", "commit": "1fb7869866dd619168d840291c9c54b4f129758b"}
FAMILIES = (
    "agentflow architecture block c4 class cynefin er eventmodeling flowchart gantt "
    "gitgraph info ishikawa journey kanban mindmap packet pie quadrant radar railroad "
    "requirement sankey sequence state swimlane timeline tree-view treemap usecase "
    "venn wardley xy zenuml"
).split()


def require(condition: bool, message: str) -> None:
    """Keep validation active even when Python is invoked with -O."""
    if not condition:
        raise ValueError(message)


def digest(value: bytes | str) -> str:
    return hashlib.sha256(value.encode("utf-8") if isinstance(value, str) else value).hexdigest()


def load_corpus(directory: Path | None = None) -> tuple[list[dict], list[dict]]:
    directory = directory or REPOSITORY / "tests/mermaid_regression/cases"
    paths = sorted(directory.glob("*.json"))
    require([p.stem for p in paths] == FAMILIES, "Every supported family must have a corpus")
    ids, records, summary = set(), [], []
    for path in paths:
        data = path.read_bytes()
        document = json.loads(data)
        require(document["family"] == path.stem, f"{path}: wrong family")
        cases = document["cases"]
        require(len(cases) >= 8, f"{path}: at least eight regression cases required")
        complex_count, inputs = 0, set()
        for item in cases:
            ident = item["id"]
            require(re.fullmatch(r"[a-z0-9][a-z0-9-]*", ident) is not None, f"Invalid id {ident}")
            require(ident not in ids, f"Duplicate case id {ident}")
            ids.add(ident)
            require(item["level"] in ("basic", "intermediate", "complex"), f"{ident}: invalid level")
            complex_count += item["level"] == "complex"
            require(isinstance(item["source"], str) and bool(item["source"].strip()), f"{ident}: empty source")
            require(isinstance(item["features"], list) and bool(item["features"]), f"{ident}: missing features")
            provenance = item.get("upstream", {})
            require(bool(provenance.get("path") and provenance.get("section")), f"{ident}: missing provenance")
            require("skip" not in item and "expectedFailure" not in item, f"{ident}: regressions cannot be skipped")
            key = json.dumps([item["source"].strip(), item.get("config", {})], ensure_ascii=False)
            require(key not in inputs, f"{ident}: duplicate input does not add coverage")
            inputs.add(key)
            records.append({"family": document["family"], **item})
        require(path.stem == "info" or complex_count >= 2, f"{path}: multiple complex regressions required")
        summary.append({"family": path.stem, "cases": len(cases), "complex": complex_count,
                        "features": sorted({f for item in cases for f in item["features"]}),
                        "sha256": digest(data)})
    return records, summary
