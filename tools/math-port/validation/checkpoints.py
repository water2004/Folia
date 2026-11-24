"""Run independently pinned parser and Unicode corpora through native observers."""
import hashlib
import os
from pathlib import Path
import re

from .common import ROOT, MISSING, digest, difference, emit, equal, field, loads, read, require, truthy, write
from .processes import executable, execute

PARSERS = [
    {'name': 'base', 'exe': 'FoliaMathBaseMethodsDump', 'file': True,
     'source': 'build/math-port/base-methods-original.json', 'base': True},
    {'name': 'frontend', 'exe': 'FoliaMathFrontendDump', 'file': True,
     'source': 'docs/math-port/evidence/textmacros-native-tree-source.json', 'tree': True},
    {'name': 'bold', 'exe': 'FoliaMathBoldsymbolConfigmacrosDump', 'file': False,
     'source': 'tools/math-port/boldsymbol-configmacros-reference/original.json',
     'fields': ['name', 'result', 'error', 'maps', 'packageData']},
    {'name': 'ams', 'exe': 'FoliaMathAmsNewcommandDump', 'file': True,
     'source': 'tools/math-port/ams-newcommand-reference/ams-newcommand-source.json',
     'fields': ['result', 'error', 'dynamicMaps', 'packageData']},
    {'name': 'ams-boundary', 'exe': 'FoliaMathAmsNewcommandBoundaryDump', 'file': True,
     'source': 'tools/math-port/ams-newcommand-reference/ams-newcommand-boundary-source.json',
     'fields': ['result', 'error', 'dynamicMaps', 'packageData']},
]


def label(args):
    require(bool(args) and re.fullmatch('[a-zA-Z0-9-]+', args[0]) is not None, 'Supply a checkpoint label')
    return args[0]


def parser_comparison(spec, source, native):
    if spec.get('tree'):
        equal(source['schema'], field(native, 'schema'))
    rows = source['cases'] if isinstance(source['cases'], list) else [
        {'name': name, 'value': value} for name, value in source['cases'].items()]
    actuals = ({row['name']: row for row in native['cases']}
               if isinstance(native['cases'], list) else native['cases'])
    equal(sorted(row['name'] for row in rows), sorted(actuals), spec['name'] + ' coverage differs')
    if spec.get('base'):
        equal(source['methodNames'], field(native, 'methodNames'))
    failures = []
    for row in rows:
        actual = actuals[row['name']]
        fields = spec.get('fields', ['value'])
        if spec.get('base'):
            fields = ['packages', 'error'] + ([] if truthy(field(row, 'error')) else ['tree', 'parser'])
        differences = []
        for name in fields:
            if spec.get('tree'):
                expected_value, actual_value = row['value'], actual
            elif spec.get('base') and name in ['tree', 'parser']:
                expected_value, actual_value = field(field(row, 'result'), name), field(field(actual, 'result'), name)
            else:
                expected_value, actual_value = field(row, name), field(actual, name)
            if difference(expected_value, actual_value) is not None:
                differences.append(name)
        if differences:
            failures.append({'name': row['name'], 'fields': differences,
                             'error': field(actual, 'error') if truthy(field(actual, 'error')) else None})
    return {'total': len(rows), 'matched': len(rows) - len(failures), 'failures': failures}


def parser_checkpoint(args):
    name, requested = label(args), args[1:]
    require(len(set(requested)) == len(requested), 'Duplicate selection')
    require(all(item in [spec['name'] for spec in PARSERS] for item in requested), 'Unknown selection')
    destination = ROOT / f'build/math-port/parser-checkpoint-{name}'
    destination.mkdir(parents=True, exist_ok=True)
    results = []
    for spec in PARSERS:
        if requested and spec['name'] not in requested:
            continue
        program = executable(spec['exe'])
        native_path = destination / (spec['name'] + '.json')
        record = {'name': spec['name'], 'executable': str(program), 'executableSha256': digest(program),
                  'source': spec['source'], 'sourceSha256': digest(ROOT / spec['source'])}
        result, stdout, stderr = execute(program, [native_path] if spec['file'] else [], timeout=30)
        (destination / (spec['name'] + '.stderr')).write_bytes(stderr)
        (destination / (spec['name'] + '.stdout') if spec['file'] else native_path).write_bytes(stdout)
        record['exitCode'] = result['exitCode']
        if result.get('error') or result['exitCode'] != 0 or result.get('timedOut'):
            record['executionError'] = result.get('error') or stderr.decode('utf-8', errors='replace') or str(result)
        else:
            record.update(parser_comparison(spec, read(ROOT / spec['source']), read(native_path)))
        results.append(record)
    write(destination / 'summary.json', results)
    emit(results)
    return int(any(row.get('executionError') or row.get('failures') for row in results))


def unicode_comparison(source, native):
    equal(len(source['records']), len(native['records']), 'Native row count')
    names = [row['name'] for row in native['records']]
    equal(len(names), len(set(names)), 'Duplicate native names')
    equal([row['name'] for row in source['records']], names, 'Whole corpus in original order')
    return [{'name': expected['name'], 'source': expected, 'native': actual}
            for expected, actual in zip(source['records'], native['records'])
            if difference(expected, actual) is not None]


def unicode_checkpoint(args):
    name = label(args)
    # Node is used only for the independent official ECMAScript oracle.
    oracle_script = ROOT / 'tools/math-port/unicode-regexp-original-oracle.cjs'
    original, stdout, stderr = execute(os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'), [oracle_script])
    require(original['exitCode'] == 0, stderr.decode('utf-8', errors='replace') or 'Original capture failed')
    equal('e6c01910ecb588816a0b1c99f7746b7a9b21bd8abeb1cd84f93a3b7163e5d634',
          hashlib.sha256(stdout).hexdigest(), 'Original observation changed; review source/host provenance')
    source_file = ROOT / 'build/math-port/unicode-regexp-original-root.json'
    source_file.parent.mkdir(parents=True, exist_ok=True)
    source_file.write_bytes(stdout)
    source = loads(stdout.decode('utf-8'))
    program = executable('FoliaMathUnicodeRegexpDump')
    destination = ROOT / f'build/math-port/unicode-checkpoint-{name}'
    destination.mkdir(parents=True, exist_ok=True)
    summary = {'executable': str(program), 'executableSha256': digest(program),
               'source': str(source_file.relative_to(ROOT)), 'sourceSha256': digest(source_file),
               'sourceHost': {key: source[key] for key in ['node', 'v8', 'unicode']},
               'total': len(source['records']), 'matched': 0, 'failures': []}
    result, stdout, stderr = execute(program)
    (destination / 'native.json').write_bytes(stdout)
    (destination / 'native.stderr').write_bytes(stderr)
    summary['exitCode'] = result['exitCode']
    if result.get('error') or result['exitCode'] != 0 or result.get('timedOut'):
        summary['executionError'] = result.get('error') or str(result)
    else:
        summary['failures'] = unicode_comparison(source, loads(stdout.decode('utf-8')))
        summary['matched'] = summary['total'] - len(summary['failures'])
    write(destination / 'summary.json', summary)
    emit({'checkpoint': name, 'exitCode': summary['exitCode'], 'matched': summary['matched'], 'total': summary['total'],
          'failedNames': [row['name'] for row in summary['failures']], 'executionError': summary.get('executionError'),
          'summary': str((destination / 'summary.json').relative_to(ROOT))})
    return int(bool(summary.get('executionError') or summary['failures']))
