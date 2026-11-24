"""Compare all 56 locked menclose observations without geometric tolerance."""
from pathlib import Path
import re

from ..common import equal, read, require
from .geometry import source_geometry
from .path import SPACE
from .svg import parse_svg


def parse_args(args):
    root, dump, expected, index = Path.cwd(), None, None, 0
    while index < len(args):
        arg = args[index]
        if arg in ['--repo-root', '--dump', '--expected']:
            index += 1
            require(index < len(args) and bool(args[index]), arg + ' requires a path')
            if arg == '--repo-root':
                root = Path(args[index]).resolve()
            elif arg == '--dump':
                dump = args[index]
            else:
                expected = args[index]
        elif arg.startswith('-'):
            require(False, 'Unknown option ' + arg)
        elif not dump:
            dump = arg
        else:
            require(False, 'Unexpected positional argument ' + arg)
        index += 1
    require(bool(dump), 'Expected --repo-root <repo> --dump <native.tsv> [--expected <rows.json>]')
    return ((root / dump).resolve(),
            (root / (expected or 'tests/math/fixtures/common-source/menclose-geometry-source-expected.json')).resolve())


def compare(dump_path, expected_path):
    expected = read(expected_path)
    raw = re.sub(SPACE + '+$', '', Path(dump_path).read_bytes().decode('utf-8'))
    records = []
    for line in re.split(r'\r?\n', raw):
        first, second = line.find('\t'), line.find('\t', line.find('\t') + 1)
        require(first > 0 and second > first, 'Malformed native dump row: ' + line[:120])
        records.append({'case': line[:first], 'notation': line[first + 1:second], 'svg': line[second + 1:]})
    equal(56, len(records), 'Native probe must emit every notation for both boxes')
    equal(56, len(expected['rows']), 'Locked Source fixture must contain every notation for both boxes')
    for actual, source in zip(records, expected['rows']):
        equal(source['case'], actual['case'])
        equal(source['notation'], actual['notation'])
        equal(source_geometry(source['source_svg']), parse_svg(actual['svg']), actual['case'] + '/' + actual['notation'])
    print(f'exact SVG geometry/transform/paint match for {len(records)} native writer outputs against MathJax 4.1.3')
    return 0


def main(args):
    return compare(*parse_args(args))
