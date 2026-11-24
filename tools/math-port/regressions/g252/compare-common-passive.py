#!/usr/bin/env python3
"""Compare all 75 Common records against unchanged pinned Source captures."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from validation.cli import run
from validation.common_passive import main

if __name__ == '__main__':
    raise SystemExit(run(main))
