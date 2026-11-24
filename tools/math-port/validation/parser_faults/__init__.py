"""Preflight by default; native fault execution requires an explicit choice."""
import argparse
from pathlib import Path

from ..common import ROOT, require
from .plan import preflight
from .runner import NativeRunner, run_faults


def main(args):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, default=ROOT,
                        help='Checkout containing the unchanged pinned fault plan; an isolated checkout is recommended')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--preflight', action='store_true', help='Validate hashes, method anchors, and witnesses without writes')
    mode.add_argument('--execute', action='store_true', help='Explicitly execute and restore every native semantic mutation')
    parser.add_argument('--build-dir', type=Path, help='Already configured CMake build for the selected checkout')
    parser.add_argument('--target', default='FoliaMathHostTests', help='Native host witness target present in that build')
    parser.add_argument('--executable', type=Path, help='Target executable, for a multi-configuration build layout')
    parser.add_argument('--work-dir', type=Path, help='Logs and exclusive run lock; defaults to <repo>/build/parser-host-faults')
    options = parser.parse_args(args)
    require(not options.execute or options.build_dir is not None,
            '--execute requires an explicit --build-dir; ordinary runs only perform preflight')
    root = options.repo_root.resolve()
    plan, originals, mutations, gold = preflight(root)
    if not options.execute:
        print(f'Validated {len(mutations)} method-scoped anchors, source hashes and native witnesses.')
        return 0
    work = options.work_dir or root / 'build/parser-host-faults'
    native = NativeRunner(root, options.build_dir, work, options.target, options.executable)
    run_faults(plan, originals, mutations, gold, work, native)
    return 0
