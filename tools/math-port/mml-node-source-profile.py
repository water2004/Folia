#!/usr/bin/env python3
"""Verify canonical and original MmlNode Source observation sequences."""
from validation.cli import run
from validation.observer_packets import direct

if __name__ == "__main__":
    raise SystemExit(run(lambda args: direct("mml-node", args)))
