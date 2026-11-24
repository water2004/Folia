#!/usr/bin/env python3
"""Verify all canonical and original MathChoice Source observations."""
from validation.cli import run
from validation.observer_packets import direct

if __name__ == "__main__":
    raise SystemExit(run(lambda args: direct("mathchoice", args)))
