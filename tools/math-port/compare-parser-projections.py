#!/usr/bin/env python3
"""Compare the complete pinned native observation without a Node test runner."""
from validation.cli import run
from validation.packets import parser_projections

if __name__ == "__main__":
    raise SystemExit(run(parser_projections))
