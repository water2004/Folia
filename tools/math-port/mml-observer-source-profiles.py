#!/usr/bin/env python3
"""Verify the original script/leaf Source observer profiles."""
from validation.cli import run
from validation.observer_packets import main

if __name__ == "__main__":
    raise SystemExit(run(lambda args: main("mml", args)))
