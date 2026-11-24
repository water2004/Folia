#!/usr/bin/env python3
"""Compare the complete pinned native observation without a Node test runner."""
from validation.cli import run
from validation.packets import table_layout

if __name__ == "__main__":
    raise SystemExit(run(table_layout))
