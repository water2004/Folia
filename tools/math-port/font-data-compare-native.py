#!/usr/bin/env python3
"""Check every native glyph, resource, remap, alias and observed catalog count."""
from validation.cli import run
from validation.font.comparison import main

if __name__ == '__main__':
    raise SystemExit(run(main))
