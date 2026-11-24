#!/usr/bin/env python3
"""Compare exact rational SVG geometry, transforms and every modeled paint field."""
from validation.cli import run
from validation.menclose import main

if __name__ == '__main__':
    raise SystemExit(run(main))
