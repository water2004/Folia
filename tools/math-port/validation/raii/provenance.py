"""Conservative responsibility labels and exact Markdown/header note indexes."""

import re
import json

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
        annotation_path = "tools/math-port/reference/source-annotations.json"
        payload = json.loads(repo.read(annotation_path))
        if (payload.get("schema") != "folia.math.source-annotations.v1"
                or not payload.get("provenanceRevision")
                or not isinstance(payload.get("files"), dict)
                or not isinstance(payload.get("modules"), dict)):
            raise ValueError(f"Invalid source annotation index: {annotation_path}")
        self.provenance_revision = payload["provenanceRevision"]
        records, file_index, module_index = payload.get("records"), payload["files"], payload["modules"]
        if not isinstance(records, list) or any(not isinstance(item, dict) for item in records):
            raise ValueError(f"Invalid source annotation records: {annotation_path}")

        def expand(index, label):
            if not isinstance(index, dict):
                raise ValueError(f"Invalid {label} index: {annotation_path}")
            expanded = {}
            for key, references in index.items():
                if not isinstance(key, str) or not isinstance(references, list) or any(
                        not isinstance(reference, int) or reference < 0 or reference >= len(records)
                        for reference in references):
                    raise ValueError(f"Invalid {label} references: {annotation_path}")
                expanded[key] = [records[reference] for reference in references]
            return expanded

        self.by_path = expand(file_index, "source path")
        self.by_module = expand(module_index, "module")
        for path, annotations in self.by_path.items():
            if not isinstance(path, str) or any(not isinstance(item, dict) for item in annotations):
                raise ValueError(f"Invalid source annotation entries: {annotation_path}")

    def for_file(self, path, facts):
        if re.search(r"/(?:LICENSE-[^/]+|NOTICE)\.txt$", path, re.I):
            return []
        module = facts["declaration"]
        if not module and path.endswith(".cpp") and self.repo.exists(path[:-4] + ".ixx"):
            module = self.facts(path[:-4] + ".ixx")["declaration"]
        results = []
        annotations = [*self.by_path.get(path, []), *(self.by_module.get(module, []) if module else [])]
        seen = set()
        for annotation in annotations:
            identity = (annotation.get("document"), annotation.get("line"), tuple(annotation.get("sourcePaths", [])))
            if identity in seen:
                continue
            seen.add(identity)
            results.append({**annotation, "documentRevision": self.provenance_revision})
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
