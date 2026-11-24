"""Fixture bytes, row order, corpus completeness and inventory trust boundaries."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import check_fixtures


class FixtureCheckpointTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.data = self.write('tests/math/fixtures/cases.tsv', b'# header\na\t1\nb\t2\n')
        self.write('tests/mermaid_regression/cases/flowchart.json', b'[{"name":"flow"}]\n')
        self.write('tools/mermaid-cpp/regression/tests/fixtures/samples/small.rgba', b'\0\r\n\xff')
        self.manifest = check_fixtures.checkpoint(self.root)
        self.save_manifest()

    def write(self, path, contents):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(contents)
        return target

    def save_manifest(self):
        self.write(check_fixtures.MANIFEST, json.dumps(self.manifest).encode())

    def test_complete_inventory_and_row_count(self):
        report = check_fixtures.check(self.root)
        self.assertTrue(report['success'])
        self.assertEqual(report['recorded_files'], 3)
        self.assertEqual(report['recorded_tsv_rows'], 2)
        row = next(row for row in self.manifest['files'] if row['path'].endswith('.tsv'))
        self.assertEqual(row['sha256'], hashlib.sha256(self.data.read_bytes()).hexdigest())

    def test_changed_reordered_and_duplicated_rows_are_detected_at_identical_size(self):
        for changed in (b'# header\na\t9\nb\t2\n', b'# header\nb\t2\na\t1\n', b'# header\na\t1\na\t1\n'):
            with self.subTest(changed=changed):
                self.data.write_bytes(changed)
                report = check_fixtures.check(self.root)
                self.assertFalse(report['success'])
                self.assertEqual(report['recorded_tsv_rows'], 2)
                self.assertEqual(report['failures'], ['Changed fixture: tests/math/fixtures/cases.tsv (sha256)'])

    def test_removed_and_added_rows_are_detected(self):
        for changed in (b'a\t1\n', b'a\t1\nb\t2\nc\t3\n'):
            with self.subTest(changed=changed):
                self.data.write_bytes(changed)
                report = check_fixtures.check(self.root)
                self.assertFalse(report['success'])
                self.assertIn('tsv_rows', report['failures'][0])

    def test_missing_and_new_data_are_detected(self):
        self.data.unlink()
        self.write('tests/math/fixtures/new-observation.dat', b'new')
        report = check_fixtures.check(self.root)
        self.assertEqual(report['failures'], [
            'Missing fixture: tests/math/fixtures/cases.tsv',
            'Unrecorded fixture: tests/math/fixtures/new-observation.dat'])

    def test_raw_upstream_physical_cr_and_binary_bytes_are_exact(self):
        original = b'line1\r\nline2\rline3\n'
        raw = self.write('tests/parsing/fixtures/conformance/upstream/gfm/entities.inc', original)
        self.manifest = check_fixtures.checkpoint(self.root)
        self.save_manifest()
        self.assertTrue(check_fixtures.check(self.root)['success'])
        raw.write_bytes(original.replace(b'\r\n', b'\n'))
        self.assertIn('Changed fixture:', check_fixtures.check(self.root)['failures'][0])
        raw.write_bytes(original)
        binary = self.root / 'tools/mermaid-cpp/regression/tests/fixtures/samples/small.rgba'
        binary.write_bytes(b'\0\n\xff')
        self.assertIn('Changed fixture:', check_fixtures.check(self.root)['failures'][0])

    def test_tsv_count_matches_reader_while_hash_retains_comments_and_cr(self):
        self.data.write_bytes(b'#comment\r\n\r\na\t1\r\n\tb\nlast\t')
        observation = check_fixtures.observation(self.root, 'tests/math/fixtures/cases.tsv')
        self.assertEqual(observation['tsv_rows'], 3)
        self.assertEqual(observation['bytes'], len(self.data.read_bytes()))

    def test_sdk_sources_code_and_checkpoint_itself_are_excluded(self):
        for path in ('tests/platform/fixtures/windows.json', 'tests/app/fixtures/app.txt',
                     'tests/encoding/fixtures/utf8.bin', 'tests/math/fixtures/helper.hpp',
                     'tests/math/fixtures/README.md', 'tests/math/fixtures/observer.mjs',
                     'tools/math-port/tests/fixtures/child.py', 'tools/node_modules/tests/fixtures/data.json',
                     'tools/math-port/drafts/old-checkout/tests/fixtures/cases.tsv',
                     'tests/fixtures/preservation/other-checkpoint.json'):
            self.write(path, b'not fixture data')
        self.assertTrue(check_fixtures.check(self.root)['success'])
        self.write('tests/fixtures/unicode/graphemes.txt', b'a')
        self.assertFalse(check_fixtures.check(self.root)['success'])

    def test_path_escape_and_noncanonical_paths_are_rejected(self):
        for path in ('../outside.json', '/tmp/outside.json', 'tests/math/fixtures/../../outside.json',
                     'tests/math/fixtures/./cases.tsv', 'tests//math/fixtures/cases.tsv',
                     'C:/outside.json', 'tests\\math\\fixtures\\cases.tsv', 'tests/math/fixtures/bad\0.json'):
            with self.subTest(path=path):
                changed = copy.deepcopy(self.manifest)
                changed['files'][0]['path'] = path
                with self.assertRaises(ValueError):
                    check_fixtures.validate(changed, self.root)

    def test_symlink_file_or_directory_is_rejected_without_reading_target(self):
        target = self.write('elsewhere/private.json', b'outside data')
        link = self.root / 'tests/math/fixtures/alias.json'
        try:
            link.symlink_to(target)
        except OSError as error:
            self.skipTest('This platform does not allow test symlinks: ' + str(error))
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            check_fixtures.check(self.root)
        link.unlink()
        link.symlink_to(target.parent, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            check_fixtures.check(self.root)

    def test_malformed_inventories_cannot_reduce_or_hide_scope(self):
        mutations = [
            lambda value: value.update(schema=True),
            lambda value: value.update(schema=2),
            lambda value: value.update(scope=[]),
            lambda value: value.update(hash_policy='normalized'),
            lambda value: value.update(files=[]),
            lambda value: value['files'].append(copy.deepcopy(value['files'][0])),
            lambda value: value['files'].reverse(),
            lambda value: value['files'][0].update(path='README.md'),
            lambda value: value['files'][0].update(sha256='x' * 64),
            lambda value: value['files'][0].update(bytes=True),
            lambda value: value['files'][0].update(tsv_rows=-1),
            lambda value: value['files'][0].pop('sha256'),
            lambda value: value.update(unreviewed=True),
        ]
        for mutate in mutations:
            changed = copy.deepcopy(self.manifest)
            mutate(changed)
            with self.subTest(manifest=changed), self.assertRaises(ValueError):
                check_fixtures.validate(changed, self.root)
        self.write(check_fixtures.MANIFEST, b'{"schema":1,"schema":1}')
        with self.assertRaisesRegex(ValueError, 'Duplicate JSON key'):
            check_fixtures.check(self.root)

    def test_refresh_is_explicit_and_does_not_rewrite_data(self):
        self.data.write_bytes(b'changed\tinput\n')
        before = self.data.read_bytes()
        self.assertFalse(check_fixtures.check(self.root)['success'])
        self.assertTrue(check_fixtures.refresh(self.root)['success'])
        self.assertEqual(self.data.read_bytes(), before)
        self.assertEqual(check_fixtures.check(self.root)['recorded_tsv_rows'], 1)


class RepositoryFixtureCheckpointTests(unittest.TestCase):
    def test_committed_fixture_checkpoint(self):
        report = check_fixtures.check()
        self.assertTrue(report['success'], '\n'.join(report['failures']))


if __name__ == '__main__':
    unittest.main()
