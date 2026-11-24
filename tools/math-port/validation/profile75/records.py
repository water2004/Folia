"""Complete two-session 75-case SVG records, with exact errors and scene checks."""
import hashlib
import re

from ..common import ComparisonError, equal, field, require

GROUPS = ['continuous', 'fresh-session-per-case']
NATIVE_BOUNDARIES = {
    'continuous': {'isolation': 'shared-owner-across-cases', 'owner': 'one folia.math::Renderer with one NativeRenderSession/Context'},
    'fresh-session-per-case': {'isolation': 'isolated-owner-per-case', 'owner': 'fresh folia.math::Renderer/NativeRenderSession/Context per record'},
}
SOURCE_BOUNDARIES = {
    'continuous': {'isolation': 'shared-owner-across-cases', 'owner': 'one MathJax module realm and one HTMLMathDocument'},
    'fresh-session-per-case': {'isolation': 'isolated-owner-per-case', 'owner': 'one clean process/module realm, TeX/SVG jax pair, and HTMLMathDocument per record'},
}


def default(value, fallback):
    return fallback if value is None else value


def svg_bytes(svg):
    return svg.encode('utf-16-le', errors='surrogatepass').decode('utf-16-le', errors='replace').encode('utf-8')


def svg_hash(svg):
    return None if svg is None else hashlib.sha256(svg_bytes(svg)).hexdigest()


def identity(row, item, group, index, label='record'):
    for key, expected in [('group', group), ('index', index), ('name', item['name']), ('tex', item['tex']),
                          ('display', default(item.get('display'), False)), ('em', default(item.get('em'), 16)),
                          ('ex', default(item.get('ex'), default(item.get('em'), 16) * .5)),
                          ('width', default(item.get('width'), 1280))]:
        equal(expected, field(row, key), label + ': ' + key)


def compare_records(cases, source, native):
    equal(75, len(cases))
    equal(150, len(source['records']))
    equal(150, len(native['records']))
    matched, failures = [], []
    for position in range(150):
        index, group = position % 75, GROUPS[position // 75]
        item, expected, actual = cases[index], source['records'][position], native['records'][position]
        label = f'{group}:{index}:{item["name"]}'
        try:
            for row in [expected, actual]:
                identity(row, item, group, index, label)
                require(field(row, 'svg') is None or isinstance(field(row, 'svg'), str), label + ': SVG type')
                require(isinstance(field(row, 'mmlError'), bool), label + ': MML-error flag')
                error = field(row, 'error')
                require(error is None or isinstance(error, dict) and isinstance(error.get('name'), str)
                        and isinstance(error.get('message'), str), label + ': error shape')
            equal(expected['error'], actual['error'], label + ': exception name/message')
            equal(expected['mmlError'], actual['mmlError'], label + ': MML error')
            if expected['svg'] is None:
                equal(None, actual['svg'], label + ': absent SVG')
                equal(0, field(actual, 'sceneNodes'), label + ': scene should be absent after throw')
            else:
                require(actual['svg'] is not None, label + ': native SVG missing')
                require(svg_bytes(actual['svg']) == svg_bytes(expected['svg']), label + ': complete SVG bytes differ')
                require(re.search(r'<svg\b', actual['svg'], re.ASCII) is not None, label + ': native SVG element')
                require(float(actual['sceneNodes']) > 0, label + ': rendered SVG has no completed Scene nodes')
            matched.append({'group': group, 'index': index, 'name': item['name'], 'svgSha256': svg_hash(expected['svg']),
                            'error': expected['error'], 'mmlError': expected['mmlError']})
        except (ComparisonError, ValueError, TypeError) as error:
            failures.append({'group': group, 'index': index, 'name': item['name'], 'message': str(error),
                             'sourceSvgSha256': svg_hash(expected['svg']), 'nativeSvgSha256': svg_hash(actual['svg'])})
    return matched, failures


def validate_scene_counters(capture):
    equal(150, len(capture['records']), 'Two 75-case groups are required')
    for row in capture['records']:
        label = f'{row["group"]} case {row["index"]}'
        equal(0, field(row, 'sceneOnlyMaterializeCalls'), label + ' entered Serializer::materialize')
        equal(0, field(row, 'sceneOnlySerializeXmlCalls'), label + ' entered LiteAdaptor::serializeXML')
        if row['error'] is None:
            equal(True, field(row, 'sceneEqual'), label + ' changed the produced Scene')
            require(float(row['svgMaterializeCalls']) > 0, label + ' normal SVG consumer did not materialize')
            require(float(row['svgSerializeXmlCalls']) > 0, label + ' normal SVG output did not serialize XML')
        else:
            equal(field(row['error'], 'scene'), field(row['error'], 'svg'), label + ' Scene-only/SVG route errors differ')
