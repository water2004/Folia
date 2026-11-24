import copy
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ROOT, ComparisonError, read
from validation.ledgers import CONFIG
from validation.ledgers.comparison import compare, reviewed_copy


def unreviewed(accepted):
    """Retained reviewed records are unit input, not a claim of source capture."""
    result = copy.deepcopy(accepted)
    result['status'] = 'unreviewed_member_inventory'
    for file in result['files']:
        file.pop('native_adaptation', None)
        for group in ['members', 'module_bindings', 'object_methods']:
            for member in file.get(group, []):
                for key in ['target', 'review', 'evidence', 'native_adaptation']:
                    member.pop(key, None)
                if group != 'object_methods':
                    member.update(target=None, review='unreviewed')
    return result


def fixture_paths(root, accepted):
    for file in accepted['files']:
        for group in ['members', 'module_bindings', 'object_methods']:
            for member in file.get(group, []):
                for name in [member['target'].split(':', 1)[0], member['evidence']]:
                    path = root / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.touch()


class LedgerTests(unittest.TestCase):
    def test_all_retained_members_bindings_and_source_fields(self):
        for name, profile in read(CONFIG).items():
            with self.subTest(profile=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                accepted = read(ROOT / profile['accepted'])
                fresh = unreviewed(accepted)
                fixture_paths(root, accepted)
                self.assertEqual(compare(name, profile, fresh, accepted, root=root), profile['summary'])
                for file_index, source in enumerate(fresh['files']):
                    for group in profile.get('groups', ['members']):
                        for index, member in enumerate(source.get(group, [])):
                            # Every original executable member, field, and final
                            # prototype/object-method binding must be observed.
                            changed = copy.deepcopy(accepted)
                            changed['files'][file_index][group][index]['member_sha256'] = '0' * 64
                            with self.assertRaises(ComparisonError):
                                compare(name, profile, fresh, changed, root=root)
                for key in ['source', 'source_sha256']:
                    changed = copy.deepcopy(accepted)
                    changed['files'][0][key] = 'altered'
                    with self.assertRaises(ComparisonError):
                        compare(name, profile, fresh, changed, root=root)
                for key in ['owner', 'kind', 'name', 'static', 'line', 'end_line', 'review', 'target']:
                    changed = copy.deepcopy(accepted)
                    changed['files'][0]['members'][0][key] = ''
                    with self.assertRaises(ComparisonError, msg=key):
                        compare(name, profile, fresh, changed, root=root)
                for change in ['missing-member', 'extra-member', 'missing-file', 'extra-file']:
                    changed = copy.deepcopy(accepted)
                    rows = changed['files'][0]['members'] if change.endswith('member') else changed['files']
                    if change.startswith('missing'):
                        rows.pop()
                    else:
                        rows.append(copy.deepcopy(rows[0]))
                    with self.assertRaises(ComparisonError, msg=change):
                        compare(name, profile, fresh, changed, root=root)

    def test_freeze_preserves_all_reviewed_mappings_and_descriptions(self):
        for name, profile in read(CONFIG).items():
            if 'freeze_status' not in profile:
                continue
            with self.subTest(profile=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                accepted = read(ROOT / profile['accepted'])
                fresh = unreviewed(accepted)
                fixture_paths(root, accepted)
                result = reviewed_copy(name, profile, fresh, root=root)
                self.assertEqual(result, accepted)
                compare(name, profile, result, accepted, root=root)

    def test_target_evidence_paths_and_frozen_evidence_are_strict(self):
        for name, profile in read(CONFIG).items():
            with self.subTest(profile=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                accepted = read(ROOT / profile['accepted'])
                fresh = unreviewed(accepted)
                fixture_paths(root, accepted)
                first = accepted['files'][0]['members'][0]
                if 'evidence' in profile or profile.get('evidence_exists'):
                    changed = copy.deepcopy(accepted)
                    changed['files'][0]['members'][0]['evidence'] = 'missing.md'
                    with self.assertRaises(ComparisonError):
                        compare(name, profile, fresh, changed, root=root)
                if profile.get('target_exists'):
                    (root / first['target'].split(':', 1)[0]).unlink()
                    with self.assertRaises(ComparisonError):
                        compare(name, profile, fresh, accepted, root=root)
                if profile.get('freeze_evidence_required'):
                    (root / profile['freeze_evidence_required']).unlink()
                    with self.assertRaises(ComparisonError):
                        reviewed_copy(name, profile, fresh, root=root)


if __name__ == '__main__':
    unittest.main()
