"""Native TeX package observers compared against complete locked Source data."""
from .common import (ROOT, UPSTREAM_COMMIT, MISSING, default_path, root_path, read,
                     digest, require, equal, difference, field, keys, truthy, records_report, write, emit)


def ams(args, *, boundary=False):
    suffix = '-boundary' if boundary else ''
    source = read(default_path(args, 0, f'tools/math-port/ams-newcommand-reference/ams-newcommand{suffix}-source.json'))
    native = read(default_path(args, 1, f'build/math-port/ams-newcommand{suffix}-native.json'))
    equal(UPSTREAM_COMMIT, field(field(source, 'upstream'), 'commit'), 'pinned-source provenance')
    require(isinstance(field(native, 'cases'), dict), 'native candidate has no cases object')
    fields = ['result', 'error', 'dynamicMaps', 'packageData']
    equal(sorted(row['name'] for row in source['cases']), sorted(native['cases']), 'native case set')
    for row in source['cases']:
        actual = native['cases'][row['name']]
        for name in fields:
            equal(field(row, name), field(actual, name), row['name'] + '.' + name)
    print(f'AMS/Newcommand Source differential passed: {len(source["cases"])} complete cases; {", ".join(fields)}')
    return 0


def boldsymbol(args, *, native_dump=False):
    source_path = default_path(args, 0, 'tools/math-port/boldsymbol-configmacros-reference/original.json')
    native_path = default_path(args, 1, 'build/math-port/boldsymbol-configmacros-native.json')
    source, native = read(source_path), read(native_path)
    if native_dump:
        require(not truthy(field(native, 'bootError')), f'Native setup failed: {native.get("bootError")}')
        actual = {row['name']: row for row in native['cases']}
        equal(len(source['cases']), len(actual), 'native case count/unique names')
        failures = []
        for row in source['cases']:
            require(row['name'] in actual, 'missing native case ' + row['name'])
            for name in ['result', 'error', 'maps', 'packageData']:
                found = difference(field(row, name), field(actual[row['name']], name), row['name'] + '.' + name)
                if found:
                    failures.append(found)
        emit({'compared': len(source['cases']), 'accepted': not failures, 'failures': failures})
        return int(bool(failures))
    equal('7a1683ebd2385cda44d273082658e54aaa437a6a78a187579aac60be120e969c', digest(source_path), 'Source corpus hash')
    equal(['cases'], keys(native), 'native top-level schema')
    equal('4.1.3', source['upstream']['version'], 'Source version')
    equal(UPSTREAM_COMMIT, source['upstream']['commit'], 'Source commit')
    equal([row['name'] for row in source['cases']], [row['name'] for row in native['cases']], 'case coverage/order')
    fields = ['name', 'result', 'error', 'maps', 'packageData']
    failures = []
    for row, actual in zip(source['cases'], native['cases']):
        if sorted(keys(actual)) != sorted(fields) or difference(
                {name: field(row, name) for name in fields}, actual):
            failures.append(row['name'])
    emit({'compared': len(source['cases']), 'matched': len(source['cases']) - len(failures),
          'failures': failures, 'observedFields': fields, 'inputMetadata': ['input', 'packages', 'options']})
    return int(bool(failures))


def six_packages(args):
    source_path = ROOT / 'build/math-port/tex-six-packages-original.json'
    native_path = default_path(args, 0, 'build/math-port/tex-six-packages-native.json')
    source, native = read(source_path), read(native_path)
    failures = []
    if difference(field(source, 'schema'), field(native, 'schema')):
        failures.append({'path': 'schema', 'source': field(source, 'schema'), 'native': field(native, 'schema')})
    expected, actual = field(source, 'cases'), field(native, 'cases')
    matched, total = 0, len(expected) if isinstance(expected, list) else 0
    if not isinstance(expected, list) or not isinstance(actual, list):
        failures.append({'path': 'cases', 'source': isinstance(expected, list), 'native': isinstance(actual, list)})
    else:
        matched, records = records_report(expected, actual)
        failures += records
    output = ROOT / 'build/math-port/tex-six-packages-comparison.json'
    report = {'accepted': not failures, 'total': total, 'matched': matched,
              'source': str(source_path), 'native': str(native_path), 'failures': failures}
    write(output, report, parents=True)
    emit({key: report[key] for key in ['accepted', 'total', 'matched']} | {'firstDiff': failures[0] if failures else None, 'output': str(output)})
    return int(bool(failures))


def next_packages(args, *, seven=False):
    require(bool(args), 'Expected native JSON path [summary JSON] [source JSON]')
    name = 'tex-next-seven' if seven else 'tex-next-packages'
    native_path = root_path(args, 0, '')
    summary_path = root_path(args, 1, f'build/math-port/{name}-comparison.json')
    source_path = root_path(args, 2, f'build/math-port/{name}-original.json')
    scope_path = ROOT / 'docs/math-port/scope.json'
    scope, source, native = read(scope_path), read(source_path), read(native_path)
    if seven:
        equal(23, len(source['cases']), 'Source oracle case count changed')
    equal(digest(scope_path), source['scopeSha256'], 'Source oracle scope hash differs')
    matched, failures = records_report(source['cases'], native.get('cases') or [])
    report = {'source': str(source_path), 'sourceSha256': digest(source_path), 'sourceLocks': len(scope['files']),
              'native': str(native_path), 'nativeSha256': digest(native_path), 'total': len(source['cases']),
              'matched': matched, 'accepted': not failures and matched == len(source['cases']), 'failures': failures}
    write(summary_path, report, parents=True)
    emit({key: report[key] for key in ['total', 'matched', 'accepted']} | {'firstFailure': failures[0] if failures else None, 'summary': str(summary_path)})
    return int(not report['accepted'])
