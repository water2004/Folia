"""Conservative responsibility labels and exact Markdown/header note indexes."""

import re

from .files import unique


def responsibility(path):
    name = path.lower()
    if re.search(r"(?:^|/)(?:license-[^/]+|notice)\.txt$", name):
        return {"decision": "keep-license", "responsibilityGroup": "third-party-license", "namedOwner": "Root (license provenance)"}
    patterns = [("/dependencies/mhchem_parser/", "math.tex.mhchem"), ("/input/tex/", "math.tex"),
                ("/output/common/", "math.common"), ("/output/scene/", "math.scene"), ("/output/svg/", "math.svg"),
                ("/fonts/", "math.fonts"), ("/util/", "math.util"), ("/core/mml_tree/", "math.mml"),
                ("/core/tree/", "math.tree"), ("/handlers/html/", "math.html"), ("/adaptors/", "math.dom-adaptors"),
                ("/dom_", "math.dom-adaptors"), ("/core/", "math.core"), ("/native_renderer_bridge.cpp", "math.c-abi-bridge"),
                ("/math.cpp", "math.renderer"), ("/math.ixx", "math.renderer")]
    group = next((group for pattern, group in patterns if pattern in name), "math.other")
    return {"decision": "unreviewed", "responsibilityGroup": group, "namedOwner": "unassigned"}


class SourceAnnotations:
    def __init__(self, repo, facts):
        self.repo, self.facts = repo, facts
        self.lines = [(path, index + 1, line) for path in repo.files("docs/math-port") if path.endswith(".md")
                      for index, line in enumerate(re.split(r"\r?\n", repo.read(path)))]

    def for_file(self, path, facts):
        if re.search(r"/(?:LICENSE-[^/]+|NOTICE)\.txt$", path, re.I):
            return []
        module = facts["declaration"]
        if not module and path.endswith(".cpp") and self.repo.exists(path[:-4] + ".ixx"):
            module = self.facts(path[:-4] + ".ixx")["declaration"]
        results = []
        for document, number, line in self.lines:
            if path not in line and not (module and module in line):
                continue
            sources = unique(re.findall(r"(?:\.reference/MathJax-src/)?ts/[A-Za-z0-9_./-]+\.(?:ts|js)", line))
            if not sources:
                continue
            cells = [re.sub(r"^`|`$", "", cell.strip()) for cell in line.split("|")]
            source_hash, evidence = None, ""
            ts_index = next((index for index, cell in enumerate(cells) if re.match(r"^(?:\.reference/MathJax-src/)?ts/", cell)), -1)
            adjacent = cells[ts_index + 1] if 0 <= ts_index < len(cells) - 1 else ""
            if re.fullmatch(r"[a-f0-9]{64}", adjacent, re.I):
                source_hash, evidence = adjacent.lower(), "adjacent table cell"
            else:
                hashes = unique(re.findall(r"\b[a-f0-9]{64}\b", line, re.I))
                if len(sources) == len(hashes) == 1:
                    source_hash, evidence = hashes[0].lower(), "single hash on exact mapping line"
            results.append({"document": document, "line": number, "sourcePaths": sources,
                            "sourceSha256": source_hash, "hashEvidence": evidence,
                            "status": "annotated" if source_hash else "path-only-or-ambiguous"})
        if re.search(r"\.(?:cpp|ixx|inc)$", path):
            for index, line in enumerate(re.split(r"\r?\n", self.repo.read(path))[:12]):
                sources = unique(re.findall(r"[A-Za-z0-9_./-]+\.ts\b", line))
                hashes = unique(re.findall(r"\b[a-f0-9]{64}\b", line, re.I))
                if sources or hashes:
                    results.append({"document": path, "line": index + 1, "sourcePaths": sources,
                                    "sourceSha256": hashes[0].lower() if len(hashes) == 1 else None,
                                    "hashEvidence": "source-file header" if len(hashes) == 1 else "",
                                    "status": "header-annotated" if any("ts/" in item for item in sources) and len(hashes) == 1 else "path-or-hash-incomplete"})
        return results
