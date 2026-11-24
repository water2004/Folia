#!/usr/bin/env python3
"""Compare both native session lifetimes against all six pinned Source scenes."""
import base64
import hashlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from validation.common import ROOT, UPSTREAM_COMMIT, digest, emit, equal, read, require, write
from validation.processes import capture, number_token, pin


def main(args):
    require(len(args) == 2, 'Gate and fresh output path required')
    gate, output = int(args[0]), Path(args[1]).resolve()
    for suffix in ['', '.stdin', '.stdout', '.stderr', '.provenance.json', '.comparison.json']:
        require(not Path(str(output) + suffix).exists(), 'Refusing to overwrite ' + str(output) + suffix)
    source_path = Path(__file__).with_name('source-oracle.json')
    equal('7e33b437f03e4a9240d86fbd25d4957d6bc7d9587208aa7ba544f48302de568e', digest(source_path))
    source = read(source_path)
    equal(UPSTREAM_COMMIT, source['source']['upstream']['commit'])
    equal(6, len(source['records']))
    groups = ['continuous', 'fresh-session-per-case']
    b64 = lambda value: base64.b64encode(value.encode('utf-8')).decode('ascii')
    payload = ''.join('@group\t' + b64(group) + '\n' + '\n'.join('\t'.join([
        str(index), b64(row['name']), b64(row['tex']), '0', '16', number_token(row['width'], 1280)])
        for index, row in enumerate(source['records'])) + '\n' for group in groups).encode('ascii')
    input_path = Path(str(output) + '.stdin')
    input_path.write_bytes(payload)
    _, status = capture(gate, 'FoliaMathProfile75FullSvgDump', output, input_bytes=payload, timeout_ms=120000,
                        extra={'runner': pin(Path(__file__).resolve()), 'stdin': pin(input_path)})
    if status:
        return status
    native = read(output)
    equal(groups, native['groups'])
    equal(12, len(native['records']))
    records = []
    for index, row in enumerate(native['records']):
        expected = source['records'][index % 6]
        identity = (row['group'] == groups[index // 6] and row['index'] == index % 6
                    and row['name'] == expected['name'] and row['tex'] == expected['tex']
                    and row['display'] is False and row['em'] == 16 and row['ex'] == 8 and row['width'] == expected['width'])
        records.append({'group': row['group'], 'name': row['name'], 'identity': identity, 'error': row['error'],
                        'expectedSha256': expected['sha256'],
                        'actualSha256': None if row['svg'] is None else hashlib.sha256(row['svg'].encode('utf-8')).hexdigest(),
                        'equal': identity and row['error'] is None and row['svg'] == expected['svg']})
    accepted = all(row['equal'] for row in records)
    report_path = str(output) + '.comparison.json'
    write(report_path, {'gate': gate, 'sourceSha256': digest(source_path), 'accepted': accepted, 'records': records})
    emit({'gate': gate, 'accepted': accepted, 'matches': sum(row['equal'] for row in records), 'total': 12, 'report': report_path})
    return int(not accepted)


if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))
