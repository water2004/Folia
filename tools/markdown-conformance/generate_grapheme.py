"""Generate pinned Unicode grapheme properties and the official boundary corpus.

No build-time network access is required: generated data are committed. Use
--fetch when regenerating without the hash-locked inputs in the build cache.
"""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / 'build/dependencies/markdown-conformance'


def rows(name):
    for line in (CACHE / name).read_text(encoding='utf-8').splitlines():
        value = line.split('#', 1)[0].strip()
        if value:
            yield [field.strip() for field in value.split(';')]


def span(value):
    pieces = value.split('..')
    return int(pieces[0], 16), int(pieces[-1], 16)


def merge(ranges):
    result = []
    for first, last, kind in sorted(ranges):
        if result and result[-1][1] + 1 == first and result[-1][2] == kind:
            result[-1] = result[-1][0], last, kind
        else:
            result.append((first, last, kind))
    return result


def boundary_fixture(cases, version, license_text):
    """Keep the official corpus as runtime data, including embedded NULs."""
    def escaped(points):
        units = ''.join(chr(point) for point in points).encode('utf-16-be')
        return ''.join(f'\\u{int.from_bytes(units[i:i + 2], "big"):04x}'
                       for i in range(0, len(units), 2))

    lines = [f'# Unicode {version} official extended grapheme boundary cases.',
             '# Regenerate: python tools/markdown-conformance/generate_grapheme.py --fetch',
             '# Hashes: tools/markdown-conformance/grapheme-sources.lock.json',
             '# Columns: source_line, escaped UTF-16 text, space-separated boundaries.']
    lines.extend('# ' + line for line in license_text.splitlines())
    lines.extend(f'{line_number}\t{escaped(points)}\t{" ".join(map(str, boundaries))}'
                 for points, boundaries, line_number in cases)
    return '\n'.join(lines) + '\n'


def generate(fetch=False):
    lock = json.loads(Path(__file__).with_name('grapheme-sources.lock.json').read_text())
    for entry in lock['files']:
        path = CACHE / entry['name']
        if not path.exists() and fetch:
            with urllib.request.urlopen(entry['url'], timeout=30) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise RuntimeError('Downloaded Unicode source hash mismatch: ' + entry['name'])
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Missing/changed Unicode source; use --fetch: ' + str(path))

    gcb = [(first, last, row[1]) for row in rows('GraphemeBreakProperty.txt')
           for first, last in [span(row[0])] if row[1] not in ('LV', 'LVT')]
    emoji = [(first, last, '') for row in rows('emoji-data.txt')
             for first, last in [span(row[0])] if row[1] == 'Extended_Pictographic']
    indic = [(first, last, row[2]) for row in rows('DerivedCoreProperties.txt')
             if len(row) >= 3 and row[1] == 'InCB' for first, last in [span(row[0])]]
    license_text = (CACHE / 'Unicode-LICENSE.txt').read_text(encoding='utf-8').replace('*/', '* /')
    header = [f'// Generated Unicode {lock["version"]} extended grapheme data.',
              '// Regenerate: python tools/markdown-conformance/generate_grapheme.py --fetch',
              '// Hashes: tools/markdown-conformance/grapheme-sources.lock.json',
              '/*\n' + license_text + '\n*/']
    output = list(header)
    for name, values, kind in [('break_ranges', gcb, 'Break'), ('indic_ranges', indic, 'Indic')]:
        output.append(f'inline constexpr PropertyRange<{kind}> {name}[] = {{')
        output.extend(f'    {{0x{first:x}, 0x{last:x}, {kind}::{prop}}},'
                      for first, last, prop in merge(values))
        output.append('};')
    output.append('inline constexpr unicode_property_detail::Range pictographic_ranges[] = {')
    output.extend(f'    {{0x{first:x}, 0x{last:x}}},' for first, last, _ in merge(emoji))
    output.append('};\n')
    (ROOT / 'src/markdown/support/grapheme_properties.inc').write_text('\n'.join(output), encoding='utf-8')

    cases = []
    for line_number, line in enumerate((CACHE / 'GraphemeBreakTest.txt').read_text(encoding='utf-8').splitlines(), 1):
        value = line.split('#', 1)[0].strip()
        if not value:
            continue
        points, boundaries = [], []
        for token in value.split():
            if token == '÷':
                boundaries.append(len(points))
            elif token != '×':
                points.append(int(token, 16))
        cases.append((points, boundaries, line_number))
    fixture_path = ROOT / 'tests/fixtures/unicode/grapheme_break.tsv'
    fixture_path.parent.mkdir(parents=True, exist_ok=True)
    fixture_path.write_text(boundary_fixture(cases, lock['version'], license_text), encoding='utf-8')
    print(f'Generated {len(merge(gcb))} grapheme, {len(merge(indic))} Indic and '
          f'{len(merge(emoji))} pictographic ranges; {len(cases)} official cases')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    generate(parser.parse_args().fetch)
