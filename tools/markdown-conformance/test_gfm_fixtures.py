from __future__ import annotations
import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / 'tools' / 'markdown-conformance'
sys.path.insert(0, str(TOOLS))
_spec = importlib.util.spec_from_file_location('prepare_gfm', TOOLS / 'prepare_gfm.py')
prepare_gfm = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
_spec.loader.exec_module(prepare_gfm)

EXPECTED_COUNTS = {
    'published-gfm': 677,
    'cmark-spec': 672,
    'extensions': 30,
    'smart-punctuation': 16,
    'regressions': 26,
    'table-style-option': 1,
    'full-info-option': 4,
}


def decode_cpp_literal(literal: str) -> str:
    if not (literal.startswith('"') and literal.endswith('"')):
        raise AssertionError(f'not a generated C++ string literal: {literal!r}')
    body = literal[1:-1]
    if re.search(r'(?<!\\)\\(?!x[0-9a-f]{2})', body):
        raise AssertionError(f'unexpected non-byte escape in generated literal: {literal!r}')
    return bytes.fromhex(re.sub(r'\\x', '', body)).decode('utf-8')


class ExpandedFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = prepare_gfm.prepare()

    def test_all_sources_are_hash_verified_and_fixture_corpora_are_complete(self):
        lock = prepare_gfm._lock()
        counts = {}
        for case in self.cases:
            counts[case['corpus']] = counts.get(case['corpus'], 0) + 1
            self.assertIsInstance(case['markdown'], str)
            self.assertIsInstance(case['html'], str)
            self.assertTrue(case['markdown'])
            self.assertIn('source_path', case)
            self.assertGreater(case['source_lines']['start'], 0)
            self.assertGreaterEqual(case['source_lines']['end'], case['source_lines']['start'])
            self.assertIn(case['license'], ('CC BY-SA 4.0', 'BSD-2-Clause'))
        self.assertEqual(counts, EXPECTED_COUNTS)
        self.assertEqual(len(self.cases), lock['counts']['expanded_fixture_cases'])
        self.assertEqual(len({case['id'] for case in self.cases}), len(self.cases))
        self.assertEqual(len({case['native_id'] for case in self.cases}), len(self.cases))
        self.assertEqual(
            [case['example'] for case in self.cases if case['corpus'] == 'published-gfm'],
            lock['published_example_ids_all'])

    def test_published_page_includes_base_examples_and_retains_published_ids(self):
        cases = [case for case in self.cases if case['corpus'] == 'published-gfm']
        self.assertEqual(cases[0]['example'], 1)
        self.assertEqual(cases[-1]['example'], 677)
        self.assertEqual(next(case for case in cases if case['example'] == 176)['html'], '')
        self.assertEqual(next(case for case in cases if case['example'] == 188)['html'], '')
        self.assertEqual([case['example'] for case in cases if case['example'] in (176, 188, 198, 657)],
                         [176, 188, 198, 657])
        self.assertEqual(
            next(case for case in cases if case['example'] == 491)['markdown'],
            '~~Hi~~ Hello, ~there~ world!\n')
        tabs = cases[0]
        self.assertEqual(tabs['markdown'], '\tfoo\tbaz\t\tbim\n')
        self.assertEqual(tabs['source_markdown_raw'], '→foo→baz→→bim\n')
        self.assertEqual(tabs['fixture_transformations'],
                         ['U+2192 right-arrow marker converted to U+0009 TAB'])

    def test_repository_disabled_and_ignore_records_are_retained_without_policy_skip(self):
        disabled = [case for case in self.cases if case['upstream_disabled']]
        self.assertEqual([case['id'] for case in disabled], ['cmark-spec:279', 'cmark-spec:280'])
        ignored = [case for case in self.cases if case['expected_html_ignore']]
        self.assertEqual([case['id'] for case in ignored], ['extensions:20'])
        self.assertFalse(any(case['id'] != 'published-gfm:657' and
                             case.get('semantic_skip_reason') for case in self.cases))

    def test_html_policy_skips_are_explicit_and_keep_every_case_in_the_corpus(self):
        policy = json.loads((TOOLS / 'gfm-policy.json').read_text(encoding='utf-8'))['semantic_skips']
        known = {case['id']: case for case in self.cases}
        expected = {f'smart-punctuation:{number}' for number in range(1, 17)} | {
            'published-gfm:657', 'cmark-spec:652', 'extensions:22', 'extensions:20'}
        self.assertEqual(set(policy), expected)
        for case_id, entry in policy.items():
            self.assertEqual(known[case_id]['section'], entry['section'])
            self.assertTrue(entry['reason'])

    def test_reviewed_current_reference_expectations_are_source_verified_and_independent(self):
        policy = json.loads((TOOLS / 'gfm-policy.json').read_text(encoding='utf-8'))[
            'reviewed_reference_differences']
        cases = {case['id']: case for case in self.cases}
        self.assertEqual(set(policy), {
            'cmark-spec:398', 'cmark-spec:426', 'cmark-spec:434', 'cmark-spec:435',
            'cmark-spec:436', 'cmark-spec:473', 'cmark-spec:474', 'cmark-spec:475',
            'cmark-spec:477', 'published-gfm:649', 'published-gfm:650'})
        current_commonmark = {case['example']: case
                              for case in prepare_gfm._commonmark_cases(fetch=False)}
        for case_id, entry in policy.items():
            self.assertNotEqual(cases[case_id]['html'], entry['current_expected_html'])
            self.assertIn('spec.commonmark.org/0.31.2/', entry['source_url'])
            if entry['kind'] == 'current-commonmark-reference':
                reference = current_commonmark[entry['commonmark_example']]
                self.assertEqual(reference['markdown'], cases[case_id]['markdown'])
                self.assertEqual(reference['html'], entry['current_expected_html'])
            else:
                self.assertEqual(entry['current_expected_markdown'], cases[case_id]['markdown'])
        self.assertEqual(policy['published-gfm:649']['current_expected_html'],
                         '<p>foo <!-- not a comment -- two hyphens --></p>\n')
        self.assertEqual(policy['published-gfm:650']['current_expected_html'],
                         '<p>foo <!--> foo --&gt;</p>\n<p>foo <!-- foo---></p>\n')

    def test_profiles_report_missing_smart_and_output_option_interfaces(self):
        gaps = {case['corpus'] for case in self.cases if case['native_interface_gaps']}
        self.assertEqual(gaps, {'published-gfm', 'cmark-spec', 'extensions',
                                'smart-punctuation'})
        extension_tagfilter = next(case for case in self.cases if case['id'] == 'extensions:22')
        self.assertEqual(extension_tagfilter['native_interface_gaps'],
                         ['tagfilter HTML output option'])
        self.assertFalse(next(case for case in self.cases
                              if case['corpus'] == 'table-style-option')['native_interface_gaps'])
        self.assertFalse(next(case for case in self.cases
                              if case['corpus'] == 'full-info-option')['native_interface_gaps'])
        for case in self.cases:
            if case['corpus'] == 'regressions':
                self.assertEqual(
                    case['native_profile'],
                    'regressions' + (
                        '+' + '+'.join(sorted(
                            value for value in case['upstream_extensions']
                            if value != 'disabled'))
                        if case['upstream_extensions'] else ''))
        self.assertEqual(
            {case['native_profile'] for case in self.cases if case['corpus'] == 'published-gfm'},
            {'published-gfm', 'published-gfm+table', 'published-gfm+tasklist',
             'published-gfm+strikethrough', 'published-gfm+autolink',
             'published-gfm+tagfilter'})
        published = {case['example']: case for case in self.cases
                     if case['corpus'] == 'published-gfm'}
        for example in (611, 617, 620, 621):
            self.assertEqual(published[example]['native_profile'], 'published-gfm')
        for section, extension in {
            'Tables (extension)': 'table',
            'Task list items (extension)': 'tasklist',
            'Strikethrough (extension)': 'strikethrough',
            'Autolinks (extension)': 'autolink',
            'Disallowed Raw HTML (extension)': 'tagfilter',
        }.items():
            self.assertTrue(all(
                case['native_profile'] == 'published-gfm+' + extension
                for case in published.values() if case['section'] == section))

    def test_generated_cpp_groups_keep_unique_native_ids_and_exact_sample_strings(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'gfm_cases.inc'
            prepare_gfm.generate(output, self.cases)
            generated = output.read_text(encoding='utf-8')
        row_re = re.compile(r'^\{(\d+),(.+)\},$', re.MULTILINE)
        self.assertIn('static constexpr MarkdownCase gfm_all_cases[] = {', generated)
        rows = row_re.findall(generated)
        self.assertEqual(len(rows), len(self.cases))
        observed_ids = []
        by_native_id = {case['native_id']: case for case in self.cases}
        self.assertEqual(len(by_native_id), len(self.cases))
        observed_cases = []
        for native_id, literals in rows:
            case = by_native_id[int(native_id)]
            fields = re.findall(
                r'std::string_view\{"((?:\\x[0-9a-f]{2})*)",(\d+)\}', literals)
            decoded = [decode_cpp_literal('"' + value + '"') for value, _ in fields]
            lengths = [int(length) for _, length in fields]
            self.assertEqual(int(native_id), case['native_id'])
            self.assertEqual(lengths, [len(value.encode('utf-8')) for value in (
                f"{case['native_profile']}|{case['id']}|{case['section']}",
                case['markdown'], case['html'])])
            self.assertEqual(decoded, [
                f"{case['native_profile']}|{case['id']}|{case['section']}",
                case['markdown'], case['html']])
            observed_ids.append(int(native_id))
            observed_cases.append(case['id'])
        self.assertEqual(len(set(observed_ids)), len(observed_ids))
        self.assertEqual(set(observed_cases), {case['id'] for case in self.cases})
        self.assertIn('static constexpr MarkdownCase gfm_all_cases[] = {', generated)

    def test_embedded_nul_has_explicit_utf8_byte_length_in_generated_view(self):
        source = '```ruby nul\x00nul\nmodule Foo\n```\n'
        case = {
            'native_id': 60003,
            'native_profile': 'full-info-option',
            'id': 'full-info-option:3',
            'section': 'full info',
            'markdown': source,
            'html': '<pre><code>nul\x00</code></pre>\n',
        }
        view = prepare_gfm.string_view(source)
        match = re.fullmatch(r'std::string_view\{"((?:\\x[0-9a-f]{2})*)",(\d+)\}', view)
        self.assertIsNotNone(match)
        self.assertEqual(int(match.group(2)), len(source.encode('utf-8')))
        self.assertEqual(decode_cpp_literal('"' + match.group(1) + '"'), source)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'nul.inc'
            prepare_gfm.generate(output, [case])
            generated = output.read_text(encoding='utf-8')
        self.assertIn(view, generated)

    def test_repository_spec_parser_matches_upstream_arrow_and_cr_rules(self):
        sample = (
            (prepare_gfm.FENCE + ' example disabled\n')
            + 'line one\r\r\n→\n'
            + '.\n<p>line one</p>\n'
            + prepare_gfm.FENCE + '\n'
        ).encode('utf-8')
        case, = prepare_gfm._parse_spec(
            sample, 'cmark-spec', 'spec.txt', 'commonmark', 'CC BY-SA 4.0')
        self.assertEqual(case['markdown'], 'line one\r\r\n\t\n')
        self.assertEqual(case['html'], '<p>line one</p>\n')
        self.assertTrue(case['upstream_disabled'])

    def test_missing_or_modified_pinned_sources_fail(self):
        source = next(row for row in prepare_gfm._lock()['sources']
                      if row['name'] == 'published.html')
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / source['name']).write_bytes(b'<html>modified</html>')
            with patch.object(prepare_gfm, 'CACHE', cache):
                with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
                    prepare_gfm._source(source['name'], fetch=False)


