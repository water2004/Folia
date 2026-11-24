"""Real-browser observations for the saved historical family capture formats."""

from __future__ import annotations

import copy
import json
from pathlib import Path

from .corpus import require


TOOL_ROOT = Path(__file__).resolve().parents[2]
ROOT_OBSERVERS = {"agentflow", "railroad", "tree-view", "mindmap"}
RECAPTURE = {"eventmodeling", "railroad", "tree-view", "zenuml"}
PORTABLE_LABELS = {"requirement", "usecase", "swimlane", "eventmodeling"}


def observation(family):
    if family == "class-elk":
        family = "class"
    source = (TOOL_ROOT / "oracle/observations" / (family + ".mjs")).read_text(encoding="utf-8")
    declaration = "export default "
    require(source.count(declaration) == 1, "Capture module must export one browser observation")
    return "(" + source.split(declaration, 1)[1].rstrip().removesuffix(";") + ")"


def load_cases(family, path, tree_theme=None):
    rows = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(rows, list) and bool(rows), "Official capture must contain cases")
    if family == "er":
        require(len(rows) == 32, "ER official corpus must contain all 32 cases")
    if family == "tree-view" and tree_theme in ("dark", "default"):
        rows = [row for row in rows if row["name"].endswith("-dark") == (tree_theme == "dark")]
        require(bool(rows), "No Tree View records match the selected theme")
    return rows


def native_filename(family, row, oracle=False, measured=False):
    name = row.get("name", row.get("theme", row.get("file")))
    if family == "railroad":
        return row["file"]
    if family in ("venn", "wardley"):
        return name + ".svg"
    if family in ("agentflow", "eventmodeling"):
        name += "-" + row["theme"]
    prefix = "class" if family == "class-elk" else family
    if family == "xy" and oracle:
        prefix += "-oracle"
    if family == "mindmap" and measured:
        prefix += "-measured"
    return prefix + "-" + name + ".svg"


class Observer:
    def __init__(self, page, family):
        self.page, self.family = page, family
        self.script = observation(family)

    def capture(self, source, name):
        family, page = self.family, self.page
        if family == "eventmodeling":
            return page.evaluate(self.script, source)
        if family == "wardley":
            source = '<style>svg{font-family:"trebuchet ms"}</style>' + source
        page.set_content(source)
        if family in ROOT_OBSERVERS:
            return page.locator("svg").first.evaluate(self.script)
        if family in ("venn", "wardley"):
            return page.evaluate("() => (" + self.script + ")(document)")
        if family == "flowchart":
            return page.evaluate(self.script, name.startswith(("extra-shapes-", "neo-document-")))
        return page.evaluate(self.script)

    def pairs(self, rows, native_directory, oracle=False):
        """Yield every native/official observation pair, including both Mindmap modes."""
        family = self.family
        for row in rows:
            name = row.get("name", row.get("theme", row.get("file")))
            require(isinstance(name, str) and bool(name), "Official capture has no case identifier")
            if family == "ishikawa" and row.get("error") or family == "usecase" and not row.get("svg"):
                # These are upstream source failures, never successful SVG comparisons.
                yield {"name": name, "status": "official-render-error", "error": row.get("error")}, None, None
                continue
            expected = copy.deepcopy(row)
            if family in RECAPTURE:
                expected = self.capture(row["svg"], name)
            elif family == "usecase" and any("glyphBox" not in label for label in row["labels"]):
                labels = self.capture(row["svg"], name)["labels"]
                require(len(labels) == len(expected["labels"]), name + ": official painted glyph count")
                for label, observed in zip(expected["labels"], labels):
                    label["glyphBox"] = observed["glyphBox"]
            expected["name"] = name
            for measured in ((False, True) if family == "mindmap" else (False,)):
                file = native_directory / native_filename(family, row, oracle, measured)
                if family == "swimlane" and not file.exists() and name.endswith("-default"):
                    file = native_directory / ("swimlane-" + name[:-8] + ".svg")
                source = file.read_text(encoding="utf-8")
                if family in PORTABLE_LABELS:
                    require("<foreignObject" not in source, name + ": labels must survive SVG normalization")
                if family == "mindmap":
                    expected = dict(expected, _measured=measured)
                actual = self.capture(source, name)
                yield {"name": name, "file": file.name, "status": "passed"}, actual, expected
