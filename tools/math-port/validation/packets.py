"""Exact structured observations, schema coverage and native owner checks."""
import hashlib
import os
from pathlib import Path
import sys

from .common import (ROOT, MISSING, UPSTREAM_COMMIT, default_path, difference, digest, emit, equal,
                     field, keys, loads, read, records_report, require, root_path, truthy, write)
from .processes import execute

SESSION_VALUES = {
    'sameModuleOwnersShareMap': True,
    'moduleAStillOwnsOriginalMapAfterModuleBRegistersSameName': True,
    'moduleBOwnsItsMap': True,
    'distinctModuleMaps': True,
    'interleavedCallbacks': ['A', 'B', 'A', 'B'],
}
SESSION_CHECKS = ['callbackIdentitiesPreserved', 'subHandlersShareMap', 'replacementVisibleToFutureAdds',
                  'replacementPreservesExistingHandler', 'tokenMapHostUsesSelectedOwner', 'deadOwnerRejected', 'registryRelease']
OWNER_CHECKS = ['variantMapStorageIdentity', 'functionListOwnerIdentity',
                'nodeFactoryConfigurationValueOwnerRetainsReplacement', 'nodeFactoryNativeCallableBodiesShared',
                'nodeFactoryContextFunctionIdentityIsDistinct', 'nodeFactoryContextFunctionPropertiesAreIndependent']


def json_values(value):
    """JSON.stringify equality collapses signed zero but keeps field order."""
    if isinstance(value, dict):
        return {key: json_values(item) for key, item in value.items()}
    if isinstance(value, list):
        return list(map(json_values, value))
    return 0.0 if isinstance(value, (float, int)) and not isinstance(value, bool) and value == 0 else value


def same_json(left, right):
    return difference(json_values(left), json_values(right), ordered=True) is None


def concat_report(source, native, mapping):
    expected_rows = {row['name']: row for row in source['rows']}
    actual_rows = {row['name']: row for row in native['rows']}
    mismatches, boundaries, matched = [], [], []
    if source.get('method') != 'Array.prototype.concat' or native.get('method') != source.get('method'):
        mismatches.append({'reason': 'method identity differs', 'source': source.get('method'), 'native': native.get('method')})
    if source.get('schema') != 'tagged-slots-v1' or native.get('schema') != source.get('schema'):
        mismatches.append({'reason': 'structured schema differs', 'source': source.get('schema'), 'native': native.get('schema')})
    for row in mapping['rows']:
        expected, actual = expected_rows.get(row['source']), actual_rows.get(row['source'])
        if expected is None or actual is None:
            mismatches.append({'name': row['source'], 'reason': 'missing source row' if expected is None else 'missing native row'})
        elif row['status'] == 'unsupported':
            boundaries.append({'name': row['source'], 'sourceError': expected.get('error'),
                               'nativeError': actual.get('error'), 'nativeResult': actual.get('result')})
            if expected.get('error') != 'ok' or actual.get('error') == 'ok':
                mismatches.append({'name': row['source'], 'reason': 'unsupported boundary did not execute both sides as expected'})
        elif not same_json(expected, actual):
            mismatches.append({'name': row['source'], 'expected': expected, 'actual': actual})
        else:
            matched.append(row['source'])
    for side, rows in [('source', expected_rows), ('native', actual_rows)]:
        if len(rows) != len(mapping['rows']):
            mismatches.append({'reason': side + ' row count differs from row map', side: len(rows), 'mapped': len(mapping['rows'])})
    return {'sourceNode': source.get('node'), 'nativeSchema': native.get('schema'), 'matched': len(matched),
            'boundaries': boundaries, 'mismatches': mismatches}


def array_concat(args):
    if len(args) != 2:
        print('Expected source.json and native.json', file=sys.stderr)
        return 2
    report = concat_report(read(args[0]), read(args[1]), read(ROOT / 'tools/math-port/array-concat-row-map.json'))
    emit(report)
    return 1 if report['mismatches'] else 2 if report['boundaries'] else 0


def session_shape(original, native):
    expected = {'name': 'map-handler-two-module-instances-interleaved', 'values': SESSION_VALUES}
    equal(['sourceLockSha256', 'cases'], keys(original), 'Source output keys/order')
    equal([expected], original['cases'], 'Source witness case/complete value shape')
    equal(['cases', 'nativeChecks'], keys(native), 'Native output keys/order')
    equal(['name', 'values'], keys(native['cases'][0]), 'Native case keys/order')
    equal(list(SESSION_VALUES), keys(native['cases'][0]['values']), 'Native Source values keys/order')
    equal(original['cases'], native['cases'], 'Complete original observation')
    equal(1, len(native['cases']), 'Native case count')
    equal(SESSION_CHECKS, keys(native['nativeChecks']), 'Native checks keys/order')
    for key in SESSION_CHECKS:
        equal(True, native['nativeChecks'][key], 'Native-only check ' + key)


