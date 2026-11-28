"""Repository discovery and byte-preserving inventory I/O."""

import hashlib
import os
from datetime import datetime, timezone
from pathlib import Path


def discover_root(start=None):
    candidate = Path(start or __file__).resolve()
    if candidate.is_file():
        candidate = candidate.parent
    for directory in (candidate, *candidate.parents):
        if (directory / "CMakeLists.txt").exists() and (directory / "tools/math-port/native-raii-inventory.py").exists():
            return directory
    raise ValueError(f"Could not discover repository root from {start}")


def sha(data):
    return hashlib.sha256(data.encode("utf-8") if isinstance(data, str) else data).hexdigest()


def utc(milliseconds):
    return datetime.fromtimestamp(milliseconds / 1000, timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def relative(root, path):
    return Path(os.path.relpath(path, root)).as_posix()


def same_path(a, b):
    return os.path.normcase(os.path.abspath(a)) == os.path.normcase(os.path.abspath(b))


def unique(values):
    return list(dict.fromkeys(values))


class Repository:
    def __init__(self, root):
        self.root = Path(root).resolve()

    def read(self, path):
        return (self.root / path).read_bytes().decode("utf-8")

    def files(self, directory):
        base = self.root / directory
        if not base.is_dir():
            raise FileNotFoundError(base)
        return sorted(path.relative_to(self.root).as_posix() for path in base.rglob("*") if path.is_file())

    def exists(self, path):
        return (self.root / path).exists()

    def bytes(self, path):
        return (self.root / path).read_bytes()
