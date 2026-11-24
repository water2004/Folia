#!/usr/bin/env python3
"""Check all pinned container Source profiles with the portable comparator."""
from validation.cli import run
from validation.source_profiles import main

if __name__ == '__main__':
    raise SystemExit(run(lambda args: main('container', args)))
