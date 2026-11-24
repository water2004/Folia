#!/usr/bin/env python3
"""Compare the complete pinned native observation without a Node test runner."""
from validation.cli import run
from validation.packets import map_handler

if __name__ == "__main__":
    raise SystemExit(run(map_handler))
