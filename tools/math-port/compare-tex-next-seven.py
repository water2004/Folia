#!/usr/bin/env python3
"""Compare the tex-next-seven native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("tex-next-seven", sys.argv[1:]))
