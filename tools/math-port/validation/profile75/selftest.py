"""Original historical comparer CLI success, boundary and corruption probes."""
import copy
from pathlib import Path
import re
import subprocess
import sys

from ..common import ROOT, emit, equal, read, require, write
from .records import NATIVE_BOUNDARIES

FOLDER = ROOT / 'tools/math-port/candidates'


def main(args, *, clean=False):
    require(not args, 'This historical comparator selftest takes no arguments')
    if clean:
        source_path = ROOT / 'build/math-port/profile75-full-svg-source-clean-g150.json'
        scratch = ROOT / 'build/math-port/profile75-full-svg-clean-comparer-g150'
        comparator = FOLDER / 'profile75-full-svg-clean-source-g150/compare.py'
    else:
        source_path = FOLDER / 'profile75-full-svg-g147/profile75-full-svg-source.json'
        scratch = ROOT / 'build/math-port/profile75-full-svg-comparer-test'
        comparator = FOLDER / 'profile75-full-svg-g147/compare-profile75-full-svg.py'
    source = read(source_path)
    equal(150, len(source['records']), 'Capture complete Source records first')
    scratch.mkdir(parents=True, exist_ok=True)
    native_path = scratch / ('native.json' if clean else 'synthetic-native.json')
    comparison_path = scratch / 'comparison.json'
    native = {'schema': 1, 'renderer': 'folia.math::Renderer', 'groups': source['groups'],
              'corpusSha256': source['source']['corpusSha256'], 'caseInventorySha256': source['source']['caseInventorySha256']}
    if clean:
        annotated = read(ROOT / 'build/math-port/profile75-full-svg-native-boundary-g150.json')
        native.update(boundaryEvidence=annotated['boundaryEvidence'], runtimeBoundaries=copy.deepcopy(NATIVE_BOUNDARIES))
    native['records'] = [copy.deepcopy(row) | {'sceneNodes': 0 if row['svg'] is None else 1} for row in source['records']]

    def invoke():
        write(native_path, native)
        return subprocess.run([sys.executable, str(comparator), str(source_path), str(native_path), str(comparison_path)],
                              cwd=ROOT, capture_output=True, text=True)
    positive = invoke()
    equal(0, positive.returncode, positive.stderr or positive.stdout)
    equal(150, read(comparison_path)['matched'])
    if clean:
        native['runtimeBoundaries']['fresh-session-per-case']['owner'] = 'one reused renderer'
        require(invoke().returncode != 0, 'Boundary mismatch must be rejected')
        native['runtimeBoundaries'] = copy.deepcopy(NATIVE_BOUNDARIES)
        index = 0
    else:
        index = next((index for index, row in enumerate(native['records']) if row['svg'] is not None), -1)
        require(index != -1, 'Expected an SVG observation')
    native['records'][index]['svg'] += '<!-- intentional synthetic mismatch -->' if clean else '<!-- deliberate test mismatch -->'
    negative = invoke()
    equal(1, negative.returncode, negative.stderr or negative.stdout)
    failed = read(comparison_path)
    equal(149, failed['matched'])
    equal(1, len(failed['failures']))
    if clean:
        emit({'positiveMatched': 150, 'boundaryMismatchRejected': True, 'svgMismatchMatched': 149, 'parityClaim': False})
    else:
        for key in ['sourceSvgSha256', 'nativeSvgSha256']:
            require(re.fullmatch('[0-9a-f]{64}', failed['failures'][0][key]) is not None, 'Invalid SVG byte fingerprint')
        emit({'positiveMatched': 150, 'negativeMatched': 149, 'mismatchCase': failed['failures'][0]['name'], 'parityClaim': False})
    return 0
