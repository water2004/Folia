"""Portable native-math test comparisons; JavaScript is restricted to oracles."""
import subprocess
import sys

from .common import ComparisonError
from . import hosts, packages


def main(profile, args):
    handlers = {
        'ams-newcommand': packages.ams,
        'ams-newcommand-boundary': lambda values: packages.ams(values, boundary=True),
        'boldsymbol-configmacros': packages.boldsymbol,
        'boldsymbol-native-dump': lambda values: packages.boldsymbol(values, native_dump=True),
        'base-methods': hosts.base_methods,
        'tex-six-packages': packages.six_packages,
        'tex-next-seven': lambda values: packages.next_packages(values, seven=True),
        'tex-next-packages': packages.next_packages,
        'common-styles-native': hosts.styles,
        'math-api': hosts.math_api,
        'output-jax': hosts.output_jax,
    }
    try:
        return handlers[profile](args)
    except (ComparisonError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f'{profile}: {error}', file=sys.stderr)
        return 1
