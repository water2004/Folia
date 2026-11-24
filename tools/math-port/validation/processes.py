"""Bounded native test execution with byte-exact output and provenance."""
from datetime import datetime, timezone
import os
import math
from pathlib import Path
import signal
import subprocess

from .common import ROOT, digest, equal, read, require, write


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds').replace('+00:00', 'Z')


def executable(name):
    directory = Path(os.environ.get('FOLIA_NATIVE_TEST_DIR', ROOT / 'build/core')).resolve()
    return directory / (name + ('.exe' if os.name == 'nt' else ''))


def pin(path):
    return {'path': str(path), 'sha256': digest(path)}


def number_token(value, default):
    """Keep the fixture number exact when sending a native text protocol."""
    value = default if value is None else value
    require(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value),
            'Native numeric arguments must be finite JSON numbers')
    return str(int(value)) if value == int(value) else repr(value)


def execute(program, args=(), *, input_bytes=b'', timeout=60, cwd=ROOT, env=None):
    """Capture bytes through process completion; UTF-8 is decoded only by callers."""
    record = {'startedAt': utc_now(), 'exitCode': None, 'signal': None}
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    try:
        child = subprocess.Popen([str(program), *map(str, args)], cwd=cwd, env=env,
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **options)
        record['pid'] = child.pid
        try:
            stdout, stderr = child.communicate(input_bytes, timeout=timeout)
        except subprocess.TimeoutExpired:
            record['timedOut'] = True
            child.kill()
            stdout, stderr = child.communicate()
        if child.returncode < 0:
            record['signal'] = signal.Signals(-child.returncode).name
        else:
            record['exitCode'] = child.returncode
    except OSError as error:
        record['error'] = str(error)
        stdout = stderr = b''
    record['endedAt'] = utc_now()
    return record, stdout, stderr


def build_gate(gate):
    require(isinstance(gate, int) and 0 < gate <= 2**53 - 1, 'A completed build number is required')
    state = read(ROOT / 'build/math-port/shared-build-state.json')
    require(state.get('gate') == gate and state.get('status') == 'terminal'
            and state.get('exitCode') == 0 and state.get('changedInputs') == 0,
            'A successful unchanged shared build is required')
    inputs_path = ROOT / f'build/math-port/gate-{gate}-inputs-before.json'
    audit_path = ROOT / f'build/math-port/gate-{gate}-input-changes.json'
    inputs = read(inputs_path)

    def audit():
        for item in inputs:
            equal(item['sha256'], digest(ROOT / item['path']), 'Build input changed: ' + item['path'])

    audit()
    return {'gate': gate, 'inputs': pin(inputs_path), 'audit': pin(audit_path),
            'log': pin(ROOT / state['log'])}, audit


def capture(gate, target, output, args=(), *, input_bytes=b'', timeout_ms=60000, output_format='json', extra=None):
    require(output_format in ['json', 'text'], 'Observation format must be json or text')
    require(isinstance(timeout_ms, int) and timeout_ms > 0, 'Positive observation timeout required')
    build, audit = build_gate(gate)
    program = executable(target)
    record = {'gate': gate, 'executable': str(program), 'args': list(args), 'sha256': digest(program),
              'build': build, 'format': output_format, 'timeoutMs': timeout_ms,
              'outputCapture': pin(Path(__file__).resolve())}
    record.update(extra or {})
    process, stdout, stderr = execute(program, args, input_bytes=input_bytes, timeout=timeout_ms / 1000)
    record.update(process)
    try:
        record['sha256After'] = digest(program)
    except OSError as error:
        record['executableHashError'] = str(error)
    record['executableUnchanged'] = record.get('sha256After') == record['sha256']
    try:
        audit()
        build['inputsUnchangedAfter'] = True
    except (OSError, RuntimeError) as error:
        build['inputAuditError'] = str(error)
    record.update(stdoutBytes=len(stdout), stderrBytes=len(stderr))
    Path(str(output) + '.stdout').write_bytes(stdout)
    Path(str(output) + '.stderr').write_bytes(stderr)
    record['stdoutSha256'] = digest(str(output) + '.stdout')
    record['stderrSha256'] = digest(str(output) + '.stderr')
    if record['exitCode'] == 0:
        try:
            if output_format == 'json':
                from .common import loads
                write(output, loads(stdout.decode('utf-8')))
            else:
                Path(output).write_bytes(stdout)
            record['outputSha256'] = digest(output)
        except (OSError, UnicodeError, ValueError, RuntimeError) as error:
            record['parseError'] = str(error)
    write(str(output) + '.provenance.json', record)
    accepted = (record['exitCode'] == 0 and not record.get('parseError') and not record.get('error')
                and record['executableUnchanged'] and not build.get('inputAuditError') and not record.get('timedOut'))
    return record, int(not accepted)
