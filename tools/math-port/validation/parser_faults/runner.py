"""Explicit native fault execution with portable builds and exact restoration."""
from pathlib import Path
import os
import re

from ..common import ROOT, equal, require, write
from ..processes import execute
from .plan import sha256


def configured_build(root, directory):
    """Prevent a selected checkout from accidentally rebuilding another one."""
    root, directory = Path(root).resolve(), Path(directory).resolve()
    cache = (directory / 'CMakeCache.txt').read_text(encoding='utf-8')
    match = re.search(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$', cache, re.MULTILINE)
    require(match is not None, 'Configured CMake source directory is required')
    source = Path(match[1].rstrip('\r')).resolve()
    require(source == root or source.is_relative_to(root),
            f'CMake build belongs to a different checkout: {source}')
    return directory


class NativeRunner:
    def __init__(self, root, build_dir, work, target, program=None):
        self.root = Path(root).resolve()
        self.build_dir = configured_build(self.root, build_dir)
        self.work, self.target = Path(work), target
        self.program = Path(program).resolve() if program else self.build_dir / (
            target + ('.exe' if os.name == 'nt' else ''))
        require(self.program.is_relative_to(self.build_dir), 'Native executable must be inside the selected build')

    def capture(self, program, args, label, timeout):
        process, stdout, stderr = execute(program, args, cwd=self.root, timeout=timeout)
        (self.work / (label + '.log')).write_bytes(stdout + stderr)
        require(not process.get('error'), f'{label}: {process.get("error")}')
        require(not process.get('timedOut'), f'{label}: timed out after {timeout}s')
        require(not process.get('signal'), f'{label}: abnormal exit: {process.get("signal")}')
        require(len(stdout) + len(stderr) <= 16 * 1024 * 1024, label + ': output exceeds 16 MiB')
        return process['exitCode'], stdout

    def build(self, label):
        # Run the real build graph so Ninja/MSBuild creates transient response
        # files. Do not replay archive commands or depend on a VS installation.
        code, _ = self.capture(os.environ.get('FOLIA_CMAKE_EXECUTABLE', 'cmake'),
                               ['--build', str(self.build_dir), '--target', self.target, '--parallel', '4'],
                               label + '-build', 60)
        equal(0, code, label + ' must compile and link')

    def run(self, pattern, label):
        return self.capture(self.program, [pattern] if pattern else [], label + '-test', 30)


def run_faults(plan, originals, mutations, gold, work, native):
    """Execute one changed body at a time; every started mutation is restored."""
    work = Path(work)
    work.mkdir(parents=True, exist_ok=True)
    lock = work / 'run.lock'
    rows, dirty = [], set()
    # Exclusive creation preserves the original shared-run lock contract.
    with lock.open('x', encoding='ascii') as handle:
        handle.write(str(os.getpid()))
        handle.flush()
        try:
            code, stdout = native.run(None, 'baseline')
            equal(0, code, 'Baseline must pass')
            require(re.search(rb'[1-9]\d* asserts in [1-9]\d* tests', stdout) is not None,
                    'Baseline must execute nonempty native tests and assertions')
            for row in mutations:
                original = originals[row['source']]
                require(original.data == original.path.read_bytes(), row['id'] + ' source changed after preflight')
                try:
                    dirty.add(row['source'])
                    original.path.write_bytes(row['changed'])
                    native.build(row['id'])
                    code, stdout = native.run('*' + row['nativeCase'] + '*', row['id'])
                    equal(1, code, row['id'] + ' must fail assertions rather than crash')
                    require(re.search(rb'FAILED|failed|FAIL', stdout) is not None,
                            row['id'] + ' must report failed assertions')
                    rows.append({'id': row['id'], 'source': row['source'], 'nativeCase': row['nativeCase'],
                                 'compiles': True, 'testExit': code, 'detected': True})
                finally:
                    original.path.write_bytes(original.data)
                    equal(sha256(original.data), sha256(original.path.read_bytes()), row['id'] + ' restoration')
                    dirty.discard(row['source'])
                    native.build(row['id'] + '-restored')
                equal(0, native.run('*' + row['nativeCase'] + '*', row['id'] + '-restored')[0],
                      row['id'] + ' restored witness')
                write(work / 'progress.json', {'completed': len(rows), 'total': len(mutations), 'rows': rows})
                print(f'Detected and restored {row["id"]} ({len(rows)}/{len(mutations)}).')
            equal(0, native.run(None, 'restored-full')[0], 'Restored full host suite')
            for file, data in gold:
                require(data == file.read_bytes(), str(file) + ' unchanged')
            for original in originals.values():
                require(original.data == original.path.read_bytes(), str(original.path) + ' baseline restored')
            report = {'schema': 'folia.math.parser-host-native-faults.v1', 'baseline': plan['baselineSha256'],
                      'rows': rows, 'goldUnchanged': True, 'restoredFullExit': 0}
            write(work / 'results.json', report)
            return report
        finally:
            # Restore only files this process changed, without overwriting
            # unrelated edits to another baseline file in a shared checkout.
            try:
                for source in dirty:
                    original = originals[source]
                    original.path.write_bytes(original.data)
            finally:
                handle.close()
                lock.unlink()
