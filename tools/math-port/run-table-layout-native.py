#!/usr/bin/env python3
"""Run the full focused table corpus through an explicitly supplied native host."""
import base64
from pathlib import Path
import sys

from validation.common import ROOT, digest, emit, equal, loads, read, require, root_path, write
from validation.processes import execute, number_token, utc_now


def main(args):
    require(bool(args), 'Expected native executable [output JSON]')
    program = Path(args[0]).resolve()
    cases_path = ROOT / 'tools/math-port/table-layout-cases.json'
    cases, scope = read(cases_path)['cases'], read(ROOT / 'docs/math-port/scope.json')
    b64 = lambda value: base64.b64encode(value.encode('utf-8')).decode('ascii')
    payload = '@packages\t' + '\t'.join(map(b64, scope['profile']['tex_packages'])) + '\n'
    payload += '\n'.join('\t'.join([b64(row['name']), b64(row['tex']), '1' if row['display'] else '0',
        number_token(row.get('width'), 1280), number_token(row.get('em'), 16), number_token(row.get('ex'), 8)]) for row in cases) + '\n'
    provenance = {'executable': str(program), 'executableSha256': digest(program), 'corpusSha256': digest(cases_path),
                  'startedUtc': utc_now()}
    result, stdout, stderr = execute(program, input_bytes=payload.encode('ascii'), timeout=120)
    provenance.update({'finishedUtc': result['endedAt'], 'pid': result.get('pid'), 'exitCode': result['exitCode'],
                       'signal': result['signal'], 'error': result.get('error')})
    if result.get('timedOut'):
        provenance['error'] = 'Native table observer timed out'
    output = root_path(args, 1, 'build/math-port/table-layout-native-current.json')
    write(str(output) + '.provenance.json', provenance, parents=True)
    Path(str(output) + '.stderr').write_bytes(stderr)
    if result['exitCode'] != 0 or provenance['error']:
        emit(provenance)
        return 1
    native = loads(stdout.decode('utf-8'))
    require(isinstance(native.get('records'), list) and len(native['records']) == len(cases),
            f'Expected {len(cases)} native records')
    for index, (expected, actual) in enumerate(zip(cases, native['records'])):
        equal(expected['name'], actual['name'], f'Native row {index} name')
        equal(expected['tex'], actual['tex'], f'Native row {index} TeX')
    native['tableCorpusSha256'] = provenance['corpusSha256']
    write(output, native)
    emit({'output': str(output), 'caseCount': len(cases), 'provenance': provenance})
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
