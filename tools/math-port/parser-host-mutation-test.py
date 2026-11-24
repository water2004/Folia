#!/usr/bin/env python3
"""Validate or explicitly execute the unchanged native parser-host fault plan."""
from validation.cli import run
from validation.parser_faults import main

if __name__ == '__main__':
    raise SystemExit(run(main))
