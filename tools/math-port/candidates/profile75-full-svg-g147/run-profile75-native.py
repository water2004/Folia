#!/usr/bin/env python3
"""Preserve the complete 75-case profile contract with portable Python tooling."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from validation.cli import run
from validation.profile75.native import run_native

if __name__ == "__main__":
    raise SystemExit(run(run_native))
