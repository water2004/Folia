"""Replay pinned MathJax observations; compare every frozen value in Python.

The JavaScript owners only emit observations from the actual upstream classes.
The nine complete source locks and the independent multiscript parent projection
remain unchanged. A stale lock or unavailable upstream dependency is a failure.
"""
from __future__ import annotations

import os
from pathlib import Path
import subprocess

from .common import ROOT, digest, emit, equal, loads, read, require

CONFIG = ROOT / 'tools/math-port/source-profile-checks.json'


def compare_profile(entry, actual, expected, *, parent=None):
    """Preserve the original full-profile or ordered-record comparison contract."""
    if entry['comparison'] == 'records':
        equal(Path(entry['profile']).name, actual['profile'], 'profile')
        # The original record checker compares JSON.stringify for each entire
        # observation: own-key order, array order, and every scalar are exact.
        equal(len(expected['records']), len(actual['records']), 'record count')
        for index, (left, right) in enumerate(zip(expected['records'], actual['records'])):
            equal(left['name'], right['name'], f'records[{index}].name')
            equal(left['observations'], right['observations'],
                  f'records[{index}].observations', ordered=True)
        return {'profile': Path(entry['profile']).name, 'cases': len(actual['records']),
                'observations': sum(len(row['observations']) for row in actual['records']),
                'mode': 'check', 'sha256': entry['sha256']}

    equal(expected, actual, 'profile')
    if 'parent_profile' in entry:
        require(parent is not None, 'Missing independent multiscript parent profile')
        names = {row['name'] for row in parent['scenarios']}
        scenarios = [row for row in actual['scenarios'] if row['name'] in names]
        equal(entry['parent_cases'], len(scenarios), 'parent case count')
        equal(entry['parent_values'], sum(len(row['expected']) for row in scenarios), 'parent value count')
        equal(parent['scenarios'], scenarios, 'parent scenarios')
    if entry['summary'] == 'counts':
        return dict(actual['counts'], mode='check')
    if entry['summary'] == 'mo-order':
        cases = actual['cases']
        return {'profile': Path(entry['profile']).name,
                'cases': len(cases) if isinstance(cases, list) else cases,
                'assertions': actual.get('assertions'), 'mode': 'check'}
    summary = actual['summary']
    return {'cases': summary['cases'], 'sourceExpectations': summary['assertions'],
            'rawInheritanceRows': 240, 'tableDisplayDefaultRows': 160, 'mode': 'check'}


def capture(entry, *, root=ROOT, tools=None, runner=subprocess.run):
    """Read a complete, bounded observation stream from an official generator."""
    profile = root / entry['profile']
    equal(entry['sha256'], digest(profile), f'Frozen Source profile changed: {entry["profile"]}')
    owners = Path(tools or os.environ.get('FOLIA_SOURCE_CHECK_TOOLS', root / 'tools/math-port'))
    result = runner([os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'), str(owners / entry['owner'])],
                    cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60,
                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    require(result.returncode == 0,
            f'{entry["owner"]}: exit {result.returncode}: {result.stderr.decode("utf-8", errors="replace")}')
    # Observation output is larger than the old one-line acceptance report;
    # preserve bounded capture while allowing the complete existing profiles.
    require(len(result.stdout) <= 64 * 1024 * 1024, 'Source observation exceeds 64 MiB')
    return loads(result.stdout.decode('utf-8'))


def replay(entry, *, root=ROOT, tools=None, runner=subprocess.run):
    actual = capture(entry, root=root, tools=tools, runner=runner)
    profile = root / entry['profile']
    parent = read(root / entry['parent_profile']) if 'parent_profile' in entry else None
    return compare_profile(entry, actual, read(profile), parent=parent)


def main(group, args):
    require(not args or args == ['--check'], 'Usage: source-check.py [--check]')
    entries = read(CONFIG)[group]
    for entry in entries:
        emit(replay(entry))
    return 0
