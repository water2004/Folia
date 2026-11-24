#!/usr/bin/env python3
"""Validate complete pinned observations and their provenance with Python."""
from validation.cli import run
from validation.texttip import main

if __name__ == "__main__":
    raise SystemExit(run(main))
