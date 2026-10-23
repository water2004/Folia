import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

RUN_PATH = Path(__file__).with_name('run.py')
spec = importlib.util.spec_from_file_location('folia_commonmark_runner', RUN_PATH)
run = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run)


class RunnerContractTests(unittest.TestCase):
    def setUp(self):
        self.fixture = {
            'example': 626,
            'section': 'HTML blocks',
            'markdown': '<div>\n\n<!-- split\ncomment -->\n</div>\n',
            'html': '<div>\n\n<!-- split\ncomment -->\n</div>\n',
        }

    def result(self, projected_html, **extra):
        return {'example': 626, 'projected_html': projected_html, **extra}

    def test_correct_projected_html_passes_using_fixture_html(self):
        row = run.case_result(626, self.fixture, self.result(self.fixture['html']))
        self.assertTrue(row['semantic_match'])
        self.assertEqual(row['expected_html'], self.fixture['html'])

    def test_report_cannot_replace_official_expected_html(self):
        row = run.case_result(626, self.fixture, self.result(
            self.fixture['html'], expected_html='<bogus/>', semantic_match=True,
            expected=['forged'], actual=['forged']))
        self.assertEqual(row['expected_html'], self.fixture['html'])
        self.assertTrue(row['semantic_match'])
        self.assertEqual(row['expected'], run.canonical_html(self.fixture['html']))

    def test_html_difference_fails_even_when_filtered(self):
        row = run.case_result(626, self.fixture, self.result('<div>wrong</div>'))
        self.assertFalse(row['semantic_match'])
        self.assertEqual(run.calculate_formal_exit_code(
            0, [row], [], [], [], [], True), 1)

    def test_cross_block_comment_and_entities_use_shared_canonicalizer(self):
        expected = '<div title="a&amp;b">\n<!-- multi\nline -->\n<p>x &amp; y</p>\n</div>'
        actual = '<DIV title="a&amp;b">\n<!-- multi\nline -->\n<p>x &#38; y</p>\n</DIV>'
        fixture = {**self.fixture, 'html': expected}
        row = run.case_result(626, fixture, self.result(actual))
        self.assertTrue(row['semantic_match'])

    def test_missing_projected_html_is_malformed_not_precomputed_success(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'results.jsonl'
            path.write_text(json.dumps({'example': 626, 'semantic_match': True}) + '\n', encoding='utf-8')
            rows, malformed = run.read_results(path)
        self.assertEqual(rows, [])
        self.assertEqual(len(malformed), 1)
        row = run.case_result(626, self.fixture, None)
        self.assertIsNone(row['semantic_match'])
        self.assertEqual(run.calculate_formal_exit_code(
            0, [row], [], [], [], malformed, True), 1)

    def test_non_string_projection_is_malformed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'results.jsonl'
            path.write_text(json.dumps({'example': 626, 'projected_html': [], 'semantic_match': True}) + '\n', encoding='utf-8')
            rows, malformed = run.read_results(path)
        self.assertEqual(rows, [])
        self.assertEqual(len(malformed), 1)
        self.assertEqual(run.calculate_formal_exit_code(
            0, [], [], [], [], malformed, True), 1)

    def test_full_suite_incompleteness_and_exceptions_fail(self):
        base = run.case_result(626, self.fixture, self.result(self.fixture['html']))
        error = run.case_result(626, self.fixture, {'example': 626, 'error': 'parser failed'})
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [1], [], [], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [], [999], [], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [], [], [626], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [], [], [], [{'line': 2}], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [error], [], [], [], [], False), 1)

    def test_html_policy_skip_is_explicit_and_not_a_pass(self):
        row = run.case_result(626, self.fixture, self.result('<div>different</div>'),
                              skip_reason='Unsupported HTML presentation')
        self.assertIsNone(row['semantic_match'])
        self.assertTrue(row['reported'])
        self.assertEqual(row['semantic_skip_reason'], 'Unsupported HTML presentation')
        totals = run.summarize([row])['totals']
        self.assertEqual(totals['semantic_skipped'], 1)
        self.assertEqual(totals['strict_pass'], 0)
        self.assertEqual(totals['unreported'], 0)

    def test_native_error_or_source_failure_is_not_skipped_by_html_policy(self):
        error = run.case_result(626, self.fixture, {'error': 'parse error'},
                                skip_reason='Unsupported HTML presentation')
        self.assertIsNone(error['semantic_skip_reason'])
        self.assertEqual(run.calculate_formal_exit_code(0, [error], [], [], [], [], False), 1)
        row = run.case_result(626, self.fixture, self.result(self.fixture['html']),
                              skip_reason='Unsupported HTML presentation')
        self.assertEqual(run.calculate_formal_exit_code(1, [row], [], [], [], [], False), 1)

    def test_record_cannot_skip_its_own_semantic_difference(self):
        row = run.case_result(626, self.fixture, self.result('<div>different</div>',
                              semantic_skip_reason='unsupported'))
        self.assertIsNone(row['semantic_skip_reason'])
        self.assertFalse(row['semantic_match'])


if __name__ == '__main__':
    unittest.main()
