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

    def result(self, projected_html, fixture=None, **extra):
        fixture = fixture or self.fixture
        return {'example': 626, 'markdown': fixture['markdown'],
                'expected_html': fixture['html'], 'projected_html': projected_html,
                'inline_lossless': True, 'roundtrip': True, **extra}

    def test_correct_projected_html_passes_using_fixture_html(self):
        row = run.case_result(626, self.fixture, self.result(self.fixture['html']))
        self.assertTrue(row['semantic_match'])
        self.assertEqual(row['expected_html'], self.fixture['html'])
        self.assertEqual(row['id'], 'commonmark:626')

    def test_corpus_qualified_identity_and_native_id_are_reported(self):
        fixture = {
            'id': 'published-gfm:1',
            'native_id': 1,
            'example': 1,
            'corpus': 'published-gfm',
            'section': 'Introduction',
            'markdown': 'foo\n',
            'html': '<p>foo</p>\n',
            'source_path': 'https://github.github.com/gfm/',
            'source_lines': {'start': 50, 'end': 55},
            'native_profile': 'published-gfm',
            'native_interface_gaps': [],
        }
        row = run.case_result(1, fixture, self.result('<p>foo</p>\n', fixture=fixture))
        self.assertEqual(row['id'], 'published-gfm:1')
        self.assertEqual(row['example'], 1)
        self.assertEqual(row['native_id'], 1)
        self.assertEqual(row['corpus'], 'published-gfm')
        self.assertEqual(row['source_path'], fixture['source_path'])
        self.assertEqual(row['source_lines'], fixture['source_lines'])
        self.assertTrue(row['semantic_match'])

    def test_partial_commonmark_report_labels_unreported_fixture_without_missing_id(self):
        # A filtered run or timeout may leave this fixture absent from JSONL.
        # Its stable report identity must still be available for the missing list.
        native_to_fixture = {626: self.fixture}
        self.assertEqual(run.label_native_examples([626], native_to_fixture), ['commonmark:626'])

    def test_missing_and_duplicate_gfm_labels_keep_corpus_qualified_ids(self):
        fixture = {'id': 'published-gfm:1', 'native_id': 1, 'example': 1,
                   'corpus': 'published-gfm', 'section': 'Introduction',
                   'markdown': 'foo\n', 'html': '<p>foo</p>\n'}
        native_to_fixture = {1: fixture}
        self.assertEqual(run.label_native_examples([1], native_to_fixture), ['published-gfm:1'])
        self.assertEqual(run.label_native_examples([999], native_to_fixture), [999])

    def test_corpus_ids_keep_colliding_original_numbers_separate(self):
        published = {'id': 'published-gfm:279', 'native_id': 279, 'example': 279,
                     'corpus': 'published-gfm', 'section': 'Task list items',
                     'markdown': '- [ ] a\n', 'html': '<ul></ul>\n'}
        repository = {'id': 'cmark-spec:279', 'native_id': 10279, 'example': 279,
                      'corpus': 'cmark-spec', 'section': 'Lists',
                      'markdown': '- a\n', 'html': '<ul><li>a</li></ul>\n'}
        first = run.case_result(279, published, self.result(published['html']))
        second = run.case_result(10279, repository,
                                 {'example': 10279, 'markdown': repository['markdown'],
                                  'expected_html': repository['html'],
                                  'projected_html': repository['html']})
        self.assertEqual(first['id'], 'published-gfm:279')
        self.assertEqual(second['id'], 'cmark-spec:279')
        self.assertNotEqual(first['native_id'], second['native_id'])

    def test_report_cannot_replace_official_expected_html(self):
        row = run.case_result(626, self.fixture, self.result(
            self.fixture['html'], expected_html='<bogus/>', semantic_match=True,
            expected=['forged'], actual=['forged']))
        self.assertEqual(row['expected_html'], self.fixture['html'])
        self.assertTrue(row['semantic_match'])
        self.assertFalse(row['fixture_input_match'])
        self.assertEqual(run.calculate_formal_exit_code(0, [row], [], [], [], [], True), 1)
        self.assertEqual(row['expected'], run.canonical_html(self.fixture['html']))

    def test_reported_markdown_must_match_the_full_fixture_input(self):
        fixture = {**self.fixture, 'markdown': 'prefix\x00suffix\n', 'html': '<p>full</p>\n'}
        report = {'example': 626, 'markdown': 'prefix', 'expected_html': fixture['html'],
                  'projected_html': fixture['html'], 'inline_lossless': True,
                  'roundtrip': True}
        row = run.case_result(626, fixture, report)
        self.assertFalse(row['fixture_input_match'])
        self.assertEqual(row['reported_markdown'], 'prefix')
        self.assertEqual(run.calculate_formal_exit_code(0, [row], [], [], [], [], True), 1)

    def test_reviewed_reference_difference_is_verified_without_becoming_a_pass_or_skip(self):
        fixture = {**self.fixture, 'id': 'cmark-spec:398', 'example': 398,
                   'corpus': 'cmark-spec', 'markdown': '__foo, __bar__, baz__\n',
                   'html': '<p><strong>foo, bar, baz</strong></p>\n',
                   'reviewed_reference_difference': {
                       'kind': 'current-commonmark-reference',
                       'current_expected_html': '<p><strong>foo, <strong>bar</strong>, baz</strong></p>\n',
                       'commonmark_example': 389,
                   }}
        result = self.result(fixture['reviewed_reference_difference']['current_expected_html'],
                             fixture=fixture)
        row = run.case_result(102398, fixture, result)
        self.assertFalse(row['semantic_match'])
        self.assertIsNone(row['semantic_skip_reason'])
        self.assertTrue(row['reference_difference_verified'])
        self.assertEqual(run.calculate_formal_exit_code(0, [row], [], [], [], [], False), 0)
        summary = run.summarize([row])
        self.assertEqual(summary['totals']['strict_pass'], 0)
        self.assertEqual(summary['totals']['strict_diff'], 1)
        self.assertEqual(summary['totals']['reference_difference_verified'], 1)
        self.assertEqual(summary['totals']['unresolved_diff'], 0)

    def test_reference_verification_requires_current_output_and_native_source_invariants(self):
        fixture = {**self.fixture, 'id': 'cmark-spec:398', 'example': 398,
                   'reviewed_reference_difference': {
                       'current_expected_html': '<p>current</p>\n'}}
        result = self.result('<p>different</p>\n')
        row = run.case_result(102398, fixture, result)
        self.assertFalse(row['reference_difference_verified'])
        self.assertEqual(run.calculate_formal_exit_code(0, [row], [], [], [], [], False), 1)
        result = self.result('<p>current</p>\n', inline_lossless=False)
        row = run.case_result(102398, fixture, result)
        self.assertFalse(row['reference_difference_verified'])
        self.assertEqual(run.calculate_formal_exit_code(0, [row], [], [], [], [], False), 1)
        result = self.result('<p>current</p>\n', roundtrip=False)
        result.pop('roundtrip')
        row = run.case_result(102398, fixture, result)
        self.assertFalse(row['reference_difference_verified'])
        self.assertEqual(run.calculate_formal_exit_code(0, [row], [], [], [], [], False), 1)

    def test_html_difference_fails_even_when_filtered(self):
        row = run.case_result(626, self.fixture, self.result('<div>wrong</div>'))
        self.assertFalse(row['semantic_match'])
        self.assertEqual(run.calculate_formal_exit_code(
            0, [row], [], [], [], [], True), 1)

    def test_cross_block_comment_and_entities_use_shared_canonicalizer(self):
        expected = '<div title="a&amp;b">\n<!-- multi\nline -->\n<p>x &amp; y</p>\n</div>'
        actual = '<DIV title="a&amp;b">\n<!-- multi\nline -->\n<p>x &#38; y</p>\n</DIV>'
        fixture = {**self.fixture, 'html': expected}
        row = run.case_result(626, fixture, self.result(actual, fixture=fixture))
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
        source_failure = run.case_result(
            626, self.fixture, self.result(self.fixture['html'], inline_lossless=False))
        missing_save = self.result(self.fixture['html'])
        missing_save.pop('roundtrip')
        missing_source_check = run.case_result(626, self.fixture, missing_save)
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [1], [], [], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [], [999], [], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [], [], [626], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [base], [], [], [], [{'line': 2}], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(0, [error], [], [], [], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(
            0, [source_failure], [], [], [], [], False), 1)
        self.assertEqual(run.calculate_formal_exit_code(
            0, [missing_source_check], [], [], [], [], False), 1)

    def test_filtered_run_requires_at_least_one_reported_case(self):
        self.assertEqual(run.calculate_formal_exit_code(
            0, [], [], [], [], [], True), 1)
        unreported = run.case_result(626, self.fixture, None)
        self.assertEqual(run.calculate_formal_exit_code(
            0, [unreported], [], [], [], [], True), 1)

    def test_valid_filtered_case_passes_but_unexpected_native_id_fails(self):
        base = run.case_result(626, self.fixture, self.result(self.fixture['html']))
        self.assertEqual(run.calculate_formal_exit_code(
            0, [base], [], [], [], [], True), 0)
        self.assertEqual(run.calculate_formal_exit_code(
            0, [base], [], [999], [], [], True), 1)

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
