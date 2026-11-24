#!/usr/bin/env python3
"""Compare the tex-six-packages native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("tex-six-packages", sys.argv[1:]))