class PublishedHtmlParserTests(unittest.TestCase):
    def test_entities_space_spans_tabs_and_physical_line_endings_are_preserved(self):
        source = (
            '<h2><span class="number">6.9</span>Autolinks (extension)</h2>\r\n'
            '<div class="example" id="example-42">\r\n'
            '<div class="column"><pre><code class="language-markdown">'
            '&lt;x&amp;y&gt;<span class="space"> </span>a\tb\r\nc\n'
            '</code></pre></div>\r\n'
            '<div class="column"><pre><code class="language-html">'
            '&lt;p&gt;&amp;lt;x&amp;amp;y&amp;gt;<span class="space"> </span>a&amp;b&lt;/p&gt;\r\n'
            '</code></pre></div>\r\n'
            '</div>\r\n'
        )
        parser = prepare_gfm._PublishedPageParser()
        parser.feed(source)
        self.assertEqual(len(parser.cases), 1)
        case = parser.cases[0]
        self.assertEqual(case['id'], 'published-gfm:42')
        self.assertEqual(case['markdown'], '<x&y> a\tb\r\nc\n')
        self.assertEqual(case['html'], '<p>&lt;x&amp;y&gt; a&b</p>\r\n')
        self.assertEqual(case['source_lines'], {'start': 2, 'end': 8})


if __name__ == '__main__':
    unittest.main()
