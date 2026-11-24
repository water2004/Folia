#!/usr/bin/env python3
"""Validate the complete pinned mfenced member ledger in Python."""
from validation.cli import run
from validation.ledgers import main

if __name__ == '__main__':
    raise SystemExit(run(lambda args: main('mfenced', args)))
