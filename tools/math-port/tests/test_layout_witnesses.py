import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import read
from validation.layout_witnesses import comparison
from validation.ordered import differences
from validation.texttip import svg_difference


class LayoutWitnessTests(unittest.TestCase):
    def test_every_ordered_source_field_and_native_check(self):
        fixture = read(Path(__file__).with_name('fixtures') / 'layout-witnesses.json')
        source, native = fixture['source'], fixture['native']
        self.assertEqual(comparison(source[0], native[0], source[1], native[1]), [])
        for index in range(2):
            for field in source[index]:
                changed = copy.deepcopy(native)
                changed[index][field] = 'changed'
                self.assertTrue(comparison(source[0], changed[0], source[1], changed[1]))
            for field in native[index]['nativeChecks']:
                changed = copy.deepcopy(native)
                changed[index]['nativeChecks'][field] = None
                self.assertTrue(comparison(source[0], changed[0], source[1], changed[1]))
        changed = copy.deepcopy(native)
        changed[1]['measurements'][0]['w'] += 1e-12
        self.assertTrue(comparison(source[0], changed[0], source[1], changed[1]))
        changed[0] = dict(reversed(list(changed[0].items())))
        self.assertTrue(comparison(source[0], changed[0], source[1], changed[1]))

    def test_all_ordered_differences_and_array_identity(self):
        report = differences({'a': [1, False], 'b': 2}, {'b': 3, 'a': [1, 0], 'extra': None})
        self.assertEqual([row['path'] for row in report], ['$.[key-order]', '$.a[1]', '$.b', '$.extra'])
        self.assertTrue(differences([], {}, host=True))
        self.assertTrue(differences([], {}))
        self.assertEqual(len(differences([1, 2], [], host=True)), 3)

    def test_svg_diagnostics_use_original_utf16_units(self):
        report = svg_difference('<svg>🙂a</svg>', '<svg>🙂b</svg>')
        self.assertEqual(report['offset'], 7)
        self.assertEqual(report['sourceLength'], 14)
        self.assertEqual(report['nativeLength'], 14)
        self.assertEqual(report['source'], '<svg>🙂a</svg>')


if __name__ == '__main__':
    unittest.main()
