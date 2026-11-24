"""Portable native corpus execution and immutable capture boundary annotation."""
import base64
import os
from pathlib import Path

from ..common import ROOT, digest, emit, equal, loads, read, require, root_path, truthy, write
from ..processes import execute, number_token, utc_now
from .metadata import CORPUS, INPUT_CAPTURE, INVENTORY_HASH, LOCK, PROBE, inventory
from .records import GROUPS, NATIVE_BOUNDARIES, identity, svg_bytes


def run_native(args):
    require(bool(args), 'Expected native executable [output JSON]')
    program = Path(args[0]).resolve()
    output = root_path(args, 1, 'build/math-port/profile75-full-svg-native.json')
    corpus, capture, cases = inventory()
    equal(read(LOCK)['corpusSha256'], digest(CORPUS), 'Locked profile case corpus changed')
    equal(INVENTORY_HASH, digest(INPUT_CAPTURE), 'Locked 75-case input capture changed')
    def projection(row, captured):
        result = {'name': row['name'], 'tex': row['tex']}
        if truthy(row.get('display')):
            result['display'] = True
        if (row.get('width') != 1280 if captured else 'width' in row):
            result['width'] = row.get('width')
        return result
    equal([projection(row, False) for row in corpus['cases']], [projection(row, True) for row in cases[-len(corpus['cases']):]],
          '75-case input inventory locked 57-case suffix')
    equal(read(ROOT / 'docs/math-port/scope.json')['profile']['tex_packages'], capture['configuredPackages'])
    b64 = lambda value: base64.b64encode(svg_bytes(value)).decode('ascii')
    payload = ''.join('@group\t' + b64(group) + '\n' + '\n'.join('\t'.join([
        str(index), b64(row['name']), b64(row['tex']), '1' if truthy(row.get('display')) else '0',
        number_token(row.get('em'), 16), number_token(row.get('width'), 1280)]) for index, row in enumerate(cases)) + '\n' for group in GROUPS)
    provenance = {'executable': str(program), 'executableSha256': digest(program), 'corpusSha256': digest(CORPUS),
                  'caseInventorySha256': digest(INPUT_CAPTURE), 'startedUtc': utc_now()}
    result, stdout, stderr = execute(program, input_bytes=payload.encode('ascii'), timeout=300)
    provenance.update(finishedUtc=result['endedAt'], pid=result.get('pid'), exitCode=result['exitCode'],
                      signal=result['signal'], error=result.get('error') or ('Native observer timed out' if result.get('timedOut') else None))
    write(str(output) + '.provenance.json', provenance, parents=True)
    Path(str(output) + '.stdout').write_bytes(stdout)
    Path(str(output) + '.stderr').write_bytes(stderr)
    if result['exitCode'] != 0 or provenance['error']:
        emit(provenance)
        return 1
    observation = loads(stdout.decode('utf-8'))
    equal(GROUPS, observation['groups'])
    equal(150, len(observation['records']), 'Two independently labeled 75-case groups')
    for index, row in enumerate(observation['records']):
        identity(row, cases[index % 75], GROUPS[index // 75], index % 75, 'record ' + str(index))
    native = {'schema': 1, 'renderer': observation['renderer'], 'groups': observation['groups'],
              'corpusSha256': digest(CORPUS), 'caseInventorySha256': digest(INPUT_CAPTURE), 'records': observation['records']}
    write(output, native)
    emit({'output': str(output), 'groups': GROUPS, 'caseCount': len(cases), 'records': len(native['records']),
          'svgRecords': sum(row['svg'] is not None for row in native['records']), 'errors': sum(row['error'] is not None for row in native['records']),
          'mmlErrors': sum(truthy(row['mmlError']) for row in native['records']), 'provenance': provenance})
    return 0


def annotate(args, *, scene=False):
    require(not scene or len(args) == 3, 'Expected native capture, output and build gate')
    capture_path = root_path(args, 0, 'build/math-port/profile75-full-svg-native-g149.json')
    output_path = root_path(args, 1, 'build/math-port/profile75-full-svg-native-boundary-g150.json')
    gate = int(args[2]) if len(args) > 2 else 149
    require(0 < gate <= 2**53 - 1, 'Invalid build gate')
    manifest_path = ROOT / f'build/math-port/gate-{gate}-inputs-before.json'
    audit_path = ROOT / f'build/math-port/profile75-full-svg-binary-audit-g{gate}.json'
    native, provenance = read(capture_path), read(str(capture_path) + '.provenance.json')
    captured_probe = next((row for row in read(manifest_path) if (ROOT / row['path']).resolve() == PROBE), None)
    require(captured_probe is not None, 'Native profile probe is absent from build inputs')
    equal(captured_probe['sha256'], digest(PROBE), 'Profile probe differs from compiled input')
    audit = read(audit_path)
    equal(0, audit['exitCode'])
    equal(True, audit['unchanged'])
    equal(provenance['executableSha256'], audit['before'].lower())
    equal(provenance['executableSha256'], audit['after'].lower())
    equal('folia.math::Renderer', native['renderer'])
    equal(GROUPS, native['groups'])
    equal(150, len(native['records']))
    equal(0, provenance['exitCode'])
    equal(provenance['executableSha256'], digest(provenance['executable']), 'Native executable changed since capture')
    if not scene:
        probe = PROBE.read_text(encoding='utf-8')
        require('if(activeGroup=="continuous")continuous=std::make_unique<Renderer>();' in probe, 'Continuous owner construction changed')
        require('else {Renderer fresh;records.push_back(renderRecord(row,fresh));}' in probe, 'Per-case Renderer construction changed')
        equal(provenance['corpusSha256'], native['corpusSha256'])
        equal(provenance['caseInventorySha256'], native['caseInventorySha256'])
    output = native | {'runtimeBoundaries': NATIVE_BOUNDARIES,
                       'boundaryEvidence': {'buildGate': gate, 'nativeCapture': os.path.relpath(capture_path, ROOT).replace(os.sep, '/'),
                         'nativeCaptureSha256': digest(capture_path), 'executable': provenance['executable'],
                         'executableSha256': provenance['executableSha256'], 'probeSource': PROBE.relative_to(ROOT).as_posix(),
                         'probeSourceSha256': digest(PROBE), 'buildInputsSha256': digest(manifest_path), 'binaryAuditSha256': digest(audit_path)}}
    write(output_path, output, parents=True)
    emit({'output': str(output_path), 'records': len(output['records']), 'executableSha256': provenance['executableSha256'],
          'probeSourceSha256': output['boundaryEvidence']['probeSourceSha256']})
    return 0
