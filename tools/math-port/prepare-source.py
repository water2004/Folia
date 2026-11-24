#!/usr/bin/env python3
"""Restore exact locked TypeScript entity inputs from the installed MathJax package.

This performs no downloads and executes no JavaScript. A line-ending variant is
accepted only when its complete SHA-256 equals the existing source lock.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys


def pinned_bytes(raw: bytes, expected: str, name: str) -> bytes:
    lf = raw.replace(b"\r\n", b"\n")
    for candidate in (raw, lf, lf.replace(b"\n", b"\r\n")):
        if hashlib.sha256(candidate).hexdigest() == expected:
            return candidate
    raise ValueError("Pinned MathJax entity source mismatch: " + name)


def restore(root: Path, check: bool = False) -> int:
    lock = json.loads((root / "docs/math-port/evidence/entities-source-lock.json").read_bytes())
    package = root / "tools/mathjax-bundle/node_modules/@mathjax/src/ts/util"
    target = root / ".reference/MathJax-src/ts/util"
    writes = []
    for name, expected in lock["source"]["ts"].items():
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Invalid source-lock path: " + name)
        destination = target / name
        if destination.exists():
            raw = destination.read_bytes()
        elif check:
            raise ValueError("Missing locked MathJax source: " + name)
        else:
            source = package / name
            if not source.is_file():
                raise ValueError("Missing MathJax package inputs; run npm ci --prefix tools/mathjax-bundle --ignore-scripts")
            raw = source.read_bytes()
        expected_bytes = pinned_bytes(raw, expected, name)
        if not destination.exists() or destination.read_bytes() != expected_bytes:
            if check:
                raise ValueError("MathJax checkout bytes differ from the source lock: " + name)
            writes.append((destination, expected_bytes))
    # Validate every input before changing any file.
    for destination, data in writes:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    return len(lock["source"]["ts"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    count = restore(Path(__file__).resolve().parents[2], args.check)
    print(f"Verified {count} locked MathJax entity source files")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
