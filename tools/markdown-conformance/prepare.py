"""Verify official fixtures and serialize unchanged examples as runtime TSV data."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request
import fixture_table

ROOT = Path(__file__).resolve().parents[2]
DEPENDENCIES = ROOT / 'build/dependencies/markdown-conformance'
BUNDLED = ROOT / 'tests/parsing/fixtures/conformance/upstream/commonmark'


def prepare(fetch=False, reference=False):
    lock = json.loads((Path(__file__).with_name('sources.lock.json')).read_text(encoding='utf-8'))
    DEPENDENCIES.mkdir(parents=True, exist_ok=True)
    for entry in lock['files']:
        if entry['name'] != 'spec.json' and not reference:
            continue
        path = DEPENDENCIES / entry['name']
        if not path.exists() and (BUNDLED / entry['name']).is_file():
            path = BUNDLED / entry['name']
        if not path.exists():
            if not fetch:
                raise RuntimeError('Missing fixture; run python tools/markdown-conformance/prepare.py --fetch')
            data = urllib.request.urlopen(entry['url'], timeout=30).read()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise RuntimeError('Downloaded source hash mismatch: ' + entry['name'])
            path.write_bytes(data)
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Source hash mismatch: ' + entry['name'])
    spec = DEPENDENCIES / 'spec.json'
    if not spec.exists():
        spec = BUNDLED / 'spec.json'
    cases = json.loads(spec.read_bytes())
    if len(cases) != 652 or [c['example'] for c in cases] != list(range(1, 653)):
        raise RuntimeError('Incomplete or reordered CommonMark 0.31.2 fixtures')
    return cases


def generate(path, cases, label="CommonMark 0.31.2"):
    fixture_table.write(path, ([case['example'], case['section'], case['markdown'], case['html']]
                              for case in cases), f'Hash-verified {label}; do not edit.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--reference', action='store_true', help='Also fetch/verify pinned md4c for failure investigation')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    examples = prepare(args.fetch, args.reference)
    if args.output:
        generate(args.output, examples)
    print(f'Verified CommonMark 0.31.2: {len(examples)} official examples')
