#!/usr/bin/env python3
"""Preserve the full profile75 regression workflow without a JavaScript runner."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from validation.cli import run
from validation.profile75.selftest import main

if __name__ == "__main__":
    raise SystemExit(run(main))
