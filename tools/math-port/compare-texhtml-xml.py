#!/usr/bin/env python3
"""Regenerate official Source fixtures, then compare native records in Python."""
from validation.cli import run
from validation.texhtml import main

if __name__ == '__main__':
    raise SystemExit(run(main))
