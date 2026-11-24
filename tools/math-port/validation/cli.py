"""Consistent failure status for standalone validation entrypoints."""
import subprocess
import sys

from .common import ComparisonError


def run(function, args=None):
    try:
        return function(sys.argv[1:] if args is None else args)
    except (ComparisonError, OSError, ValueError, KeyError, TypeError, IndexError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 1
