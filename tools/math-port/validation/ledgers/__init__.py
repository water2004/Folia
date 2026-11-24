"""Portable ledger validation with optional explicit reviewed snapshot updates."""
import json
import os
import subprocess

from ..common import ROOT, emit, read, require, write
from .comparison import compare, reviewed_copy

CONFIG = ROOT / 'tools/math-port/ledger-profiles.json'


def main(name, args):
    profile = read(CONFIG)[name]
    require(not args or args == ['--freeze-reviewed'], 'Usage: ledger-check.py [--freeze-reviewed]')
    if args:
        require('freeze_status' in profile, f'{name} does not support --freeze-reviewed')
    captured = subprocess.run([os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'),
                               str(ROOT / 'tools/math-port/ledger-source.cjs'), name],
                              cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    require(captured.returncode == 0, captured.stderr.decode("utf-8", errors="replace"))
    # AST line/count fields are integers. Preserve the original JSON formatting
    # for an explicitly requested reviewed snapshot update.
    fresh = json.loads(captured.stdout)
    file = ROOT / profile['accepted']
    if args:
        fresh = reviewed_copy(name, profile, fresh)
        write(file, fresh)
    emit(compare(name, profile, fresh, read(file)))
    return 0
