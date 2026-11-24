#!/usr/bin/env python3
"""Reject corrupt structured records even when the raw SVG stays unchanged."""
import copy
import re

from validation.common import ROOT, equal, read, require, emit
from validation.reference import compare


def dom_find(node, predicate):
    if predicate(node):
        return node
    for child in node.get('children', []):
        found = dom_find(child, predicate)
        if found is not None:
            return found
    return None


def run(snapshots):
    for name, snapshot in snapshots.items():
        equal([], compare(snapshot, snapshot), name + ' baseline')
    mutations = []

    def mutate(name, case, change, path):
        expected = snapshots[case]
        actual = copy.deepcopy(expected)
        change(actual)
        differences = compare(expected, actual)
        require(bool(differences), name + ' was not detected')
        require(any(re.search(path, difference['path']) for difference in differences),
                name + ' detected the wrong representation: ' + str(differences))
        mutations.append({'name': name, 'case': case, 'detected_at': differences[0]['path']})

    def variant(snapshot):
        wrapper = next(item for run in snapshot['layout_runs'] for item in run['wrappers']
                       if isinstance(item['state'].get('variant'), str))
        wrapper['state']['variant'] = 'incorrect-variant'

    def glyph(snapshot):
        item = next(item for item in snapshot['placements'] if item['method'] == 'placeChar'
                    and isinstance(item.get('x'), (float, int)))
        item['x'] += 0.001

    def rule(snapshot):
        rect = dom_find(snapshot['result']['fragments'][0]['tree'], lambda node: node['kind'] == 'rect')
        require(rect is not None, 'fraction fixture has no rule')
        # SVG attributes are strings; preserve the original numeric mutation.
        value = float(rect['attributes']['height']) + 10
        rect['attributes']['height'] = str(int(value)) if value.is_integer() else str(value)

    def viewport(snapshot):
        root = snapshot['result']['fragments'][0]['tree']
        svg = dom_find(root, lambda node: node['kind'] == 'svg' and node is not root
                       and 'viewBox' in node['attributes'])
        require(svg is not None, 'clip fixture has no nested viewport')
        svg['attributes']['viewBox'] = '0 0 1 1'

    def macro_scope(snapshot):
        texts = [node for node in snapshot['parsed_mml'] if node['kind'] == 'text'
                 and isinstance(node.get('text'), str)]
        equal(['b', '+', 'a'], [node['text'] for node in texts], 'scoped b then restored a')
        texts[-1]['text'] = 'b'

    def attribute(snapshot):
        node = next(node for node in snapshot['parsed_mml'] if node.get('attributes', {}).get('explicit', {}).get('names'))
        explicit = node['attributes']['explicit']
        del explicit['values'][explicit['names'].pop(0)]

    def bbox(snapshot):
        snapshot['layout_runs'][0]['wrappers'][0]['bbox']['w'] += 0.001

    def diagnostic(snapshot):
        snapshot['diagnostics'][0]['id'] = 'IncorrectError'

    def priority(snapshot):
        snapshot['postfilters'][0]['priority'] += 1e-12

    mutate('missing_second_fragment', 'regression_inline_fragments',
           lambda snapshot: snapshot['result']['fragments'].pop(1), r'\.result\.fragments$')
    mutate('wrong_variant', 'regression_inline_fragments', variant, r'\.state\.variant$')
    mutate('shifted_glyph', 'regression_inline_fragments', glyph, r'\.placements\[\d+\]\.x$')
    mutate('wrong_rule_thickness', 'regression_fraction_rule', rule,
           r'\.result\.fragments\[0\]\.tree.*\.attributes\.height$')
    mutate('wrong_nested_viewport_clip', 'regression_vertical_clip', viewport,
           r'\.result\.fragments\[0\]\.tree.*\.attributes\.viewBox$')
    mutate('wrong_macro_scope_result', 'regression_group_macro', macro_scope, r'\.parsed_mml\[\d+\]\.text$')
    mutate('dropped_mml_attribute', 'regression_fraction_rule', attribute, r'\.parsed_mml.*\.attributes\.explicit')
    mutate('wrong_bbox', 'regression_fraction_rule', bbox, r'\.bbox\.w$')
    mutate('dropped_postfilter', 'regression_fraction_rule', lambda snapshot: snapshot['postfilters'].pop(), r'\.postfilters$')
    mutate('wrong_diagnostic', 'regression_double_script', diagnostic, r'\.diagnostics\[0\]\.id$')
    expected = snapshots['regression_fraction_rule']
    near = copy.deepcopy(expected)
    near['layout_runs'][0]['wrappers'][0]['bbox']['w'] += 1e-12
    equal([], compare(expected, near), 'documented geometry tolerance')
    mutate('non_geometry_numbers_exact', 'regression_fraction_rule', priority, r'\.priority$')
    return {'baselines': len(snapshots), 'geometry_tolerance': True, 'discrete_numbers_exact': True, 'mutations': mutations}


if __name__ == '__main__':
    corpus = read(ROOT / 'docs/math-port/reference-corpus.json')
    snapshots = {case['name']: read(ROOT / 'build/math-port-reference' / (case['name'] + '.json.gz'))
                 for case in corpus['cases']}
    emit(run(snapshots))
