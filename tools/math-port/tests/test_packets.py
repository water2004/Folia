import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validation.common import ComparisonError
from validation.packets import (OWNER_CHECKS, SESSION_CHECKS, SESSION_VALUES, concat_report,
                                projection_comparison, session_shape)


class PacketComparisonTests(unittest.TestCase):
    def test_concat_preserves_unsupported_status_and_full_records(self):
        source = {'node': 'fixture', 'method': 'Array.prototype.concat', 'schema': 'tagged-slots-v1', 'rows': [
            {'name': 'supported', 'error': 'ok', 'result': {'slots': [1, None]}, 'trace': ['read']},
            {'name': 'boundary', 'error': 'ok', 'result': 1, 'trace': []}]}
        native = copy.deepcopy(source)
        native['rows'][1]['error'] = 'TypeError'
        mapping = {'rows': [{'source': 'supported', 'status': 'unit-observed'}, {'source': 'boundary', 'status': 'unsupported'}]}
        report = concat_report(source, native, mapping)
        self.assertEqual(report['matched'], 1)
        self.assertEqual(len(report['boundaries']), 1)
        self.assertEqual(report['mismatches'], [])
        for change in ['error', 'result', 'trace', 'extra-key', 'order', 'missing', 'extra', 'method', 'schema']:
            with self.subTest(change=change):
                changed = copy.deepcopy(native)
                if change in ['error', 'result', 'trace']:
                    changed['rows'][0][change] = {'wrong': True}
                elif change == 'extra-key':
                    changed['rows'][0]['extra'] = None
                elif change == 'order':
                    changed['rows'][0] = dict(reversed(list(changed['rows'][0].items())))
                elif change == 'missing':
                    changed['rows'].pop()
                elif change == 'extra':
                    changed['rows'].append({'name': 'unexpected'})
                else:
                    changed[change] = 'different'
                self.assertTrue(concat_report(source, changed, mapping)['mismatches'])
        changed = copy.deepcopy(native)
        changed['rows'][1]['error'] = 'ok'
        self.assertTrue(concat_report(source, changed, mapping)['mismatches'])

    def test_every_map_handler_native_check_and_order(self):
        source = {'sourceLockSha256': 'pinned', 'cases': [{'name': 'map-handler-two-module-instances-interleaved',
                                                        'values': copy.deepcopy(SESSION_VALUES)}]}
        native = {'cases': copy.deepcopy(source['cases']), 'nativeChecks': dict.fromkeys(SESSION_CHECKS, True)}
        session_shape(source, native)
        for key in SESSION_CHECKS:
            changed = copy.deepcopy(native)
            changed['nativeChecks'][key] = False
            with self.assertRaises(ComparisonError):
                session_shape(source, changed)
        for location in ['top', 'values', 'checks', 'extra-case']:
            changed = copy.deepcopy(native)
            if location == 'top':
                changed = dict(reversed(list(changed.items())))
            elif location == 'values':
                changed['cases'][0]['values'] = dict(reversed(list(changed['cases'][0]['values'].items())))
            elif location == 'checks':
                changed['nativeChecks'] = dict(reversed(list(changed['nativeChecks'].items())))
            else:
                changed['cases'].append(copy.deepcopy(changed['cases'][0]))
            with self.assertRaises(ComparisonError):
                session_shape(source, changed)

    def test_projection_checks_complete_records_and_all_six_owners(self):
        source = {'nodeFactory': {'values': [1, False], 'metadata': {'first': 1, 'second': 2}},
                  'parseOptions': {'options': {'text': 'µ', 'width': 1}}}
        native = copy.deepcopy(source) | {'runtimeOwners': dict.fromkeys(OWNER_CHECKS, True)}
        self.assertEqual(projection_comparison(source, native), [])
        for key in OWNER_CHECKS:
            changed = copy.deepcopy(native)
            changed['runtimeOwners'][key] = False
            self.assertTrue(projection_comparison(source, changed))
        changed = copy.deepcopy(native)
        changed['nodeFactory']['metadata'] = {'second': 2, 'first': 1}
        self.assertTrue(projection_comparison(source, changed))
        changed = copy.deepcopy(native)
        changed['parseOptions']['options']['width'] += 1e-12
        self.assertTrue(projection_comparison(source, changed))


if __name__ == '__main__':
    unittest.main()
