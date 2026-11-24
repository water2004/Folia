#!/usr/bin/env python3
"""Compare the common-styles-native native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("common-styles-native", sys.argv[1:]))
