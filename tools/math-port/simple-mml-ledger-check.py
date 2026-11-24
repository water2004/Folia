#!/usr/bin/env python3
"""Validate the complete pinned simple-mml member ledger in Python."""
from validation.cli import run
from validation.ledgers import main

if __name__ == '__main__':
    raise SystemExit(run(lambda args: main('simple-mml', args)))
