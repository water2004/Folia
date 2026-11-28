"""Portable ledger validation with optional explicit reviewed snapshot updates."""
import hashlib
import json
import os
import subprocess

from ..common import ROOT, digest, emit, loads, read, require, write
from .comparison import compare, reviewed_copy

CONFIG = ROOT / 'tools/math-port/ledger-profiles.json'


def accepted_ledger(profile, *, root=ROOT):
    """Load only the active accepted snapshot and verify its exact pinned bytes."""
    file = root / profile['accepted']
    data = file.read_bytes()
    require(hashlib.sha256(data).hexdigest() == profile['accepted_sha256'],
            'Accepted ledger bytes differ from the pinned SHA-256')
    return loads(data)


def freeze_reviewed(name, profile, fresh, profiles, *, root=ROOT, config=CONFIG):
    """Validate a reviewed snapshot before writing the snapshot or its pin."""
    frozen = reviewed_copy(name, profile, fresh)
    summary = compare(name, profile, frozen, frozen, root=root)
    file = root / profile['accepted']
    write(file, frozen)
    profile['accepted_sha256'] = digest(file)
    write(config, profiles)
    return summary


def main(name, args):
    profiles = read(CONFIG)
    profile = profiles[name]
    require(not args or args == ['--freeze-reviewed'], 'Usage: ledger-check.py [--freeze-reviewed]')
    if args:
        require('freeze_status' in profile, f'{name} does not support --freeze-reviewed')
    # Ordinary checks must verify and parse the same pinned bytes. Explicit
    # freeze is the recovery path for an interrupted two-file snapshot update.
    accepted = None if args else accepted_ledger(profile)
    captured = subprocess.run([os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'),
                               str(ROOT / 'tools/math-port/ledger-source.cjs'), name],
                              cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    require(captured.returncode == 0, captured.stderr.decode("utf-8", errors="replace"))
    # AST line/count fields are integers. Preserve the original JSON formatting
    # for an explicitly requested reviewed snapshot update.
    fresh = json.loads(captured.stdout)
    if args:
        summary = freeze_reviewed(name, profile, fresh, profiles)
    else:
        summary = compare(name, profile, fresh, accepted)
    emit(summary)
    return 0
