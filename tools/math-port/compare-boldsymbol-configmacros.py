#!/usr/bin/env python3
"""Compare the boldsymbol-configmacros native observations with the pinned reference."""
import sys
from validation import main

if __name__ == "__main__":
    raise SystemExit(main("boldsymbol-configmacros", sys.argv[1:]))
