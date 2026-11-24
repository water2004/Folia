#!/usr/bin/env python3
"""Compare the base-methods native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("base-methods", sys.argv[1:]))
