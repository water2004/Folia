#!/usr/bin/env python3
"""Preserve the locked public-renderer proof sequence validation."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from validation.cli import run
from validation.bussproofs import main

if __name__ == "__main__":
    raise SystemExit(run(main))
