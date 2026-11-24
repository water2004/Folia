import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ROOT, ComparisonError, read
from validation.profile75.metadata import validate_owner_boundaries
from validation.profile75.records import (NATIVE_BOUNDARIES, SOURCE_BOUNDARIES, compare_records, validate_scene_counters)


class Profile75Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = read(ROOT / 'tools/math-port/candidates/profile75-full-svg-g147/profile75-full-svg-source.json')
        cls.cases = [{key: row[key] for key in ['name', 'tex', 'display', 'em', 'ex', 'width']} for row in cls.source['records'][:75]]

    def native(self):
        return {'records': [copy.deepcopy(row) | {'sceneNodes': 0 if row['svg'] is None else 1} for row in self.source['records']]}

    def test_all150_original_records_and_svg_corruption_probe(self):
        native = self.native()
        matched, failures = compare_records(self.cases, self.source, native)
        self.assertEqual((len(matched), len(failures)), (150, 0))
        index = next(index for index, row in enumerate(native['records']) if row['svg'] is not None)
        native['records'][index]['svg'] += '<!-- deliberate test mismatch -->'
        matched, failures = compare_records(self.cases, self.source, native)
        self.assertEqual((len(matched), len(failures)), (149, 1))
        self.assertRegex(failures[0]['sourceSvgSha256'], '^[0-9a-f]{64}$')
        self.assertRegex(failures[0]['nativeSvgSha256'], '^[0-9a-f]{64}$')

    def test_each_identity_output_and_owner_boundary(self):
        index = next(index for index, row in enumerate(self.source['records']) if row['svg'] is not None)
        for key, value in [('group', 'wrong'), ('index', -1), ('name', 'wrong'), ('tex', 'wrong'), ('display', None),
                           ('em', -1), ('ex', -1), ('width', -1), ('svg', None), ('mmlError', 'false'),
                           ('error', {'name': 'Error', 'message': 'different'}), ('sceneNodes', 0)]:
            with self.subTest(field=key):
                native = self.native()
                native['records'][index][key] = value
                matched, failures = compare_records(self.cases, self.source, native)
                self.assertEqual((len(matched), len(failures)), (149, 1))
        source = {'source': {'runtimeBoundaries': copy.deepcopy(SOURCE_BOUNDARIES)}}
        native = {'runtimeBoundaries': copy.deepcopy(NATIVE_BOUNDARIES)}
        validate_owner_boundaries(source, native)
        native['runtimeBoundaries']['fresh-session-per-case']['owner'] = 'one reused renderer'
        with self.assertRaises(ComparisonError):
            validate_owner_boundaries(source, native)

    def test_all_scene_only_counters_and_error_routes(self):
        native = self.native()
        for row in native['records']:
            row.update(sceneOnlyMaterializeCalls=0, sceneOnlySerializeXmlCalls=0, sceneEqual=True,
                       svgMaterializeCalls=1, svgSerializeXmlCalls=1)
            if row['error'] is not None:
                row['error'] = {'scene': copy.deepcopy(row['error']), 'svg': copy.deepcopy(row['error'])}
        validate_scene_counters(native)
        index = next(index for index, row in enumerate(native['records']) if row['error'] is None)
        for key, value in [('sceneOnlyMaterializeCalls', 1), ('sceneOnlySerializeXmlCalls', 1), ('sceneEqual', False),
                           ('svgMaterializeCalls', 0), ('svgSerializeXmlCalls', 0)]:
            changed = copy.deepcopy(native)
            changed['records'][index][key] = value
            with self.assertRaises(ComparisonError):
                validate_scene_counters(changed)
        changed = copy.deepcopy(native)
        changed['records'][index]['error'] = {'scene': {'name': 'Error'}, 'svg': {'name': 'Different'}}
        with self.assertRaises(ComparisonError):
            validate_scene_counters(changed)


if __name__ == '__main__':
    unittest.main()
