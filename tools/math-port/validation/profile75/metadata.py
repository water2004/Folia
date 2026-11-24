"""All original profile, executable, dependency and session-boundary pins."""
from ..common import ROOT, UPSTREAM_COMMIT, digest, equal, field, read, require
import math
from .records import GROUPS, NATIVE_BOUNDARIES, SOURCE_BOUNDARIES

CORPUS = ROOT / 'tools/math-port/common-layout-reference/profile-cases.json'
LOCK = ROOT / 'tools/math-port/common-layout-reference/profile-source-lock.json'
INPUT_CAPTURE = ROOT / 'build/math-port/common-layout-profile-root-locked.json'
INVENTORY_HASH = '9f58d09411512b4682b1f0049e40d6a47844ddffb9794eda52e2700998e0d740'
PROBE = ROOT / 'tools/math-port/candidates/profile75-full-svg-g147/profile75-full-svg-native-candidate.cpp'


def inventory():
    corpus, capture = read(CORPUS), read(INPUT_CAPTURE)
    equal(1, corpus['schema'])
    equal(57, len(corpus['cases']))
    equal(75, len(capture['records']))
    cases = [{key: row.get(key) for key in ['name', 'tex', 'display', 'em', 'ex', 'width']} for row in capture['records']]
    require(all(row['ex'] == row['em'] * .5 for row in cases), 'Locked Renderer profile ex must equal em/2')
    return corpus, capture, cases


def validate(source, native):
    _, _, cases = inventory()
    scope_path = ROOT / 'docs/math-port/scope.json'
    scope, lock = read(scope_path), read(LOCK)
    equal(1, source['schema'])
    equal(1, native['schema'])
    metadata = source['source']
    equal('4.1.3', metadata['version'])
    equal(UPSTREAM_COMMIT, metadata['commit'])
    equal(digest(scope_path), metadata['scopeSha256'])
    equal(scope['profile']['sha256'], metadata['profileSha256'])
    equal(digest(ROOT / scope['profile']['file']), metadata['profileSha256'])
    equal(digest(LOCK), metadata['sourceLockSha256'])
    equal(digest(CORPUS), metadata['corpusSha256'])
    equal(INVENTORY_HASH, metadata['caseInventorySha256'])
    equal(digest(INPUT_CAPTURE), metadata['caseInventorySha256'])
    equal(scope['dependency_lock_sha256'], metadata['dependencyLockSha256'])
    equal(metadata['dependencyLockSha256'], digest(ROOT / 'tools/mathjax-bundle/package-lock.json'))
    equal(lock['dependencies'], metadata['dependencies'])
    for dependency in lock['dependencies']:
        equal(dependency['sha256'], digest(ROOT / 'tools/mathjax-bundle/node_modules/@mathjax' / dependency['file']), dependency['file'])
    equal(scope['profile']['tex_packages'], metadata['configuredPackages'])
    equal(GROUPS, source['groups'])
    equal(GROUPS, native['groups'])
    equal('folia.math::Renderer', native['renderer'])
    equal(digest(CORPUS), native['corpusSha256'])
    equal(digest(INPUT_CAPTURE), native['caseInventorySha256'])
    equal(150, len(source['records']))
    equal(150, len(native['records']))
    return cases


def validate_boundaries(source, native):
    original_source_path = ROOT / 'build/math-port/profile75-full-svg-source-g149.json'
    evidence = native['boundaryEvidence']
    gate = evidence.get('buildGate') if evidence.get('buildGate') is not None else 149
    require(isinstance(gate, (int, float)) and not isinstance(gate, bool) and math.isfinite(gate)
            and gate == int(gate) and 0 < gate <= 2**53 - 1, 'Invalid native build gate')
    gate = int(gate)
    original_native_path = ROOT / f'build/math-port/profile75-full-svg-native-g{gate}.json'
    worker = ROOT / 'tools/math-port/candidates/profile75-full-svg-clean-source-g150/worker.cjs'
    method = source['source']['captureMethod']
    equal('build/math-port/profile75-full-svg-source-g149.json', method['continuousInput'])
    equal(75, method['cleanProcesses'])
    equal('tools/math-port/candidates/profile75-full-svg-clean-source-g150/worker.cjs', method['freshWorker'])
    equal(digest(worker), method['freshWorkerSha256'])
    equal(digest(original_source_path), method['originalG149Sha256'])
    equal(read(original_source_path)['records'][:75], source['records'][:75], 'Untouched continuous G149 observations')
    equal(f'build/math-port/profile75-full-svg-native-g{gate}.json', evidence['nativeCapture'])
    equal(digest(original_native_path), evidence['nativeCaptureSha256'])
    equal('tools/math-port/candidates/profile75-full-svg-g147/profile75-full-svg-native-candidate.cpp', evidence['probeSource'])
    equal(digest(PROBE), evidence['probeSourceSha256'])
    provenance = read(str(original_native_path) + '.provenance.json')
    equal(provenance['executableSha256'], evidence['executableSha256'])
    equal(provenance['executableSha256'], digest(provenance['executable']))
    equal(0, provenance['exitCode'])
    if 'buildGate' in evidence:
        equal(digest(ROOT / f'build/math-port/gate-{gate}-inputs-before.json'), evidence['buildInputsSha256'])
        equal(digest(ROOT / f'build/math-port/profile75-full-svg-binary-audit-g{gate}.json'), evidence['binaryAuditSha256'])
    validate_owner_boundaries(source, native)


def validate_owner_boundaries(source, native):
    equal(SOURCE_BOUNDARIES, source['source']['runtimeBoundaries'], 'Source capture module/process boundaries')
    equal(NATIVE_BOUNDARIES, native['runtimeBoundaries'], 'Native capture Renderer/Context boundaries')
    for group in GROUPS:
        equal(source['source']['runtimeBoundaries'][group]['isolation'], native['runtimeBoundaries'][group]['isolation'], group)
