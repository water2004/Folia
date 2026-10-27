"""Generate immutable Unicode properties used by Markdown recognition.

Inputs and their license are pinned in sources.lock.json; no platform locale
or dependency on the math engine is involved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / 'build/dependencies/markdown-conformance'


def generate(fetch=False):
    source = json.loads(Path(__file__).with_name('sources.lock.json').read_text(encoding='utf-8'))['unicode']
    for entry in source['files']:
        path = CACHE / entry['name']
        if not path.exists() and fetch:
            data = urllib.request.urlopen(entry['url'], timeout=30).read()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise RuntimeError('Downloaded Unicode source hash mismatch')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Missing/changed Unicode source; use --fetch: ' + str(path))

    punctuation = []
    spaces = []
    for line in (CACHE / 'UnicodeData.txt').read_text(encoding='utf-8').splitlines():
        row = line.split(';')
        codepoint = int(row[0], 16)
        if row[2].startswith(('P', 'S')):
            punctuation.append(codepoint)
        if row[2] == 'Zs':
            spaces.append(codepoint)

    def ranges(values):
        result = []
        for value in values:
            if result and value == result[-1][1] + 1:
                result[-1][1] = value
            else:
                result.append([value, value])
        return result

    folding = {}
    for line in (CACHE / 'CaseFolding.txt').read_text(encoding='utf-8').splitlines():
        value = line.split('#')[0].strip()
        if not value:
            continue
        cp, status, mapping, _ = (piece.strip() for piece in value.split(';'))
        if status in ('C', 'F'):
            folding[int(cp, 16)] = [int(c, 16) for c in mapping.split()]
    assert max(map(len, folding.values())) <= 3

    license_text = (CACHE / 'Unicode-LICENSE.txt').read_text(encoding='utf-8')
    output = [f'// Generated Unicode {source["version"]} P/S, Zs and default full case folding.',
              '// Regenerate: python tools/markdown-conformance/generate_unicode.py --fetch',
              '// Hash-locked Unicode inputs: tools/markdown-conformance/sources.lock.json',
              '/*\n' + license_text.replace('*/', '* /') + '\n*/',
              'struct Range { char32_t first; char32_t last; };']
    for name, values in [('punctuation_or_symbol', punctuation), ('space_separators', spaces)]:
        output.append(f'inline constexpr Range {name}[] = {{')
        output.extend(f'    {{0x{first:x},0x{last:x}}},' for first, last in ranges(values))
        output.append('};')
    output.append('struct Fold { char32_t from; std::array<char32_t,3> to; unsigned char size; };')
    output.append('inline constexpr Fold case_folding[] = {')
    for cp, mapping in sorted(folding.items()):
        points = ','.join(f'0x{x:x}' for x in mapping + [0] * (3 - len(mapping)))
        output.append(f'    {{0x{cp:x},{{{points}}},{len(mapping)}}},')
    output.append('};\n')
    path = ROOT / 'src/markdown/support/unicode_properties.inc'
    path.write_text('\n'.join(output), encoding='utf-8')
    print(f'Generated {len(ranges(punctuation))} punctuation/symbol ranges; {len(folding)} case folds')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    generate(parser.parse_args().fetch)
