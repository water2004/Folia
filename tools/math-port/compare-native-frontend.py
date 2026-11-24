#!/usr/bin/env python3
"""Compare the complete pinned native observation without a Node test runner."""
from validation.cli import run
from validation.packets import native_frontend

if __name__ == "__main__":
    raise SystemExit(run(native_frontend))
