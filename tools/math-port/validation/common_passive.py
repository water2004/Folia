"""Strict 75-row Common promotion and passive-event provenance checks."""
from pathlib import Path

from .common import (ROOT, MISSING, UPSTREAM_COMMIT, difference, digest, emit, equal, field, loads, require, same_scalar, truthy, write)

FOLDER = ROOT / 'tools/math-port/regressions/g252'
EVENTS = {'action-toggle': ['click'], 'action-tooltip': ['mouseover', 'mouseout']}


def read(path):
    return loads(Path(path).read_bytes().decode('utf-8').removeprefix('\ufeff'))


def first_diff(left, right, path='$'):
    found = difference(left, right, path)
    if (found and found['path'].endswith('.keys')
            and isinstance(found.get('source'), list) and isinstance(found.get('native'), list)):
        found['path'] = found['path'][:-5] + '.$keys'
    return found


def observations(source, native, old, lock):
    failures, unchanged, matches = [], [], []
    actions = set(lock['actionRows'])
    for index in range(75):
        fresh, prior, actual = source['records'][index], old['records'][index], native['records'][index]
        if fresh['name'] != prior['name'] or fresh['name'] != actual['name']:
            failures.append({'kind': 'record-order', 'index': index, 'source': fresh['name'], 'old': prior['name'], 'native': actual['name']})
        if fresh['name'] not in actions:
            found = first_diff(prior, fresh, f'oldSource.records[{index}]({fresh["name"]})')
            if found:
                failures.append({'kind': 'old-source-preservation'} | found)
            else:
                unchanged.append(fresh['name'])
        found = first_diff(fresh, actual, f'records[{index}]({fresh["name"]})')
        if found:
            failures.append({'kind': 'strict-source-native'} | found)
        else:
            matches.append(fresh['name'])
    for name, types in EVENTS.items():
        row = next((row for row in source['records'] if row['name'] == name), None)
        event = next((row for row in source.get('passiveHost', {}).get('records', []) if row['name'] == name), None)
        if row is None or field(row, 'error') is not None or not isinstance(row.get('svg'), str) or not row.get('wrappers'):
            failures.append({'kind': 'action-not-complete', 'name': name})
        listeners = [field(item, 'type') for item in event['listeners']] if event and isinstance(event.get('listeners'), list) else MISSING
        if event is None or first_diff(types, listeners, f'passiveHost.{name}.listeners'):
            record = {'kind': 'action-listeners', 'name': name, 'expected': types}
            if listeners is not MISSING:
                record['actual'] = listeners
            failures.append(record)
    return unchanged, matches, failures


def validate_metadata(source, native, lock):
    require(same_scalar(source.get('schema'), 1) and source.get('version') == '4.1.3'
            and field(field(source, 'upstream'), 'version') == '4.1.3'
            and field(field(source, 'upstream'), 'commit') == lock['sourceCommit']
            and source.get('scopeHash') == lock['profile']['scopeSha256']
            and source.get('corpusSha256') == lock['corpus']['sha256']
            and isinstance(source.get('records'), list) and len(source['records']) == 75,
            'Fresh Source capture metadata/profile is not the pinned 75-row profile')
    for key, value in [('observerSha256', lock['producer']['sha256']), ('overlaySha256', lock['host']['sha256']),
                       ('oldCaptureSha256', lock['oldSourceCaptureSha256']), ('profileCaptureSha256', lock['host']['upstreamCaptureSha256']),
                       ('listenerCaptureSha256', lock['host']['listenerCaptureSha256'])]:
        equal(value, field(field(source, 'witness'), key), 'Fresh Source witness provenance ' + key)
    equal(lock['profile']['configuredPackages'], len(source['configuredPackages']), 'Source package inventory count')
    for key in ['schema', 'version', 'scopeHash', 'corpusSha256']:
        equal(field(source, key), field(native, key), 'Native global Source/profile metadata ' + key)
    equal(source['configuredPackages'], field(native, 'configuredPackages'), 'Native configured packages')
    require(isinstance(native.get('registeredPackages'), list) and isinstance(native.get('configurationWarnings'), list),
            'Native observer omitted actual package registration state')
    equal(source['configuredPackages'], native['registeredPackages'], 'Native actual Context package registrations')
    equal([], native['configurationWarnings'], 'Native Context configuration warnings')
    require(isinstance(native.get('records'), list) and len(native['records']) == 75, 'Native observation must have 75 records')


