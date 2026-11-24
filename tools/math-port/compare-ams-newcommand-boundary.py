#!/usr/bin/env python3
"""Compare the ams-newcommand-boundary native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("ams-newcommand-boundary", sys.argv[1:]))
