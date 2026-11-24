"""Full-record comparison for native math hosts and parser sessions."""
from .common import (ROOT, MISSING, default_path, read, digest, require, equal,
                     difference, field, keys, truthy, oracle, write, emit, records_report)


def base_methods(args):
    source = read(default_path(args, 0, 'build/math-port/base-methods-original.json'))
    native = read(default_path(args, 1, 'build/math-port/base-methods-native.json'))
    failures = []
    for label, expected, actual in [('methodNames', field(source, 'methodNames'), field(native, 'methodNames')),
                                   ('case names', [row['name'] for row in source['cases']], keys(native['cases']))]:
        found = difference(expected, actual, label)
        if found:
            failures.append(found)
    for row in source['cases']:
        actual = native['cases'].get(row['name'])
        if actual is None:
            continue  # Already rejected by the complete case-name check.
        checks = [('packages', field(row, 'packages'), field(actual, 'packages'))]
        if truthy(field(row, 'error')):
            checks.append(('error', row['error'], field(actual, 'error')))
        elif truthy(field(actual, 'error')):
            checks.append(('error', None, actual['error']))
        else:
            checks.extend((name, field(field(row, 'result'), name), field(field(actual, 'result'), name))
                          for name in ['tree', 'parser'])
        for name, expected, got in checks:
            found = difference(expected, got, row['name'] + '/' + name)
            if found:
                failures.append(found)
    emit({'compared': len(source['cases']), 'accepted': not failures, 'failures': failures})
    return int(bool(failures))


def styles(args):
    source_path = ROOT / 'build/math-port/common-styles-original.json'
    native_path = default_path(args, 0, 'build/math-port/common-styles-native.json')
    source, native = read(source_path), read(native_path)
    fresh = oracle('tools/math-port/common-styles-original-oracle.cjs')
    for name in ['version', 'hashes', 'cjsHashes', 'records']:
        equal(field(source, name), field(fresh, name), 'pinned original ' + name)
    equal(source['version'], native['version'], 'native version differs')
    equal([row['name'] for row in source['records']], [row['name'] for row in native['records']], 'row coverage/order')
    for expected, actual in zip(source['records'], native['records']):
        equal(expected, actual, expected['name'])
    print(f'Styles/StyleJson strict Source differential passed: {len(source["records"])} records')
    return 0


def math_api(args):
    require(len(args) >= 2, 'Expected source JSON, native JSON [summary JSON]')
    source_path, native_path = default_path(args, 0, ''), default_path(args, 1, '')
    source, native = read(source_path), read(native_path)
    equal('4fe64e87ccaf386e19f67defd1d940d5332b247151d4d0f41b90274a500f443c', digest(source_path), 'Source output hash')
    equal(digest(ROOT / 'tools/math-port/math-api-cases.json'), source['corpusSha256'], 'corpus hash')
    equal(1, source['schema'], 'Source schema')
    equal(1, native['schema'], 'native schema')
    equal(9, len(source['records']), 'Source record count')
    equal(9, len(native['records']), 'native record count')
    matched, failures = records_report(source['records'], native['records'], 'records')
    equal(sum(field(row, 'svg') is not None for row in native['records']), native['detachedScenes'], 'detached scene count')
    require(native['detachedSceneNodes'] > 0, 'detached scene nodes must be nonempty')
    summary = {'total': 9, 'matched': matched, 'accepted': matched == 9, 'failures': failures}
    write(default_path(args, 2, 'build/math-port/math-api-comparison.json'), summary)
    emit({name: summary[name] for name in ['total', 'matched', 'accepted']})
    return int(not summary['accepted'])


def output_jax(args):
    source = read(default_path(args, 0, 'build/math-port/output-jax-original.json'))
    fresh = oracle('tools/math-port/output-jax-original-oracle.cjs')
    equal(source, fresh, 'original behavior/source hashes differ from frozen baseline')
    equal(14, len(source['records']), 'original fixture coverage')
    native = read(default_path(args, 1, 'build/math-port/output-jax-native.json'))
    equal(sorted(keys(source)), sorted(keys(native)), 'native top-level schema')
    for name in ['version', 'sourceFile', 'sourceHash', 'cjsHash']:
        equal(field(source, name), field(native, name), 'native ' + name)
    equal([row['name'] for row in source['records']], [row['name'] for row in native['records']], 'row coverage/order')
    failures = [expected['name'] for expected, actual in zip(source['records'], native['records']) if difference(expected, actual)]
    emit({'total': 14, 'matched': 14 - len(failures), 'failures': failures})
    return int(bool(failures))
