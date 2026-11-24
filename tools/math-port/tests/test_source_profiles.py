import copy
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import Mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ROOT, ComparisonError, read
from validation.source_profiles import CONFIG, compare_profile, replay


class SourceProfileTests(unittest.TestCase):
    def test_all_nine_complete_frozen_profiles_and_corruptions(self):
        config = read(CONFIG)
        for entry in config['container'] + config['remaining']:
            with self.subTest(owner=entry['owner']):
                expected = read(ROOT / entry['profile'])
                actual = ({'profile': Path(entry['profile']).name, 'records': copy.deepcopy(expected['records'])}
                          if entry['comparison'] == 'records' else copy.deepcopy(expected))
                parent = read(ROOT / entry['parent_profile']) if 'parent_profile' in entry else None
                compare_profile(entry, actual, expected, parent=parent)
                field = 'records' if 'records' in actual else 'scenarios' if 'scenarios' in actual else 'cases'
                for change in ['name', 'missing', 'extra', 'order']:
                    changed = copy.deepcopy(actual)
                    rows = changed[field]
                    if change == 'name':
                        identity = 'name' if 'name' in rows[0] else 'id'
                        rows[0][identity] = 'corrupted-' + rows[0][identity]
                    elif change == 'missing':
                        rows.pop()
                    elif change == 'extra':
                        rows.append(copy.deepcopy(rows[0]))
                    else:
                        rows[0], rows[1] = rows[1], rows[0]
                    with self.assertRaises(ComparisonError, msg=change):
                        compare_profile(entry, changed, expected, parent=parent)
                if parent:
                    changed = copy.deepcopy(parent)
                    changed['scenarios'][0]['expected'].pop()
                    with self.assertRaises(ComparisonError):
                        compare_profile(entry, actual, expected, parent=changed)

    def test_record_observations_retain_own_key_order(self):
        entry = {'profile': 'unit.json', 'sha256': 'unit', 'comparison': 'records'}
        expected = {'records': [{'name': 'unit', 'observations': [{'first': 1, 'second': 2}]}]}
        actual = {'profile': 'unit.json', 'records': copy.deepcopy(expected['records'])}
        compare_profile(entry, actual, expected)
        actual['records'][0]['observations'][0] = {'second': 2, 'first': 1}
        with self.assertRaises(ComparisonError):
            compare_profile(entry, actual, expected)

    def test_hash_failure_precedes_process_and_nonzero_capture_is_rejected(self):
        entry = read(CONFIG)['container'][0]
        runner = Mock()
        changed = dict(entry, sha256='0' * 64)
        with self.assertRaises(ComparisonError):
            replay(changed, runner=runner)
        runner.assert_not_called()
        runner.return_value = subprocess.CompletedProcess([], 4, b'{}', b'capture failed')
        with self.assertRaisesRegex(ComparisonError, 'capture failed'):
            replay(entry, runner=runner)
        self.assertEqual(runner.call_args.kwargs['timeout'], 60)
        self.assertNotIn('--check', runner.call_args.args[0])


if __name__ == '__main__':
    unittest.main()
