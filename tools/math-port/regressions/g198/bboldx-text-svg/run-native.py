#!/usr/bin/env python3
"""Observe every bboldx case with continuous and fresh native sessions."""
import base64
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from validation.common import emit, equal, loads, read, require, truthy
from validation.processes import execute


def main(args):
    require(len(args) == 1, 'Expected FoliaMathBboldxTextSvgProbe executable')
    cases = read(Path(__file__).with_name('cases.json'))['cases']
    groups = ['continuous', 'fresh-session-per-case']
    b64 = lambda value: base64.b64encode(value.encode('utf-8')).decode('ascii')
    payload = ''.join('@group\t' + b64(group) + '\n' + '\n'.join('\t'.join([
        str(index), b64(row['name']), b64(row['tex']), 'bboldx-light' if truthy(row.get('options')) else 'default', '0', '16', '1280'])
        for index, row in enumerate(cases)) + '\n' for group in groups).encode('ascii')
    record, stdout, stderr = execute(Path(args[0]).resolve(), input_bytes=payload, timeout=120)
    require(record['exitCode'] == 0 and not record.get('timedOut'), stderr.decode('utf-8', errors='replace') or str(record))
    result = loads(stdout.decode('utf-8'))
    equal(groups, result['groups'])
    equal(len(cases) * 2, len(result['records']))
    emit(result)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
