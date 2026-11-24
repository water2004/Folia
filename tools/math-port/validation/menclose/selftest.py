"""Retain all direct and command-line corruption probes from the JS comparator."""
from pathlib import Path
import re
import subprocess
import sys

from ..common import ROOT, ComparisonError, difference, emit, equal, read, require
from . import compare
from .geometry import source_geometry
from .path import decimal, key, source_path
from .serialize import decimal_text, dump
from .svg import parse_svg


def rejected(function, pattern):
    try:
        function()
    except (ComparisonError, ValueError) as error:
        require(re.search(pattern, str(error), re.IGNORECASE) is not None, 'Unexpected rejection: ' + str(error))
        return
    require(False, 'Expected rejection: ' + pattern)


def primitive_checks():
    prefix = ('<svg style="x" xmlns="http://www.w3.org/2000/svg" width="1" height="1" role="img" focusable="false" '
              'viewBox="0 0 1 1"><g stroke="currentColor" fill="currentColor" stroke-width="0" transform="scale(1,-1)">'
              '<g data-mml-node="mrow"><path d="')
    suffix = '" /></g></g></svg>'
    rejected(lambda: parse_svg(prefix + 'M 0 0 A -1 1 0 0 1 2 2' + suffix), 'radii must be nonnegative')
    rejected(lambda: parse_svg(prefix + 'M 1e999 0 L 1 1' + suffix), 'finite SVG number range')
    for tag, attrs, expected_fill in [
        ('rect', {'x': '0', 'y': '0', 'width': '1', 'height': '1', 'stroke-width': '1'}, 'inherited'),
        ('ellipse', {'rx': '1', 'ry': '1', 'cx': '0', 'cy': '0', 'fill': 'none', 'stroke-width': '1'}, 'none'),
        ('path', {'d': 'M 0 0 L 1 1', 'style': {'stroke-width': '1'}, 'fill': 'red'}, 'red'),
        ('path', {'d': 'M 0 0 L 1 1', 'style': {'stroke-width': '1'}}, 'inherited'),
    ]:
        equal(expected_fill, source_geometry([{'tag': tag, 'attrs': attrs}])[0]['paint']['fill'])
    equal(source_path('M 0 0 l -30.1 408.5 l 67 100.5'), source_path('M 0 0 L -30.1 408.5 L 36.9 509'))
    fixture = read(ROOT / 'tools/math-port/tests/fixtures/menclose-primitives.json')
    svg = fixture['svg']
    parsed = parse_svg(svg)
    equal(source_geometry(fixture['source_svg']), parsed, 'Complete primitive geometry')
    equal(['M', '1/250', '0/1'], parsed[1]['segments'][0], 'Clamped rounded-rectangle radius')
    for original, mutation in [('width="10"', 'width="11"'), ('fill="none"', 'fill="red"'), ('rotate(20,0,0)', 'rotate(21,0,0)')]:
        require(difference(parsed, parse_svg(svg.replace(original, mutation, 1))) is not None, 'Primitive mutation ignored: ' + original)
    for original, mutation, reason in [
        ('<line ', '<line data-extra="x" ', 'unsupported attribute'),
        ('stroke-width: 3;', 'stroke-width: 3; fill: red;', 'only supports'),
        ('x1="1"', 'x1="1e999"', 'finite SVG number range'),
        ('width="10"', 'width="-10"', 'rect width must be nonnegative'),
        ('rx="8"', 'rx="-1"', 'rect rx must be nonnegative'),
        ('rx="3"', 'rx="-3"', 'ellipse rx must be nonnegative'),
    ]:
        rejected(lambda: parse_svg(svg.replace(original, mutation, 1)), reason)


