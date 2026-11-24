#!/usr/bin/env python3
"""Compare the complete pinned native observation without a Node test runner."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.cli import run
from validation.packages import boldsymbol

if __name__ == "__main__":
    raise SystemExit(run(lambda args: boldsymbol(args, native_dump=True)))