def main(args):
    require(len(args) == 3, 'Expected source JSON, native JSON and summary JSON')
    source_path, native_path, summary_path = [(ROOT / item).resolve() for item in args]
    lock_path = FOLDER / 'common-reference-lock.json'
    lock, source, native = read(lock_path), read(source_path), read(native_path)
    equal(1, lock['schema'])
    equal(75, lock['records'])
    equal(UPSTREAM_COMMIT, lock['sourceCommit'])
    pins = [(FOLDER / lock['producer']['path'], lock['producer']['sha256'], 'producer'),
            (FOLDER / lock['host']['path'], lock['host']['sha256'], 'host'),
            (source_path, lock['sourceCaptureSha256'], 'Source capture'),
            (ROOT / lock['baseObserverPath'], lock['baseObserverSha256'], 'base observer'),
            (ROOT / lock['host']['upstreamCapture'], lock['host']['upstreamCaptureSha256'], 'G213 Source capture'),
            (ROOT / lock['host']['listenerCapture'], lock['host']['listenerCaptureSha256'], 'G213 listener capture'),
            (ROOT / lock['sourceRunnerPath'], lock['sourceRunnerSha256'], 'G213 Source runner'),
            (ROOT / lock['profile']['scopePath'], lock['profile']['scopeSha256'], 'scope'),
            (ROOT / lock['profile']['file'], lock['profile']['sha256'], 'profile'),
            (ROOT / lock['corpus']['path'], lock['corpus']['sha256'], 'corpus'),
            (ROOT / lock['corpus']['profileLockPath'], lock['corpus']['profileLockSha256'], 'profile source lock'),
            (ROOT / lock['corpus']['inputCapturePath'], lock['corpus']['inputCaptureSha256'], 'locked profile input'),
            (ROOT / lock['dependencyLockPath'], lock['dependencyLockSha256'], 'dependency package lock'),
            (ROOT / lock['corpus']['inputCapturePath'], lock['oldSourceCaptureSha256'], 'old Common Source capture')]
    for path, expected, label in pins:
        equal(expected, digest(path), label + ' SHA-256 mismatch')
    validate_metadata(source, native, lock)
    unchanged, matches, failures = observations(source, native, read(ROOT / lock['corpus']['inputCapturePath']), lock)
    summary = {'schema': 1, 'sourceCommit': lock['sourceCommit'], 'sourcePath': args[0], 'sourceSha256': digest(source_path),
               'nativePath': args[1], 'nativeSha256': digest(native_path), 'lockSha256': digest(lock_path),
               'profileSha256': lock['profile']['sha256'], 'corpusSha256': lock['corpus']['sha256'],
               'oldSourceSha256': lock['oldSourceCaptureSha256'], 'total': 75,
               'unchangedOldSourceRows': len(unchanged), 'unchangedOldSourceNames': unchanged, 'nativeStrictMatches': len(matches),
               'actualRegisteredPackages': len(native['registeredPackages']), 'configurationWarnings': len(native['configurationWarnings']),
               'actionListenerRows': list(EVENTS), 'accepted': not failures and len(unchanged) == lock['unchangedOldRowsExpected'] and len(matches) == 75,
               'failures': failures}
    write(summary_path, summary, parents=True)
    emit({'accepted': summary['accepted'], 'unchangedOldSourceRows': len(unchanged), 'nativeStrictMatches': len(matches),
          'failures': len(failures), 'summary': args[2]})
    return int(not summary['accepted'])