def mutations(baseline):
    first_path = baseline.index('<path ')
    first_end = baseline.index(' />', first_path) + 3
    arc_flags = re.sub(r'(A [^ ]+ [^ ]+ [^ ]+ [01] )1(?= )', lambda match: match[1] + '2', baseline, count=1)
    require(arc_flags != baseline, 'The baseline needs an arc segment')
    return [
        ('mutated-coordinate', re.sub(r'd="M (-?[0-9]+(?:\.[0-9]+)?)',
          lambda match: 'd="M ' + decimal_text(key(decimal(match[1]) + decimal('0.001'))), baseline, count=1)),
        ('tiny-nonzero-coordinate-difference', re.sub(r'd="M [^ ]+', 'd="M 0.000000000000001', baseline, count=1)),
        ('mutated-transform', re.sub(r'rotate\(([^,]+),', 'rotate(99,', baseline, count=1)),
        ('mutated-paint', baseline.replace('stroke-linecap="round"', 'stroke-linecap="butt"', 1)),
        ('extra-nested-root-flip', baseline[:first_path] + '<g data-mml-node="mrow" transform="scale(1,-1)">' +
         baseline[first_path:first_end] + '</g>' + baseline[first_end:]),
        ('duplicate-root-coordinate-group', baseline.replace('</g></svg>',
         '</g><g stroke="currentColor" fill="currentColor" stroke-width="0" transform="scale(1,-1)"></g></svg>', 1)),
        ('mutated-inherited-paint', baseline[:first_path] + '<g fill="none" stroke-linecap="butt">' +
         baseline[first_path:first_end] + '</g>' + baseline[first_end:]),
        ('unknown-path-attribute', baseline.replace('<path ', '<path data-unmodeled="changed" ', 1)),
        ('unmodeled-svg-shape', baseline.replace('</svg>', '<rect x="0" y="0" width="1" height="1" /></svg>', 1)),
        ('unparsed-path-character', baseline.replace(' d="', ' d="@', 1)),
        ('invalid-arc-flag', arc_flags),
        ('wrong-command-arity', baseline.replace('L 3000 1483.5', 'L 3000', 1)),
        ('non-finite-coordinate', re.sub(r'd="M [^ ]+ ', 'd="M 1e999 ', baseline, count=1)),
    ]


def run_selftests(root=ROOT, directory=None):
    root = Path(root).resolve()
    expected_path = root / 'tests/math/fixtures/common-source/menclose-geometry-source-expected.json'
    expected = read(expected_path)
    equal(56, len(expected['rows']))
    directory = Path(directory or root / 'build/math-port/common-source/menclose-comparator-selftest')
    directory.mkdir(parents=True, exist_ok=True)
    primitive_checks()
    baseline = dump(expected['rows'])
    comparator = ROOT / 'tools/math-port/common-menclose-compare-svg.py'
    checks = [('equivalent-baseline', baseline, True)] + [(name, text, False) for name, text in mutations(baseline)]
    results = {}
    for name, text, should_pass in checks:
        file = directory / (name + '.tsv')
        file.write_text(text, encoding='utf-8')
        result = subprocess.run([sys.executable, str(comparator), '--repo-root', str(root), '--expected',
                                 str(expected_path.relative_to(root)), '--dump', str(file)],
                                cwd=root.anchor, capture_output=True, text=True)
        equal(should_pass, result.returncode == 0, f'{name}: status={result.returncode}\n{result.stdout}\n{result.stderr}')
        results[name] = 'accepted expected baseline' if should_pass else 'rejected as expected'
    positional = subprocess.run([sys.executable, str(comparator), str(directory / 'equivalent-baseline.tsv')],
                                cwd=root, capture_output=True, text=True)
    equal(0, positional.returncode, positional.stderr)
    results['positional-cli'] = 'accepted positional baseline'
    results['exact-relative-absolute'] = 'equivalent'
    return results


def main(args):
    require(not args or len(args) == 2 and args[0] == '--repo-root', 'Expected [--repo-root <repository>]')
    report = run_selftests(Path(args[1]) if args else ROOT)
    emit(report)
    return 0
