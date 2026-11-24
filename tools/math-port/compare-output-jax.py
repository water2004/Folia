#!/usr/bin/env python3
"""Compare the output-jax native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("output-jax", sys.argv[1:]))
