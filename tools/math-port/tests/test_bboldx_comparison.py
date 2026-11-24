import copy
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).resolve().parents[1] / 'regressions/g198/bboldx-text-svg/compare.py'
spec = importlib.util.spec_from_file_location('bboldx_comparison', path)
bboldx = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bboldx)


class BboldxComparisonTests(unittest.TestCase):
    def setUp(self):
        expected = {'group': 'continuous', 'index': 0, 'name': 'glyph', 'tex': r'\bboldx{x}', 'configuration': 'default',
                    'display': False, 'em': 16, 'ex': 8, 'width': 1280, 'svg': '<svg>µ</svg>', 'mmlError': None,
                    'error': None, 'pathInventory': [{'codepoint': '1D465', 'd': 'M0 0L1 1'}]}
        actual = {key: value for key, value in expected.items() if key != 'pathInventory'}
        actual.update(glyphPaths=[{'codepoint': 0x1D465, 'd': 'M0 0L1 1'}], scene=[])
        self.source = {'groups': ['continuous'], 'records': [expected]}
        self.native = {'groups': ['continuous'], 'records': [actual]}

    def test_every_source_field_and_native_glyph_path(self):
        self.assertTrue(bboldx.compare(self.source, self.native)['accepted'])
        for key in ['index', 'name', 'tex', 'configuration', 'display', 'em', 'ex', 'width', 'svg', 'mmlError', 'error']:
            with self.subTest(field=key):
                changed = copy.deepcopy(self.native)
                changed['records'][0][key] = {'changed': True}
                report = bboldx.compare(self.source, changed)
                self.assertFalse(report['accepted'])
                self.assertEqual(report['mismatches'][0]['field'], key)
        for key, value in [('codepoint', 0x1D466), ('d', 'M0 1L1 1')]:
            changed = copy.deepcopy(self.native)
            changed['records'][0]['glyphPaths'][0][key] = value
            self.assertFalse(bboldx.compare(self.source, changed)['accepted'])


if __name__ == '__main__':
    unittest.main()
