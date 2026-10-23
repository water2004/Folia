"""Verify official fixtures and generate unchanged examples as C++ test data."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
DEPENDENCIES = ROOT / 'build/dependencies/markdown-conformance'


def prepare(fetch=False, reference=False):
    lock = json.loads((Path(__file__).with_name('sources.lock.json')).read_text(encoding='utf-8'))
    DEPENDENCIES.mkdir(parents=True, exist_ok=True)
    for entry in lock['files']:
        if entry['name'] != 'spec.json' and not reference:
            continue
        path = DEPENDENCIES / entry['name']
        if not path.exists():
            if not fetch:
                raise RuntimeError('Missing fixture; run python tools/markdown-conformance/prepare.py --fetch')
            data = urllib.request.urlopen(entry['url'], timeout=30).read()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise RuntimeError('Downloaded source hash mismatch: ' + entry['name'])
            path.write_bytes(data)
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Source hash mismatch: ' + entry['name'])
    cases = json.loads((DEPENDENCIES / 'spec.json').read_text(encoding='utf-8'))
    if len(cases) != 652 or [c['example'] for c in cases] != list(range(1, 653)):
        raise RuntimeError('Incomplete or reordered CommonMark 0.31.2 fixtures')
    return cases


def literal(value):
    return '"' + ''.join(f'\\x{byte:02x}' for byte in value.encode('utf-8')) + '"'


def generate(path, cases, label="CommonMark 0.31.2"):
    output = [f'// Generated from hash-verified {label}; do not edit.',
              'static constexpr MarkdownCase markdown_cases[] = {']
    for case in cases:
        values = [case['section'], case['markdown'], case['html']]
        output.append('{' + str(case['example']) + ',' + ','.join(map(literal, values)) + '},')
    output.append('};\n')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(output), encoding='utf-8')


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
