#!/usr/bin/env python3
"""Compare isolated Source captures with state inherited from a continuous run."""
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from validation.cli import run
from validation.common import ROOT, emit, loads, read, require
from validation.packets import same_json
from validation.processes import execute

FOLDER = Path(__file__).resolve().parent


def capture(mode, name):
    result, stdout, stderr = execute(os.environ.get('FOLIA_NODE_EXECUTABLE', 'node'), [FOLDER / 'capture.cjs', mode, name])
    require(result['exitCode'] == 0 and not result.get('timedOut'), stderr.decode('utf-8', errors='replace') or str(result))
    return loads(stdout.decode('utf-8'))


def main(args):
    require(bool(args), 'Expected parent, or single/after-continuous and a case name')
    if args[0] != 'parent':
        require(len(args) == 2, 'A Source case name is required')
        emit(capture(args[0], args[1]))
        return 0
    names = read(FOLDER / 'cases.json')['cases']
    records = read(ROOT / 'build/math-port/profile75-full-svg-source-g149.json')['records']
    rows = []
    signature = lambda row: {key: row[key] for key in ['svg', 'error'] if key in row}
    for name in names:
        isolated, after = capture('single', name), capture('after-continuous', name)
        locked = next(row for row in records if row['group'] == 'fresh-session-per-case' and row['name'] == name)
        continuous = next(row for row in records if row['group'] == 'continuous' and row['name'] == name)
        row = {'name': name, 'isolatedMatchesContinuous': same_json(signature(isolated), signature(continuous)),
               'afterContinuousMatchesLocked': same_json(signature(after), signature(locked)),
               'isolatedError': isolated.get('error'), 'afterContinuousError': after.get('error')}
        for key, record in [('isolatedLength', isolated), ('afterContinuousLength', after), ('continuousLength', continuous)]:
            if isinstance(record.get('svg'), str):
                row[key] = len(record['svg'].encode('utf-16-le', errors='surrogatepass')) // 2
        rows.append(row)
    emit({'rows': rows})
    return 0


if __name__ == '__main__':
    raise SystemExit(run(main))