def map_handler(args):
    source_path = default_path(args, 0, 'build/math-port/map-handler-session-original.json')
    native_path = default_path(args, 1, 'build/math-port/map-handler-session-native.json')
    lock_path = ROOT / 'tools/math-port/map-handler-session-reference/source-lock.json'
    lock, original, native = read(lock_path), read(source_path), read(native_path)
    equal('53ccdf3ee3b017c46be3a697efeb789a355ed64d4e865e88471d9f3c57f14afe', digest(lock_path), 'MapHandler Source lock')
    equal(1, lock['schema'])
    equal('4.1.3', lock['mathJaxVersion'])
    equal(['ts/input/tex/MapHandler.ts'], [row['file'] for row in lock['sources']])
    equal('f7a03711e682f19c70f071c3412a127341c377929b9cefd9ad563e8061053e9e', digest(source_path), 'MapHandler Source output')
    equal(digest(lock_path), original['sourceLockSha256'])
    session_shape(original, native)
    emit({'accepted': True, 'cases': len(native['cases']), 'nativeChecks': len(SESSION_CHECKS),
          'sourceLockSha256': digest(lock_path), 'originalSha256': digest(source_path), 'nativeSha256': digest(native_path)})
    return 0


def projection_comparison(source, native):
    failures = []
    for name, row in source.items():
        found = difference(row, field(native, name), name, ordered=True)
        if found:
            failures.append({'record': name, 'difference': found})
    try:
        equal(['nodeFactory', 'parseOptions', 'runtimeOwners'], keys(native))
        equal(OWNER_CHECKS, keys(native['runtimeOwners']))
        for key in OWNER_CHECKS:
            equal(True, native['runtimeOwners'][key], key)
    except RuntimeError as error:
        failures.append({'record': 'runtimeOwners', 'difference': str(error)})
    return failures


def parser_projections(args):
    require(len(args) == 2, 'Native observation and report paths are required')
    native_path, report_path = map(lambda value: Path(value).resolve(), args)
    source, provenance = {}, []
    for name, script in [('nodeFactory', 'tools/math-port/drafts/parser-projection-original-oracle.cjs'),
                         ('parseOptions', 'tools/math-port/drafts/parser-options-projection-original-oracle.cjs')]:
        result, stdout, stderr = execute(os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'), [ROOT / script])
        require(result['exitCode'] == 0 and not result.get('timedOut'), stderr.decode('utf-8', errors='replace') or str(result))
        source[name] = loads(stdout.decode('utf-8'))
        provenance.append({'script': script, 'scriptSha256': digest(ROOT / script), 'outputSha256': hashlib.sha256(stdout).hexdigest()})
    failures = projection_comparison(source, read(native_path))
    report = {'accepted': not failures, 'sourceRecords': 2, 'sourceProvenance': provenance,
              'native': {'path': str(native_path), 'sha256': digest(native_path)}, 'failures': failures}
    write(report_path, report)
    emit(report)
    return int(not report['accepted'])


def native_frontend(args):
    result, stdout, stderr = execute(os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'),
                                    [ROOT / 'tools/math-port/textmacros-native-tree-oracle.cjs', '--check'])
    sys.stdout.buffer.write(stdout)
    sys.stderr.buffer.write(stderr)
    require(result['exitCode'] == 0 and not result.get('timedOut'), 'Pinned MathJax source fixture differs')
    source = read(ROOT / 'docs/math-port/evidence/textmacros-native-tree-source.json')
    native_path = default_path(args, 0, 'build/math-port/native-frontend.json')
    native = read(native_path)
    equal(source['schema'], field(native, 'schema'), 'Native schema differs')
    equal(sorted(source['cases']), sorted(native['cases']), 'Native case coverage')
    for name in sorted(source['cases']):
        equal(source['cases'][name], native['cases'][name], name)
    emit({'matched': len(source['cases']), 'source': 'pinned MathJax CJS', 'native': str(native_path)})
    return 0


def table_layout(args):
    source_path = root_path(args, 0, 'build/math-port/table-layout-source.json')
    native_path = root_path(args, 1, 'build/math-port/table-layout-native-current.json')
    summary_path = root_path(args, 2, 'build/math-port/table-layout-comparison.json')
    source, native = read(source_path), read(native_path)
    equal(1, source['schema'])
    equal(source['corpusSha256'], native['tableCorpusSha256'])
    expected, actual = source.get('records') or [], native.get('records') or []
    matched, failures = records_report(expected, actual, 'records')
    report = {'source': str(source_path), 'native': str(native_path), 'total': len(expected), 'matched': matched,
              'accepted': not failures and matched == len(expected), 'failures': failures}
    write(summary_path, report)
    emit({'total': len(expected), 'matched': matched, 'accepted': report['accepted'], 'failures': len(failures),
          'firstFailure': failures[0] if failures else None, 'summary': str(summary_path)})
    return int(not report['accepted'])
