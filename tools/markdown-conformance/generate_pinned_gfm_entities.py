#!/usr/bin/env python3
"""Generate Folia parsing cases from the pinned cmark-gfm entity table.

The pinned source is cached under build/dependencies/markdown-conformance/gfm.
This emits lossless runtime TSV data only; it does not link cmark.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import urllib.request
from pathlib import Path
from fixture_table import escape

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "build" / "dependencies" / "markdown-conformance" / "gfm"
ENTITY_FILE = CACHE / "entities.inc"
OUTPUT = ROOT / "tests/parsing/fixtures/conformance/entities.tsv"
BUNDLED = ROOT / "tests/parsing/fixtures/conformance/upstream/gfm/entities.inc"
PATTERN = re.compile(r'^\{\(unsigned char\*\)"([^"]+)", \{([^}]+)\}', re.MULTILINE)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=ENTITY_FILE)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    parser.add_argument("--fetch", action="store_true", help="download the locked source when absent")
    args = parser.parse_args()
    lock = json.loads((Path(__file__).with_name("gfm-sources.lock.json")).read_text(encoding="utf-8"))
    pin = lock["commit"]
    entry = next(source for source in lock["sources"] if source["name"] == "entities.inc")
    expected_hash = entry["sha256"]
    if args.source == ENTITY_FILE and not args.source.exists() and BUNDLED.is_file():
        args.source = BUNDLED
    if not args.source.exists():
        if not args.fetch:
            raise SystemExit(f"missing cached entity source {args.source}; rerun with --fetch")
        request = urllib.request.Request(
            entry["url"], headers={"User-Agent": "Folia GFM conformance fixture loader"})
        with urllib.request.urlopen(request, timeout=30) as response:
            downloaded = response.read()
        download_hash = hashlib.sha256(downloaded).hexdigest()
        if download_hash != expected_hash:
            raise SystemExit(f"downloaded entity source hash mismatch: {download_hash}")
        args.source.parent.mkdir(parents=True, exist_ok=True)
        args.source.write_bytes(downloaded)
    raw = args.source.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if digest != expected_hash:
        raise SystemExit(f"pinned entity source hash mismatch: {digest}")
    rows = []
    for name, byte_list in PATTERN.findall(raw.decode("utf-8")):
        encoded = [int(value.strip()) for value in byte_list.split(",") if value.strip()]
        # entities.inc stores each UTF-8 byte string in a NUL-terminated array.
        if not encoded or encoded[-1] != 0:
            raise SystemExit(f"entity {name} is missing its NUL terminator")
        payload = bytes(encoded[:-1])
        decoded = payload.decode("utf-8")
        rows.append((name, decoded))
    if len(rows) != 2125:
        raise SystemExit(f"expected 2125 cmark-gfm named entities, found {len(rows)}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Independent cmark-gfm named entity observations; do not edit.",
        f"# Source commit: {pin}; SHA256: {digest}; records: {len(rows)}.",
        "# source\tdecoded",
    ]
    lines.extend(escape('&' + name + ';') + "\t" + escape(decoded) for name, decoded in rows)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {len(rows)} records to {args.output}")


if __name__ == "__main__":
    main()
