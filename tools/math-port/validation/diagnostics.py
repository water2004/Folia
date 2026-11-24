"""Enumerate every recorded JSON difference without modifying either capture."""
from __future__ import annotations

import argparse
import math
import os
from pathlib import Path
import re
import sys

from .common import (ComparisonError, MISSING, ROOT, digest, emit, field, loads,
                     same_scalar, sorted_keys, write)


def report_value(value):
    """Keep JSON.stringify's omitted fields and signed-zero output convention."""
    if isinstance(value, dict):
        return {key: report_value(item) for key, item in value.items() if item is not MISSING}
    if isinstance(value, list):
        return [None if item is MISSING else report_value(item) for item in value]
    if isinstance(value, float):
        if not math.isfinite(value):
            return None
        if value == int(value):
            return int(value)
    return value


def enumerate_differences(expected, actual, at: str = '$') -> list[dict]:
    if same_scalar(expected, actual):
        return []
    if (expected is None or actual is None or not isinstance(expected, (dict, list))
            or not isinstance(actual, (dict, list))):
        return [report_value({'path': at, 'source': expected, 'native': actual})]
    if isinstance(expected, list) != isinstance(actual, list):
        return [{'path': at, 'sourceType': 'array' if isinstance(expected, list) else 'object',
                 'nativeType': 'array' if isinstance(actual, list) else 'object'}]
    result = []
    if isinstance(expected, list):
        if len(expected) != len(actual):
            result.append({'path': at + '.length', 'source': len(expected), 'native': len(actual)})
        for index, (left, right) in enumerate(zip(expected, actual)):
            result.extend(enumerate_differences(left, right, f'{at}[{index}]'))
        return result
    source_keys, native_keys = sorted_keys(expected), sorted_keys(actual)
    if source_keys != native_keys:
        result.append({'path': at + '.[keys]', 'source': source_keys, 'native': native_keys})
    for key in source_keys:
        if key in actual:
            result.extend(enumerate_differences(expected[key], actual[key], f'{at}.{key}'))
        else:
            result.append(report_value({'path': f'{at}.{key}', 'source': expected[key],
                                        'native': {'$missing': True}}))
    for key in native_keys:
        if key not in expected:
            result.append(report_value({'path': f'{at}.{key}', 'source': {'$missing': True},
                                        'native': actual[key]}))
    return result


def summarize(source, native) -> dict:
    source_cases, native_cases = field(source, 'cases'), field(native, 'cases')
    rows = []
    if isinstance(source_cases, list) and isinstance(native_cases, list):
        if len(source_cases) != len(native_cases):
            rows.append({'name': '<cases>', 'differences': [{'path': 'cases.length',
                         'source': len(source_cases), 'native': len(native_cases)}]})
        for index, expected in enumerate(source_cases):
            name = field(expected, 'name')
            label = 'undefined' if name is MISSING else str(name)
            actual = native_cases[index] if index < len(native_cases) else MISSING
            rows.append(report_value({'name': name, 'differences': enumerate_differences(
                expected, actual, f'cases[{index}]({label})')}))
    else:
        # The former script passed a callback where its recursion needed an
        # array, crashing on unequal non-corpus inputs. Retain them as one row.
        rows.append({'name': '$', 'differences': enumerate_differences(source, native)})
    groups = {}
    for row in rows:
        for difference in row['differences']:
            path = re.sub(r'^cases\[\d+\]\([^)]*\)\.', 'case.', difference['path'])
            group = groups.setdefault(path, {'path': path, 'count': 0, 'samples': []})
            group['count'] += 1
            if len(group['samples']) < 5:
                group['samples'].append(report_value({'case': field(row, 'name'),
                    'source': field(difference, 'source'), 'native': field(difference, 'native')}))
    # Diagnostic presentation has one portable order, independent of the
    # operating system's default locale. Difference traversal remains UTF-16.
    ordered = sorted(groups.values(), key=lambda row: (
        -row['count'], row['path'].encode('utf-16-be', errors='surrogatepass')))
    return {'cases': len(rows), 'matched': sum(not row['differences'] for row in rows),
            'totalDifferences': sum(len(row['differences']) for row in rows),
            'groups': ordered, 'rows': rows}


def diagnose(source_path: Path, native_path: Path, output: Path, *, root: Path = ROOT) -> dict:
    source, native = loads(source_path.read_bytes()), loads(native_path.read_bytes())
    summary = {'source': os.path.relpath(source_path, root), 'sourceSha256': digest(source_path),
               'native': os.path.relpath(native_path, root), 'nativeSha256': digest(native_path),
               **summarize(source, native)}
    write(output, summary, parents=True)
    return {'cases': summary['cases'], 'matched': summary['matched'],
            'totalDifferences': summary['totalDifferences'], 'groups': len(summary['groups']),
            'firstGroups': summary['groups'][:12], 'output': os.path.relpath(output, root)}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source')
    parser.add_argument('native')
    parser.add_argument('summary', nargs='?')
    args = parser.parse_args(argv)
    try:
        emit(diagnose((ROOT / args.source).resolve(), (ROOT / args.native).resolve(),
                      (ROOT / (args.summary or args.native + '.all-differences.json')).resolve()))
        return 0  # A diagnostic report does not turn observed differences into a gate.
    except (OSError, ValueError, ComparisonError) as error:
        print(str(error), file=sys.stderr)
        return 1
