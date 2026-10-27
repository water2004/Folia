"""Generate immutable named-character-reference data from pinned md4c records."""
import hashlib
import json
from pathlib import Path
import re

from prepare import prepare, DEPENDENCIES, ROOT


def generate():
    prepare(fetch=True, reference=True)
    lock = json.loads(Path(__file__).with_name('sources.lock.json').read_text(encoding='utf-8'))
    source_record = next(entry for entry in lock['files'] if entry['name'] == 'entity.c')
    source = (DEPENDENCIES / 'entity.c').read_bytes()
    if hashlib.sha256(source).hexdigest() != source_record['sha256']:
        raise RuntimeError('Changed entity data source')
    rows = re.findall(r'^\s*\{\s*"(&[A-Za-z][A-Za-z0-9]*;)"\s*,\s*\{\s*(\d+)\s*,\s*(\d+)\s*\}\s*\},?\s*$',
                      source.decode('utf-8'), re.MULTILINE)
    if len(rows) != 2125 or rows != sorted(rows) or len({row[0] for row in rows}) != len(rows):
        raise RuntimeError('Incomplete, duplicate or unsorted entity mapping')
    license_text = (DEPENDENCIES / 'md4c-LICENSE.md').read_text(encoding='utf-8')
    lines = ['// Generated named-reference records; no md4c parser is linked into Folia.',
             '// Regenerate: python tools/markdown-conformance/generate_entities.py',
             '// Source: ' + source_record['url'],
             '// SHA256: ' + source_record['sha256'],
             '/*\n' + license_text.replace('*/', '* /') + '\n*/']
    lines.extend(f'{{"{name}",{first}u,{second}u}},' for name, first, second in rows)
    (ROOT / 'src/markdown/support/html_entity_data.inc').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'Generated {len(rows)} named character references')


if __name__ == '__main__':
    generate()
