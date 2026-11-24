"""Complete texttip SVG bytes tied to the captured executable and build inputs."""
from pathlib import Path
import re

from .common import ROOT, UPSTREAM_COMMIT, digest, emit, equal, read, require, write


def svg_difference(expected, actual):
    # Preserve the original diagnostic offsets and slice lengths in UTF-16 units.
    left = expected.encode('utf-16-le', errors='surrogatepass')
    right = actual.encode('utf-16-le', errors='surrogatepass')
    offset = 0
    while offset < min(len(left), len(right)) and left[offset:offset + 2] == right[offset:offset + 2]:
        offset += 2
    begin, end = max(0, offset - 80), offset + 200
    return {'path': 'svg', 'offset': offset // 2, 'sourceLength': len(left) // 2, 'nativeLength': len(right) // 2,
            'source': left[begin:end].decode('utf-16-le', errors='surrogatepass'),
            'native': right[begin:end].decode('utf-16-le', errors='surrogatepass')}


def main(args):
    require(bool(args), 'Expected native log [report JSON]')
    native_path = Path(args[0])
    report_path = Path(args[1]) if len(args) > 1 else Path(str(native_path) + '.comparison.json')
    source_path = ROOT / 'build/math-port/isolated/scene_layering/texttip-original.json'
    equal('47265558f42212b68733c7bf6f4ba0912ae9cf903fe29369650fef1b0093f083', digest(source_path))
    equal('c3430d67e3b71699ac8108b5f996a48abfa155556afee048e79d8033e726b253',
          digest(ROOT / 'tools/math-port/drafts/scene-layering-texttip-original.cjs'))
    source = read(source_path)
    equal(r'\texttip{x}{tip}', source['tex'])
    equal(UPSTREAM_COMMIT, source['provenance']['upstream']['commit'])
    provenance_path = Path(str(native_path) + '.provenance.json')
    provenance = read(provenance_path)
    equal('text', provenance['format'])
    equal(provenance['sha256'], digest(provenance['executable']))
    equal(provenance['gate'], provenance['build']['gate'])
    for pin in provenance['build'].values():
        if isinstance(pin, (dict, list)):
            equal(pin['sha256'], digest(pin['path']))
    equal([], read(provenance['build']['audit']['path']))
    equal(provenance['stdoutSha256'], digest(str(native_path) + '.stdout'))
    equal(provenance['stderrSha256'], digest(str(native_path) + '.stderr'))
    stderr = Path(str(native_path) + '.stderr').read_bytes().decode('utf-8')
    matches = re.findall(r'^texttip svg: (.*)\r?$', stderr, re.MULTILINE)
    svg = matches[0].removesuffix('\r') if len(matches) == 1 else None
    found = None
    if provenance['exitCode'] != 0 or provenance.get('signal') or provenance.get('timedOut'):
        found = {'path': 'process', 'exitCode': provenance['exitCode'], 'signal': provenance.get('signal'), 'timedOut': bool(provenance.get('timedOut'))}
    elif svg is None:
        found = {'path': 'svg.records', 'source': 1, 'native': len(matches)}
    elif svg != source['svg']:
        found = svg_difference(source['svg'], svg)
    report = {'accepted': found is None, 'gate': provenance['gate'], 'source': str(source_path.relative_to(ROOT)),
              'sourceSha256': digest(source_path), 'native': str(native_path), 'provenanceSha256': digest(provenance_path),
              'compared': 'complete SVG string', 'adaptorTraceCompared': False, 'firstDifference': found}
    write(report_path, report)
    emit(report)
    return int(not report['accepted'])
