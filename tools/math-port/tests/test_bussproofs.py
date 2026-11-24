import copy
from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.bussproofs import FOLDER, observations
from validation.common import read


class BussproofsComparatorTests(unittest.TestCase):
    def test_original_three_record_positive_and_svg_corruption_probes(self):
        source = read(FOLDER / 'bussproofs-public-render-source.json')
        cases = read(FOLDER / 'bussproofs-public-render-cases.json')['cases']
        native = {'records': [copy.deepcopy(row) | {'sceneNodes': 1} for row in source['records']]}
        matched, failures = observations(source, native, cases)
        self.assertEqual((len(matched), len(failures)), (3, 0))
        native['records'][1]['svg'] += '<!-- mismatch -->'
        matched, failures = observations(source, native, cases)
        self.assertEqual((len(matched), len(failures)), (2, 1))
        self.assertEqual(failures[0]['name'], 'ordinary-after-proof')
        self.assertRegex(failures[0]['sourceSvgSha256'], '^[0-9a-f]{64}$')
        self.assertRegex(failures[0]['nativeSvgSha256'], '^[0-9a-f]{64}$')

    def test_every_source_identity_output_and_scene_check(self):
        source = read(FOLDER / 'bussproofs-public-render-source.json')
        cases = read(FOLDER / 'bussproofs-public-render-cases.json')['cases']
        for index in range(3):
            for key, value in [('name', 'wrong'), ('tex', 'changed'), ('mmlError', True),
                               ('error', {'name': 'Error', 'message': 'changed'}), ('svg', None), ('sceneNodes', 0)]:
                with self.subTest(case=index, field=key):
                    native = {'records': [copy.deepcopy(row) | {'sceneNodes': 1} for row in source['records']]}
                    native['records'][index][key] = value
                    matched, failures = observations(source, native, cases)
                    self.assertEqual((len(matched), len(failures)), (2, 1))


if __name__ == '__main__':
    unittest.main()
