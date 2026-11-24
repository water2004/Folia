"""Complete host layout records and native checks, including ordered fields."""
import os
from pathlib import Path

from .common import ROOT, MISSING, digest, emit, field, js_type, keys, read, truthy, write
from .ordered import differences

UNSHIFT_KEYS = ['source', 'sourceSha256', 'method', 'inherited', 'sparsePresence', 'order', 'zeroArgTrace', 'partial', 'nullReceiverThrows']
BBOX_KEYS = ['source', 'sourceSha256', 'method', 'item', 'measurements', 'replacementCalls', 'exceptionPropagated']
UNSHIFT_CHECKS = ['ordinary', 'generic', 'nativeView', 'zeroArgResult', 'readOnlyLengthThrows', 'partialWriteThrows']
BBOX_CHECKS = ['sourceSetMetricsExposed', 'nativeMathItemSetMetricsUsed', 'firstCanonical', 'secondCanonical',
               'firstWidthBefore', 'firstWidthAfter', 'liveField', 'replacementForwarded']


def comparison(source_unshift, native_unshift, source_bbox, native_bbox):
    failures = []

    def same_keys(actual, expected, at):
        if not isinstance(actual, dict):
            failures.append({'path': at, 'expected': 'object', 'actual': None if actual is None else js_type(actual)})
            return
        if keys(actual) != expected:
            failures.append({'path': at + '.[key-order]', 'expected': expected, 'actual': keys(actual)})

    def record(label, source, native, fields, checks):
        if not isinstance(source, dict) or not isinstance(native, dict):
            return
        same_keys(source, fields, label + '.source')
        same_keys(native, fields + ['nativeChecks'], label + '.native')
        for name in fields:
            if name not in source:
                failures.append({'path': label + '.source.' + name, 'expected': 'Source field', 'actual': '<absent>'})
            elif name not in native:
                failures.append({'path': label + '.native.' + name, 'expected': source[name], 'actual': '<absent>'})
            else:
                failures.extend(differences(source[name], native[name], label + '.' + name, host=True))
        if 'nativeChecks' not in native:
            failures.append({'path': label + '.nativeChecks', 'expected': 'present', 'actual': '<absent>'})
        else:
            same_keys(native['nativeChecks'], checks, label + '.nativeChecks')
    record('unshift', source_unshift, native_unshift, UNSHIFT_KEYS, UNSHIFT_CHECKS)
    record('getBBox', source_bbox, native_bbox, BBOX_KEYS, BBOX_CHECKS)
    if truthy(field(native_unshift, 'nativeChecks')):
        expected = {'ordinary': ['one', 'two', 'three'], 'generic': ['one', 'head', 'inherited-tail'],
                    'nativeView': {'length': 3, 'values': ['start', 'left', 'right']}, 'zeroArgResult': 2,
                    'readOnlyLengthThrows': True, 'partialWriteThrows': True}
        failures.extend(differences(expected, native_unshift['nativeChecks'], 'unshift.nativeChecks', host=True))
    if truthy(field(native_bbox, 'nativeChecks')):
        measurements = field(source_bbox, 'measurements')
        width = field(measurements[0] if isinstance(measurements, list) and measurements else MISSING, 'w')
        expected = {'sourceSetMetricsExposed': False, 'nativeMathItemSetMetricsUsed': True, 'firstCanonical': True,
                    'secondCanonical': True, 'firstWidthBefore': width,
                    'firstWidthAfter': width + .25 if isinstance(width, (int, float)) else None,
                    'liveField': True, 'replacementForwarded': True}
        failures.extend(differences(expected, native_bbox['nativeChecks'], 'getBBox.nativeChecks', host=True))
    return failures


def main(args):
    if len(args) not in [4, 5]:
        print('Expected source-unshift native-unshift source-bbox native-bbox [report]')
        return 2
    paths = list(map(lambda value: Path(value).resolve(), args[:4]))
    failures, records = [], []
    for label, file in zip(['source.unshift.json', 'native.unshift.json', 'source.getBBox.json', 'native.getBBox.json'], paths):
        try:
            records.append(read(file))
        except (OSError, ValueError) as error:
            failures.append({'path': label, 'error': str(error)})
            records.append(MISSING)
    failures.extend(comparison(*records))
    oracles = [ROOT / 'tools/math-port/drafts/array-unshift-source-oracle.cjs', ROOT / 'tools/math-port/drafts/common-getbbox-source-oracle.cjs']
    hashes = {}
    for file in [*oracles, *paths, ROOT / 'tools/math-port/compare-host-layout-witnesses.py', Path(__file__).resolve()]:
        try:
            hashes[os.path.relpath(file, ROOT)] = digest(file)
        except OSError as error:
            failures.append({'path': 'hash:' + str(file), 'error': str(error)})
    report = {'accepted': not failures, 'comparison': 'strict Source fields including nested key order; exact nativeChecks',
              'sourceOracleScripts': [os.path.relpath(file, ROOT) for file in oracles],
              'records': {'unshift': {'source': str(paths[0]), 'native': str(paths[1])}, 'getBBox': {'source': str(paths[2]), 'native': str(paths[3])}},
              'sha256': hashes, 'failures': failures}
    if len(args) == 5:
        write(Path(args[4]).resolve(), report, parents=True)
    emit(report)
    return int(not report['accepted'])
