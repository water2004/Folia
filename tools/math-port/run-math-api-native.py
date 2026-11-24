#!/usr/bin/env python3
"""Feed every API fixture to the real native observer and preserve provenance."""
import base64
import hashlib
import sys

from validation.common import ROOT, digest, emit, read, require
from validation.processes import capture, number_token


def main(args):
    require(len(args) == 1, 'A completed build number is required')
    gate = int(args[0])
    cases_path = ROOT / 'tools/math-port/math-api-cases.json'
    cases = read(cases_path)['cases']
    b64 = lambda value: base64.b64encode(value.encode('utf-8')).decode('ascii')
    input_bytes = ('\n'.join(' '.join([b64(row['name']), b64(row['tex']), '1' if row['display'] else '0',
                     number_token(row.get('em'), 16), number_token(row.get('width'), 1280)]) for row in cases) + '\n').encode('ascii')
    output = ROOT / f'build/math-port/math-api-native-g{gate}.json'
    record, status = capture(gate, 'FoliaMathApiDump', output, input_bytes=input_bytes,
                            extra={'corpusSha256': digest(cases_path), 'stdinSha256': hashlib.sha256(input_bytes).hexdigest()})
    emit(record)
    return status


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
