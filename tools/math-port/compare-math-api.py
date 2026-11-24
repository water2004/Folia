#!/usr/bin/env python3
"""Compare the math-api native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("math-api", sys.argv[1:]))
