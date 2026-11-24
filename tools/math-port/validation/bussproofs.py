"""Pinned public Renderer proof sequences and their comparator selftests."""
import copy
import hashlib
from pathlib import Path
import re
import subprocess
import sys

from .common import ROOT, UPSTREAM_COMMIT, ComparisonError, digest, emit, equal, field, read, require, write

FOLDER = ROOT / 'tools/math-port/candidates/bussproofs-public-render-g143'


def svg_hash(svg):
    return None if svg is None else hashlib.sha256(svg.encode('utf-16-le', errors='surrogatepass')
                                                   .decode('utf-16-le', errors='replace').encode('utf-8')).hexdigest()


def observations(source, native, cases):
    matched, failures = [], []
    for index, item in enumerate(cases):
        expected, actual, name = source['records'][index], native['records'][index], item['name']
        try:
            for row in [expected, actual]:
                equal(name, field(row, 'name'))
                equal(item['tex'], field(row, 'tex'))
                equal(False, field(row, 'mmlError'), name + ': serialized MML error')
                equal(None, field(row, 'error'), name + ': threw')
                require(isinstance(row.get('svg'), str) and re.search(r'<svg\b', row['svg'], re.ASCII) is not None,
                        name + ': SVG element missing')
            require(float(actual['sceneNodes']) > 0, name + ': native completed no Scene nodes')
            fields = ['name', 'tex', 'svg', 'mmlError', 'error']
            equal({key: field(expected, key) for key in fields}, {key: field(actual, key) for key in fields}, name)
            matched.append({'name': name, 'svgSha256': svg_hash(expected['svg'])})
        except (ComparisonError, ValueError, TypeError) as error:
            failures.append({'name': name, 'message': str(error), 'sourceSvgSha256': svg_hash(expected['svg']),
                             'nativeSvgSha256': svg_hash(actual['svg'])})
    return matched, failures


def main(args):
    require(len(args) >= 2, 'Expected source JSON, native JSON [summary JSON]')
    source_path, native_path = Path(args[0]).resolve(), Path(args[1]).resolve()
    destination = Path(args[2] if len(args) > 2 else 'build/math-port/bussproofs-public-render-comparison.json').resolve()
    source, native = read(source_path), read(native_path)
    scope = read(ROOT / 'docs/math-port/scope.json')
    lock_path = ROOT / 'docs/math-port/evidence/bussproofs-source-lock.json'
    lock = read(lock_path)
    corpus_path = FOLDER / 'bussproofs-public-render-cases.json'
    cases = read(corpus_path)['cases']
    equal(3, len(cases), 'Locked proof sequence case count')
    equal(1, source['schema'])
    equal(1, native['schema'])
    equal('4.1.3', source['source']['version'])
    equal(UPSTREAM_COMMIT, source['source']['commit'])
    equal('9705d389538b2fc8bf782feea4372ace639b3727304f2971937bbcfb0042a0d1', source['source']['scopeSha256'])
    equal(scope['profile']['sha256'], source['source']['profileSha256'])
    equal(scope['dependency_lock_sha256'], source['source']['dependencyLockSha256'])
    equal(digest(lock_path), source['source']['bussproofsLockSha256'])
    equal({row['cjs']: row['cjsSha256'] for row in lock['files']}, source['source']['bussproofsCjs'])
    equal(len(cases), len(source['records']))
    equal(len(cases), len(native['records']))
    equal('mathjax-full', native['profile'])
    equal(digest(corpus_path), source['corpusSha256'])
    matched, failures = observations(source, native, cases)
    summary = {'sourceCommit': source['source']['commit'], 'total': len(cases), 'matched': len(matched),
               'accepted': len(matched) == len(cases), 'matchedCases': matched, 'failures': failures,
               'sourceInputSha256': digest(source_path), 'nativeInputSha256': digest(native_path)}
    write(destination, summary, parents=True)
    emit({key: summary[key] for key in ['total', 'matched', 'accepted', 'failures']})
    return int(not summary['accepted'])


def selftest(args):
    require(not args, 'This comparator selftest takes no arguments')
    source_path = FOLDER / 'bussproofs-public-render-source.json'
    source = read(source_path)
    comparator = FOLDER / 'compare-bussproofs-public-render.py'
    scratch = ROOT / 'build/math-port/bussproofs-comparer-tool-test'
    scratch.mkdir(parents=True, exist_ok=True)
    input_path, output_path = scratch / 'synthetic-native.json', scratch / 'comparison.json'
    native = {'schema': 1, 'profile': 'mathjax-full', 'records': [copy.deepcopy(row) | {'sceneNodes': 1} for row in source['records']]}
    write(input_path, native)
    positive = subprocess.run([sys.executable, str(comparator), str(source_path), str(input_path), str(output_path)],
                              cwd=ROOT, capture_output=True, text=True)
    equal(0, positive.returncode, positive.stderr or positive.stdout)
    equal(3, read(output_path)['matched'])
    native['records'][1]['svg'] += '<!-- mismatch -->'
    write(input_path, native)
    negative = subprocess.run([sys.executable, str(comparator), str(source_path), str(input_path), str(output_path)],
                              cwd=ROOT, capture_output=True, text=True)
    equal(1, negative.returncode, negative.stderr or negative.stdout)
    failed = read(output_path)
    equal(2, failed['matched'])
    equal(1, len(failed['failures']))
    equal('ordinary-after-proof', failed['failures'][0]['name'])
    for name in ['sourceSvgSha256', 'nativeSvgSha256']:
        require(re.fullmatch('[0-9a-f]{64}', failed['failures'][0][name]) is not None, 'Invalid SVG byte fingerprint')
    emit({'positiveMatched': 3, 'negativeMatched': 2, 'negativeFailure': failed['failures'][0]['name'], 'parityClaim': False})
    return 0
