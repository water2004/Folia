#!/usr/bin/env python3
"""Compare full pinned SVG, native glyph paths and both native session modes."""
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from validation.common import MISSING, difference, emit, equal, field, loads, require
from validation.processes import execute


def json_values(value):
    if isinstance(value, dict):
        return {key: json_values(item) for key, item in value.items()}
    if isinstance(value, list):
        return list(map(json_values, value))
    return 0.0 if isinstance(value, (float, int)) and not isinstance(value, bool) and value == 0 else value


def same(left, right):
    # Original JSON.stringify comparisons retain object order and normalize -0.
    return difference(json_values(left), json_values(right), ordered=True) is None


def compare(source, native):
    equal(source['groups'], native['groups'])
    equal(len(source['records']), len(native['records']))
    mismatches, glyph_matches = [], 0
    for expected, actual in zip(source['records'], native['records']):
        label = expected['group'] + '/' + expected['name']
        for key in ['index', 'name', 'tex', 'configuration', 'display', 'em', 'ex', 'width', 'svg', 'mmlError', 'error']:
            left, right = field(expected, key), field(actual, key)
            if not same(left, right):
                if key == 'svg':
                    left, right = ['length ' + (str(len(value)) if isinstance(value, str) else 'null') for value in [left, right]]
                mismatch = {'case': label, 'field': key}
                if left is not MISSING:
                    mismatch['source'] = left
                if right is not MISSING:
                    mismatch['native'] = right
                mismatches.append(mismatch)
        source_paths = [{'codepoint': None if row['codepoint'] is None else int(row['codepoint'], 16), 'd': row['d']}
                        for row in expected['pathInventory']]
        native_paths = [{'codepoint': row['codepoint'], 'd': row['d']} for row in actual['glyphPaths']]
        if same(source_paths, native_paths):
            glyph_matches += 1
        else:
            mismatches.append({'case': label, 'field': 'glyphPaths', 'source': source_paths, 'native': native_paths})
    return {'accepted': not mismatches, 'cases': len(source['records']),
            'svgExact': sum(field(left, 'svg') == field(right, 'svg') for left, right in zip(source['records'], native['records'])),
            'nativeSceneSnapshots': sum(isinstance(row.get('scene'), list) for row in native['records']),
            'glyphPathExact': glyph_matches, 'mismatches': mismatches}


def main(args):
    require(len(args) == 1, 'Expected FoliaMathBboldxTextSvgProbe executable')
    folder = Path(__file__).resolve().parent
    result, stdout, stderr = execute(os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'), [folder / 'source-witness.cjs'], timeout=120)
    require(result['exitCode'] == 0 and not result.get('timedOut'), stderr.decode('utf-8', errors='replace') or str(result))
    source = loads(stdout.decode('utf-8'))
    result, stdout, stderr = execute(sys.executable, [folder / 'run-native.py', args[0]], timeout=150)
    require(result['exitCode'] == 0 and not result.get('timedOut'), stderr.decode('utf-8', errors='replace') or str(result))
    report = compare(source, loads(stdout.decode('utf-8')))
    emit(report)
    return int(not report['accepted'])


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
