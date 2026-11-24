#!/usr/bin/env python3
"""Compare complete structured references, tolerating only approved geometry."""
import argparse
from validation.common import emit, read
from validation.reference import compare


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('expected')
    parser.add_argument('actual')
    args = parser.parse_args()
    differences = compare(read(args.expected), read(args.actual))
    emit({'equal': not differences, 'differences': differences})
    return int(bool(differences))


if __name__ == '__main__':
    raise SystemExit(main())
