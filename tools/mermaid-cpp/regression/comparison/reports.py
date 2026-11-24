"""Strict linkage between source corpora, capture reports, and SVG bytes."""

from __future__ import annotations

import json
from pathlib import Path

from .corpus import digest, require


class Report:
    def __init__(self, argument):
        path = Path(argument).resolve()
        self.path = path / "report.json" if path.is_dir() else path
        self.directory = self.path.parent
        self.document = json.loads(self.path.read_text(encoding="utf-8"))
        require(isinstance(self.document.get("results"), list), "Report lacks per-case results: " + str(self.path))
        self.rows = {}
        for item in self.document["results"]:
            require(item["id"] not in self.rows, "Duplicate report case " + item["id"])
            self.rows[item["id"]] = item

    def source(self, item, corpus_hash):
        snapshot = self.document.get("corpusHashes", {}).get(item["family"] + ".json")
        if snapshot is None:
            snapshot = next((row["sha256"] for row in self.document.get("coverage", [])
                             if row["family"] == item["family"]), None)
        require(snapshot == corpus_hash, f"Corpus snapshot mismatch for {item['family']} in {self.path}")
        row = self.rows.get(item["id"])
        require(row is not None, f"Missing case {item['id']} in {self.path}")
        require(row.get("family") == item["family"], "Wrong report family for " + item["id"])
        require(row.get("ok") is True, f"SVG generation failed for {item['id']}: {row.get('error', 'unknown error')}")
        require(isinstance(row.get("svgSha256"), str), "Report has no SVG evidence for " + item["id"])
        path = (self.directory / row.get("svgFile", item["id"] + ".svg")).resolve()
        require(path.is_file(), "Missing SVG file " + str(path))
        data = path.read_bytes()
        require(digest(data) == row["svgSha256"], "SVG evidence changed for " + item["id"])
        return data.decode("utf-8")
