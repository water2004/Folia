#!/usr/bin/env python3
"""Observe selected native parser executables and compare every frozen case."""
import sys
from validation.checkpoints import parser_checkpoint

if __name__ == '__main__':
    raise SystemExit(parser_checkpoint(sys.argv[1:]))
