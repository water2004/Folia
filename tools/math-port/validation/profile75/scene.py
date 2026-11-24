"""Observe both native Renderer routes and verify all scene-only counters."""
from pathlib import Path
import sys

from ..common import ROOT, emit, equal, read, require, root_path
from ..processes import execute
from .records import validate_scene_counters

FOLDER = ROOT / 'tools/math-port/candidates/profile75-full-svg-g147'


def child(script, args, timeout):
    result, stdout, stderr = execute(sys.executable, [script, *args], timeout=timeout)
    if result['exitCode'] != 0 or result.get('error') or result.get('timedOut'):
        sys.stderr.buffer.write(stdout)
        sys.stderr.buffer.write(stderr)
        return result['exitCode'] if result['exitCode'] is not None and result['exitCode'] != 0 else 1
    return 0


def main(args):
    require(len(args) >= 3, 'Expected public Renderer executable, session probe, build gate [output JSON]')
    renderer, session, gate = Path(args[0]).resolve(), Path(args[1]).resolve(), int(args[2])
    require(0 < gate <= 2**53 - 1, 'Invalid build gate')
    output = root_path(args, 3, 'build/math-port/profile75-scene-measurement-native.json')
    public_capture = ROOT / f'build/math-port/profile75-full-svg-native-g{gate}.json'
    session_capture = ROOT / f'build/math-port/profile75-scene-measurement-session-g{gate}.json'
    annotated_capture = ROOT / f'build/math-port/profile75-full-svg-native-boundary-g{gate}.json'
    comparison = ROOT / f'build/math-port/profile75-full-svg-comparison-g{gate}-scene-measurement.json'
    runner = FOLDER / 'run-profile75-native.py'
    status = child(runner, [renderer, public_capture], 360)
    if status:
        return status
    status = child(FOLDER / 'annotate-scene-measurement-renderer-boundary.py', [public_capture, annotated_capture, str(gate)], 30)
    if status:
        return status
    status = child(ROOT / 'tools/math-port/candidates/profile75-full-svg-clean-source-g150/compare.py',
                   [ROOT / 'build/math-port/profile75-full-svg-source-clean-g150-root.json', annotated_capture, comparison], 360)
    if status:
        return status
    comparison_result = read(comparison)
    equal(150, comparison_result['matched'], 'Every public Renderer SVG record must match clean Source')
    status = child(runner, [session, session_capture], 360)
    if status:
        return status
    capture = read(session_capture)
    validate_scene_counters(capture)
    output.write_bytes(session_capture.read_bytes())
    emit({'publicRendererComparison': str(comparison), 'matchedPublicSvgRecords': comparison_result['matched'],
          'sessionProbeOutput': str(session_capture), 'output': str(output), 'records': len(capture['records']),
          'groups': capture['groups'], 'sceneOnlyMaterializeCalls': 0, 'sceneOnlySerializeXmlCalls': 0,
          'matchedScenes': sum(row['error'] is None and row['sceneEqual'] for row in capture['records'])})
    return 0
