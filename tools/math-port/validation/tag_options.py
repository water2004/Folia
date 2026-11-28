"""Whole-record checks for pinned TagFormat and SetOptions observations."""
import os

from .common import ROOT, MISSING, difference, digest, emit, equal, field, read, require, root_path, write

FOLDER = ROOT / 'tools/math-port/tag-setoptions-reference'


def compare(args):
    require(bool(args), 'Expected native JSON [source JSON] [summary JSON]')
    native_path = root_path(args, 0, '')
    source_path = root_path(args, 1, 'build/math-port/tag-setoptions-original.json')
    summary_path = root_path(args, 2, 'build/math-port/tag-setoptions-comparison.json')
    lock_path = FOLDER / 'source-lock.json'
    lock, source, native = read(lock_path), read(source_path), read(native_path)
    equal(lock['scopeSha256'], digest(ROOT / 'tools/math-port/profiles/scope.json'), 'Source profile')
    equal(lock['scopeSha256'], source['scopeSha256'])
    equal(digest(lock_path), source['sourceLockSha256'])
    equal(digest(lock_path), native['sourceLockSha256'])
    equal('03e4f1e640234845e2b9854744ea3a8f2c336f66b46649914f5a60692aa744f5', digest(source_path), 'Pinned full Source output')
    if lock.get('oracleOutputSha256'):
        equal(lock['oracleOutputSha256'], digest(source_path))
    for item in lock['sources']:
        equal(item['sha256'], digest(ROOT / '.reference/MathJax-src' / item['file']), item['file'])
        if item.get('cjsFile'):
            equal(item['cjsSha256'], digest(ROOT / item['cjsFile']), item['cjsFile'])
    whole = difference(source, native, ordered=True)
    failures = [whole] if whole else []
    actual = native['cases'] if isinstance(native.get('cases'), list) else []
    matched = sum(difference(expected, actual[index] if index < len(actual) else MISSING, ordered=True) is None
                  for index, expected in enumerate(source['cases']))
    report = {'source': os.path.relpath(source_path, ROOT), 'sourceSha256': digest(source_path),
              'sourceLockSha256': digest(lock_path), 'native': os.path.relpath(native_path, ROOT),
              'nativeSha256': digest(native_path), 'sourceFiles': len(lock['sources']), 'total': len(source['cases']),
              'matched': matched, 'accepted': matched == len(source['cases']) and not failures, 'failures': failures}
    write(summary_path, report, parents=True)
    emit({'total': report['total'], 'matched': matched, 'accepted': report['accepted'],
          'firstFailure': failures[0] if failures else None, 'summary': os.path.relpath(summary_path, ROOT)})
    return int(not report['accepted'])


def default_methods(args):
    source_path = ROOT / 'build/math-port/tag-setoptions-default-methods-original.json'
    native_path = root_path(args, 0, 'build/math-port/tag-setoptions-default-methods-native.json')
    lock = read(FOLDER / 'default-methods-witness-lock.json')
    equal(lock['sourceLockSha256'], digest(FOLDER / lock['sourceLockFile']))
    equal(lock['sourceWitnessSha256'], digest(FOLDER / lock['sourceWitnessFile']))
    equal(lock['sourceOutputSha256'], digest(source_path))
    source, native = read(source_path), read(native_path)
    equal(lock['sourceLockSha256'], source['sourceLockSha256'])
    equal(lock['sourceLockSha256'], native['sourceLockSha256'])
    equal(source, native, 'Native default callback/prototype witness')
    emit({'accepted': True, 'sourceSha256': digest(source_path), 'nativeSha256': digest(native_path),
          'sourceLockSha256': lock['sourceLockSha256']})
    return 0
