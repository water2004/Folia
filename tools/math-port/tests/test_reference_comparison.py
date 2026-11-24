import copy
import importlib.util
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.reference import compare, geometry


class ReferenceComparisonTests(unittest.TestCase):
    def test_only_declared_geometry_has_tolerance(self):
        expected = {'layout_runs': [{'wrappers': [{'bbox': {'w': 1.0}, 'state': {'variant': 'normal'}}]}],
                    'placements': [{'x': 1.0, 'codepoint': 65}], 'postfilters': [{'priority': 10}]}
        actual = copy.deepcopy(expected)
        actual['layout_runs'][0]['wrappers'][0]['bbox']['w'] += 1e-12
        self.assertEqual(compare(expected, actual), [])
        actual['placements'][0]['codepoint'] += 1e-12
        self.assertEqual(compare(expected, actual)[0]['path'], '$.placements[0].codepoint')
        self.assertTrue(geometry('$.layout_runs[12].wrappers[8].line_bboxes[0].rscale'))
        self.assertFalse(geometry('$.layout_runs[12].wrappers[8].state.scale'))
        self.assertFalse(geometry('$.placements[0].fontIndex'))

    def test_original_eleven_mutations_remain_active(self):
        spec = importlib.util.spec_from_file_location('reference_mutations', Path(__file__).resolve().parents[1] / 'reference-mutations.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        fragment = {'tree': {'kind': 'svg', 'attributes': {}, 'children': [
            {'kind': 'rect', 'attributes': {'height': '10'}, 'children': []},
            {'kind': 'svg', 'attributes': {'viewBox': '0 0 10 10'}, 'children': []}]}}
        snapshot = {'result': {'fragments': [fragment, copy.deepcopy(fragment)]},
                    'layout_runs': [{'wrappers': [{'bbox': {'w': 1.0}, 'state': {'variant': 'normal'}}]}],
                    'placements': [{'method': 'placeChar', 'x': 1.0}],
                    'postfilters': [{'priority': 10.0}], 'diagnostics': [{'id': 'DoubleScript'}],
                    'parsed_mml': [{'kind': 'text', 'text': text,
                                    'attributes': {'explicit': {'names': ['variant'], 'values': {'variant': 'normal'}}}}
                                   for text in ['b', '+', 'a']]}
        names = ['regression_inline_fragments', 'regression_fraction_rule', 'regression_vertical_clip',
                 'regression_group_macro', 'regression_double_script']
        report = module.run({name: copy.deepcopy(snapshot) for name in names})
        self.assertEqual(len(report['mutations']), 11)
        self.assertTrue(report['geometry_tolerance'])
        self.assertTrue(report['discrete_numbers_exact'])

    def test_difference_limit_and_missing_properties(self):
        differences = compare({'x': 1, 'y': [1, 2]}, {'z': 3, 'y': [1]}, limit=1)
        self.assertEqual(differences, [{'path': '$', 'reason': 'own property names differ'}])
        self.assertEqual(compare([], {})[0]['reason'], 'array/object kind differs')
        self.assertEqual(compare(True, 1)[0]['reason'], 'exact value differs')


if __name__ == '__main__':
    unittest.main()
