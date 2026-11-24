"""Lossless UTF-8 TSV transport shared by independent Markdown observations."""

import json
from pathlib import Path


def escape(value: str) -> str:
    # Leave readable Unicode intact; protect separators, CR, LF, NUL and '\\'.
    return json.dumps(value, ensure_ascii=False)[1:-1]


def write(path: Path, rows, label: str) -> None:
    lines = ["# " + label, "# example\tsection\tmarkdown\texpected_html"]
    lines.extend("\t".join([str(identifier), *(escape(value) for value in values)])
                 for identifier, *values in rows)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(("\n".join(lines) + "\n").encode("utf-8"))


def read(path: Path) -> list:
    rows = []
    for number, line in enumerate(path.read_bytes().decode("utf-8").split("\n"), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4:
            raise ValueError(f"{path}:{number}: expected four columns")
        rows.append([int(fields[0]), *(json.loads('"' + cell + '"') for cell in fields[1:])])
    if not rows:
        raise ValueError(f"{path}: no fixture rows")
    return rows
