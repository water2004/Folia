"""Negative coverage for the portable comparison infrastructure."""
from contextlib import redirect_stdout, redirect_stderr
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation import main
from validation.common import ComparisonError, MISSING, ROOT, difference, loads, truthy


class StrictComparisonTests(unittest.TestCase):
    def test_json_number_semantics_and_scalar_types(self):
        self.assertIsNone(difference(loads('1'), loads('1.0')))
        self.assertIsNotNone(difference(loads('0'), loads('-0')))
        self.assertIsNotNone(difference(True, 1))
        self.assertIsNotNone(difference(None, MISSING))
        self.assertIsNotNone(difference('1', 1))
        self.assertIsNone(difference(MISSING, MISSING))
        for invalid in ['NaN', 'Infinity', '-Infinity']:
            with self.subTest(invalid=invalid), self.assertRaises(ComparisonError):
                loads(invalid)

    def test_arrays_keys_and_nested_order(self):
        self.assertIsNotNone(difference([1, 2], [2, 1]))
        self.assertIsNotNone(difference([], {}))
        self.assertIsNotNone(difference({'x': None}, {}))
        self.assertIsNotNone(difference({'x': 1}, {'x': 1, 'y': 2}))
        expected = {'owner': {'first': 1, 'second': 2}}
        native = {'owner': {'second': 2, 'first': 1}}
        self.assertIsNone(difference(expected, native))
        self.assertIsNotNone(difference(expected, native, ordered=True))
        self.assertIsNone(difference({'2': 'b', '1': 'a'}, {'1': 'a', '2': 'b'}, ordered=True))

    def test_empty_error_object_is_truthy(self):
        self.assertTrue(truthy({}))
        self.assertTrue(truthy([]))
        self.assertFalse(truthy(MISSING))
        self.assertFalse(truthy(0))
        self.assertFalse(truthy(''))


class PackageComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)

    def invoke(self, profile, source, native):
        path = self.directory / 'native.json'
        path.write_text(json.dumps(native), encoding='utf-8')
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            return main(profile, [str(source), str(path)])

    def test_ams_complete_corpora_and_each_observed_field(self):
        for boundary in [False, True]:
            suffix = '-boundary' if boundary else ''
            source = ROOT / f'tools/math-port/ams-newcommand-reference/ams-newcommand{suffix}-source.json'
            records = json.loads(source.read_text())['cases']
            fields = ['result', 'error', 'dynamicMaps', 'packageData']
            native = {'cases': {row['name']: {key: row[key] for key in fields} for row in records}}
            profile = 'ams-newcommand' + suffix
            self.assertEqual(self.invoke(profile, source, native), 0)
            for key in fields:
                changed = copy.deepcopy(native)
                changed['cases'][records[0]['name']][key] = {'unexpected': True}
                with self.subTest(profile=profile, field=key):
                    self.assertEqual(self.invoke(profile, source, changed), 1)
            for removed in [records[0]['name'], records[-1]['name']]:
                changed = copy.deepcopy(native)
                del changed['cases'][removed]
                self.assertEqual(self.invoke(profile, source, changed), 1)
            changed = copy.deepcopy(native)
            changed['cases']['unexpected-case'] = {}
            self.assertEqual(self.invoke(profile, source, changed), 1)

    def test_boldsymbol_schema_order_fields_and_source_pin(self):
        source = ROOT / 'tools/math-port/boldsymbol-configmacros-reference/original.json'
        records = json.loads(source.read_text())['cases']
        fields = ['name', 'result', 'error', 'maps', 'packageData']
        native = {'cases': [{key: row[key] for key in fields} for row in records]}
        self.assertEqual(self.invoke('boldsymbol-configmacros', source, native), 0)
        changed = copy.deepcopy(native)
        changed['cases'].reverse()
        self.assertEqual(self.invoke('boldsymbol-configmacros', source, changed), 1)
        changed = copy.deepcopy(native)
        changed['cases'][0]['unexpected'] = 1
        self.assertEqual(self.invoke('boldsymbol-configmacros', source, changed), 1)
        changed = copy.deepcopy(native)
        changed['extra'] = None
        self.assertEqual(self.invoke('boldsymbol-configmacros', source, changed), 1)
        altered = self.directory / 'source.json'
        altered.write_bytes(source.read_bytes() + b'\n')
        self.assertEqual(self.invoke('boldsymbol-configmacros', altered, native), 1)


if __name__ == '__main__':
    unittest.main()
