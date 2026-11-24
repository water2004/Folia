#!/usr/bin/env python3
"""Compare the native Unicode observer against its unchanged pinned source."""
import sys
from validation.checkpoints import unicode_checkpoint

if __name__ == '__main__':
    raise SystemExit(unicode_checkpoint(sys.argv[1:]))
