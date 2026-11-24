#!/usr/bin/env python3
"""Check the committed portable fixture checkpoint, including byte/record order."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = 'tests/fixtures/preservation/fixture-integrity.json'
PURPOSE = ('Integrity checkpoint for retained portable observations; hashes detect subsequent changes, '
           'but do not by themselves prove equivalence to pre-refactor data.')
TOOL_FIXTURE_ROOTS = ('tools/math-port/tests/fixtures', 'tools/mermaid-cpp/regression/tests/fixtures')
SCOPE = ['tests/**/fixtures/** excluding SDK suites, preservation, code and README files',
         'tests/mermaid_regression/cases/**',
         *(f'{root}/** excluding code and README files' for root in TOOL_FIXTURE_ROOTS)]
CODE = {'.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inc', '.ixx',
        '.py', '.pyc', '.pyo', '.js', '.cjs', '.mjs', '.ts', '.tsx', '.jsx',
        '.sh', '.bat', '.cmd', '.ps1', '.cmake'}
SKIP_DIRS = {'node_modules', '__pycache__', 'build', '.git', '.reference'}
RAW_UPSTREAM = PurePosixPath('tests/parsing/fixtures/conformance/upstream')


def selected(path: PurePosixPath) -> bool:
    parts = path.parts
    if not parts or any(part in SKIP_DIRS for part in parts):
        return False
    if parts[0] == 'tests':
        if len(parts) < 3 or parts[1] in {'app', 'platform', 'encoding'}:
            return False
        if path.is_relative_to('tests/fixtures/preservation'):
            return False
        in_scope = 'fixtures' in parts[1:-1] or path.is_relative_to('tests/mermaid_regression/cases')
    elif parts[0] == 'tools':
        in_scope = any(path.is_relative_to(root) for root in TOOL_FIXTURE_ROOTS)
    else:
        return False
    if not in_scope or path.name.lower().startswith('readme'):
        return False
    # This original upstream .inc is frozen input, not compiled fixture code.
    return path.is_relative_to(RAW_UPSTREAM) or path.suffix.lower() not in CODE


def safe_path(root: Path, relative: str) -> Path:
    if not isinstance(relative, str) or '\\' in relative or ':' in relative or '\0' in relative:
        raise ValueError('Invalid fixture path')
    path = PurePosixPath(relative)
    if (path.is_absolute() or '..' in path.parts or not path.parts
            or path.as_posix() != relative):
        raise ValueError('Noncanonical or escaping fixture path: ' + relative)
    root = root.resolve()
    result = root.joinpath(*path.parts)
    cursor = root
    for part in path.parts:
        cursor /= part
        if cursor.is_symlink():
            raise ValueError('Symlink is not a portable fixture: ' + relative)
    if not result.resolve().is_relative_to(root):
        raise ValueError('Escaping fixture path: ' + relative)
    return result


def fixture_paths(root: Path) -> list[str]:
    paths = []
    for base in ('tests', *TOOL_FIXTURE_ROOTS):
        for directory, dirs, files in os.walk(root / base, followlinks=False):
            dirs[:] = sorted(name for name in dirs if name not in SKIP_DIRS and not name.startswith('.'))
            parent = Path(directory).relative_to(root)
            for name in dirs:
                child = Path(directory) / name
                if child.is_symlink() and selected(PurePosixPath((parent / name / 'data').as_posix())):
                    raise ValueError('Symlink is not a portable fixture: ' + (parent / name).as_posix())
            for name in files:
                relative = (parent / name).as_posix()
                if not name.startswith('.') and selected(PurePosixPath(relative)):
                    safe_path(root, relative)
                    paths.append(relative)
    return sorted(paths)


def observation(root: Path, relative: str) -> dict:
    data = safe_path(root, relative).read_bytes()
    row = {'path': relative, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    if relative.endswith('.tsv'):
        # Match the shared native reader: LF records, optional CR terminator,
        # blank/comment lines excluded. The hash still includes every raw byte.
        lines = (line.removesuffix(b'\r') for line in data.split(b'\n'))
        row['tsv_rows'] = sum(bool(line) and not line.startswith(b'#') for line in lines)
    return row


def checkpoint(root: Path = ROOT) -> dict:
    paths = fixture_paths(root)
    if not paths:
        raise ValueError('No portable fixtures found')
    return {'schema': 1, 'purpose': PURPOSE, 'hash_policy': 'sha256-exact-bytes',
            'scope': SCOPE, 'files': [observation(root, relative) for relative in paths]}


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError('Duplicate JSON key in fixture inventory: ' + key)
        value[key] = item
    return value


def validate(manifest: dict, root: Path = ROOT) -> dict:
    if (not isinstance(manifest, dict) or set(manifest) != {'schema', 'purpose', 'hash_policy', 'scope', 'files'}
            or type(manifest.get('schema')) is not int or manifest['schema'] != 1
            or manifest['purpose'] != PURPOSE or manifest['hash_policy'] != 'sha256-exact-bytes'
            or manifest['scope'] != SCOPE or not isinstance(manifest['files'], list) or not manifest['files']):
        raise ValueError('Missing or malformed fixture integrity checkpoint')
    previous = ''
    for row in manifest['files']:
        if not isinstance(row, dict) or not isinstance(row.get('path'), str):
            raise ValueError('Malformed fixture entry')
        relative = row['path']
        safe_path(root, relative)
        if not selected(PurePosixPath(relative)) or relative <= previous:
            raise ValueError('Unsorted, duplicate or out-of-scope fixture path: ' + relative)
        previous = relative
        fields = {'path', 'bytes', 'sha256'} | ({'tsv_rows'} if relative.endswith('.tsv') else set())
        if (set(row) != fields or type(row.get('bytes')) is not int or row['bytes'] < 0
                or not isinstance(row.get('sha256'), str) or not re.fullmatch('[0-9a-f]{64}', row['sha256'])
                or ('tsv_rows' in fields and (type(row.get('tsv_rows')) is not int or row['tsv_rows'] < 0))):
            raise ValueError('Malformed fixture observation: ' + relative)
    return manifest


def check(root: Path = ROOT) -> dict:
    manifest = validate(json.loads(safe_path(root, MANIFEST).read_bytes(),
                                   object_pairs_hook=unique_object), root)
    expected = {row['path']: row for row in manifest['files']}
    current = set(fixture_paths(root))
    failures = ['Missing fixture: ' + path for path in sorted(expected.keys() - current)]
    failures += ['Unrecorded fixture: ' + path for path in sorted(current - expected.keys())]
    for path in sorted(current & expected.keys()):
        observed = observation(root, path)
        changed = [key for key in observed if expected[path][key] != observed[key]]
        if changed:
            failures.append('Changed fixture: ' + path + ' (' + ', '.join(changed) + ')')
    return {'recorded_files': len(expected), 'current_files': len(current),
            'recorded_tsv_rows': sum(row.get('tsv_rows', 0) for row in expected.values()),
            'failures': failures, 'success': not failures}


def refresh(root: Path = ROOT) -> dict:
    manifest = checkpoint(root)
    path = safe_path(root, MANIFEST)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, ensure_ascii=True, indent=2) + '\n', encoding='utf-8')
    return check(root)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--refresh', action='store_true', help='Record an intentionally reviewed fixture update')
    args = parser.parse_args(argv)
    try:
        result = refresh() if args.refresh else check()
    except (OSError, ValueError) as error:
        result = {'success': False, 'failures': [str(error)]}
    print(json.dumps(result, indent=2))
    return 0 if result['success'] else 1


if __name__ == '__main__':
    sys.exit(main())
