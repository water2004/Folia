import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.checkpoints import PARSERS, parser_comparison, unicode_comparison
from validation.common import ROOT, ComparisonError, read
from validation.processes import number_token


class CheckpointTests(unittest.TestCase):
    def test_every_retained_package_checkpoint_case_and_field(self):
        for spec in PARSERS[2:]:
            source = read(ROOT / spec['source'])
            self.assertEqual(parser_comparison(spec, source, source)['failures'], [])
            for name in spec['fields']:
                native = copy.deepcopy(source)
                native['cases'][0][name] = {'mutation': name}
                if name == 'name':
                    with self.assertRaises((ComparisonError, TypeError)):
                        parser_comparison(spec, source, native)
                else:
                    self.assertIn(name, parser_comparison(spec, source, native)['failures'][0]['fields'])

    def test_frontend_tree_and_coverage_are_complete(self):
        spec = PARSERS[1]
        source = read(ROOT / spec['source'])
        self.assertEqual(parser_comparison(spec, source, source)['failures'], [])
        native = copy.deepcopy(source)
        del native['cases'][next(iter(native['cases']))]
        with self.assertRaises(ComparisonError):
            parser_comparison(spec, source, native)
        native = copy.deepcopy(source)
        native['cases'][next(iter(native['cases']))] = {'changed': True}
        self.assertEqual(len(parser_comparison(spec, source, native)['failures']), 1)

    def test_unicode_order_identity_and_full_values(self):
        source = {'records': [{'name': 'zero', 'result': [False, 0]}, {'name': 'letter', 'result': 'µ⁢𝒙中文🙂'}]}
        self.assertEqual(unicode_comparison(source, copy.deepcopy(source)), [])
        for records in [source['records'][::-1], source['records'][:1], [source['records'][0]] * 2]:
            with self.assertRaises(ComparisonError):
                unicode_comparison(source, {'records': records})
        native = copy.deepcopy(source)
        native['records'][0]['result'][0] = 0
        self.assertEqual(len(unicode_comparison(source, native)), 1)

    def test_native_numbers_keep_roundtrip_precision(self):
        for value in [0.0, -0.0, 16.0, 1.2345678901234567, 1e-25, 1e25]:
            self.assertEqual(float(number_token(value, 16)), value)
        self.assertEqual(number_token(None, 16), '16')
        for value in [True, float('inf'), float('nan')]:
            with self.assertRaises(ComparisonError):
                number_token(value, 16)


if __name__ == '__main__':
    unittest.main()
