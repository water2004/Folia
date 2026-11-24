import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ComparisonError, read
from validation.font.comparison import FontComparison
from validation.font.glyph import effective_remaps, source_glyph
from validation.font.outline import outline_from_fragment

FIXTURES = Path(__file__).with_name('fixtures')


class FontComparisonTests(unittest.TestCase):
    def test_independent_catalog_fixture_and_complete_field_mutations(self):
        fixture = read(FIXTURES / 'font-catalog.json')
        source, native = fixture['source'], fixture['native']
        report = FontComparison().compare(source, native)
        self.assertTrue(report['passed'])
        self.assertEqual(report['native_actual_direct_glyph_records'], 6)
        name = next(iter(native['variants']))
        cp = next(iter(native['variants'][name]['glyphs']))
        paths = [
            ['face'], ['source_profile', 'source_dynamic_module_count'],
            ['parameters', next(iter(native['parameters']))],
            ['operator_remap', next(iter(native['operator_remap']))],
            ['delimiters', next(iter(native['delimiters'])), 'direction'],
            ['extension_glyph_counts', next(iter(native['extension_glyph_counts']))],
            ['variants', name, 'cache_id'], ['variants', name, 'inherit'],
            *[['variants', name, 'glyphs', cp, key] for key in ['metrics', 'path_defined', 'composition', 'outline_error']],
            ['variants', name, 'remap'], ['effective_glyphs', name, cp, 'glyph_key'],
            ['effective_glyphs', name, cp, 'svg_resource'], ['native_actual_counts', 'outline_segment_records'],
        ]
        for path in paths:
            with self.subTest(path=path):
                changed = copy.deepcopy(native)
                target = changed
                for part in path[:-1]:
                    target = target[part]
                target[path[-1]] = 'changed'
                self.assertFalse(FontComparison().compare(source, changed)['passed'])

    def test_exact_original_outline_errors_and_composition_edges(self):
        corpus = read(FIXTURES / 'font-helper-edge-cases.json')
        for row in corpus['paths']:
            with self.subTest(path=row['input']):
                self.assertEqual(outline_from_fragment(row['input']), row['expected'])
        for row in corpus['glyphs']:
            with self.subTest(glyph=row['input']):
                if row.get('reject'):
                    with self.assertRaises(ComparisonError):
                        source_glyph(row['input'])
                else:
                    self.assertEqual(source_glyph(row['input']), row['expected'])

    def test_visible_smp_remaps_and_bounded_diagnostics(self):
        self.assertEqual(effective_remaps('normal', {'1': [0, 0, 0, {'smp': 0x1D465}],
                                                    '2': [0, 0, 0, {'smp': False}], '3': [0, 0, 0, {'smp': 1.5}]}),
                         {'1': {'variant': 'normal', 'codepoint': 0x1D465}})
        check = FontComparison()
        check.eq(list(range(150)), [-1] * 150, 'all')
        self.assertEqual(check.compared, 151)
        self.assertEqual(check.mismatch_count, 150)
        self.assertEqual(len(check.failures), 100)
        check.eq(0, False, 'bool-is-not-number')
        self.assertEqual(check.mismatch_count, 151)


if __name__ == '__main__':
    unittest.main()
