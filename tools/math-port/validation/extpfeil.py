"""Diagnose the retained extpfeil capture using the locked upstream observer."""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import sys

from .common import ComparisonError, ROOT, emit, loads, read, require, same_scalar, write
from .processes import execute


def find_text(node: dict | None, kind: str, text: str) -> dict | None:
    if not node:
        return None
    if node.get('kind') == kind and node.get('text') == text:
        return node
    for child in node.get('children', []):
        found = find_text(child, kind, text)
        if found:
            return found
    return None


def diagnose(source: dict, native_bytes: bytes, comparison: dict, provenance: dict,
             native_path: Path, warnings: str = '', *, root: Path = ROOT) -> dict:
    source_case = next((row for row in source['records'] if row['name'] == 'extpfeil-arrows'), None)
    require(source_case is not None, 'Source profile omitted extpfeil-arrows')
    native = loads(native_bytes)
    native_case = next((row for row in native['records'] if row['name'] == 'extpfeil-arrows'), None)
    require(native_case is not None, 'Gate113 native capture omitted extpfeil-arrows')
    native_hash = hashlib.sha256(native_bytes).hexdigest()
    require(native_hash == comparison['nativeSha256'], 'Gate113 native capture hash changed')
    require(isinstance(provenance.get('executableSha256'), str)
            and len(provenance['executableSha256']) == 64, 'Gate113 executable hash is missing')
    require(same_scalar(provenance.get('exitCode'), 0),
            'Gate113 native capture did not complete successfully')
    source_arrow = find_text(source_case['mml'], 'mo', '↦')
    native_arrow = find_text(native_case['mml'], 'mo', '↦')
    require(source_arrow is not None and native_arrow is not None, 'Source/native MML arrow text differs')

    def place_chars(record):
        return [{key: call[key] for key in ('codepoint', 'wrapper', 'variant') if key in call}
                for call in record['calls'] if call['method'] == 'placeChar']

    source_glyphs, native_glyphs = place_chars(source_case), place_chars(native_case)
    source_call = next((row for row in source_glyphs if row['codepoint'] == 0x27FC), None)
    native_call = next((row for row in native_glyphs if row['codepoint'] == 0x21A6), None)
    require(source_call is not None, 'Source did not select the recorded NewCM xmapsto glyph U+27FC')
    require(native_call is not None, 'Native did not place the semantic xmapsto glyph U+21A6')

    def wrapper(record, identity):
        value = next((row for row in record['wrappers'] if row['id'] == identity), None)
        require(value is not None, f'Missing wrapper {identity:g}')
        return {'id': identity, 'kind': value['kind'],
                'width': value['bbox']['w'], 'scale': value['bbox']['scale']}

    return {'schema': 1, 'purpose': 'Diagnose extpfeil source semantics versus common/font rendering.',
            'sourceVersion': source['version'], 'sourceScopeHash': source['scopeHash'],
            'input': source_case['tex'],
            'sourceMmlArrow': {key: source_arrow[key] for key in ('text', 'attributes') if key in source_arrow},
            'nativeMmlArrow': {key: native_arrow[key] for key in ('text', 'attributes') if key in native_arrow},
            'sourcePlaceChars': source_glyphs, 'nativePlaceChars': native_glyphs,
            'selectedGlyphs': {'source': f'U+{int(source_call["codepoint"]):X}',
                               'native': f'U+{int(native_call["codepoint"]):X}'},
            'wrappers': {'sourceMo': wrapper(source_case, source_call['wrapper'] + 4),
                         'nativeMo': wrapper(native_case, native_call['wrapper'] + 4),
                         'sourceMpadded': wrapper(source_case, 15),
                         'nativeMpadded': wrapper(native_case, 15)},
            'sourceDynamicLoads': source_case['loads'],
            'nativeCapture': {'file': os.path.relpath(native_path, root).replace(os.sep, '/'),
                              'sha256': native_hash, **{key: provenance[key] for key in (
                                  'executableSha256', 'startedUtc', 'finishedUtc', 'pid', 'exitCode')
                                  if key in provenance}},
            'sourceWarnings': [line for line in warnings.strip().splitlines() if line]}


def capture_source(*, root: Path = ROOT):
    record, stdout, stderr = execute(os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'),
        [root / 'tools/math-port/common-layout-original-oracle.cjs', '--profile-corpus'],
        cwd=root, timeout=60)
    require(record.get('exitCode') == 0 and not record.get('timedOut') and not record.get('error'),
            stderr.decode('utf-8', errors='replace') or 'Source profile observer failed')
    require(len(stdout) <= 16 * 1024 * 1024 and len(stderr) <= 16 * 1024 * 1024,
            'Source profile observer output exceeded 16 MiB')
    return loads(stdout), stderr.decode('utf-8')


def main() -> int:
    try:
        source, warnings = capture_source()
        build = ROOT / 'build/math-port'
        native = build / 'common-layout-native-g113.json'
        result = diagnose(source, native.read_bytes(), read(build / 'common-layout-comparison-g113.json'),
                          read(str(native) + '.provenance.json'), native, warnings)
        output = build / 'extpfeil-render-diagnostic.json'
        write(output, result)
        emit({'output': os.path.relpath(output, ROOT), 'result': result})
        return 0
    except (OSError, UnicodeError, ValueError, KeyError, ComparisonError) as error:
        print(str(error), file=sys.stderr)
        return 1
