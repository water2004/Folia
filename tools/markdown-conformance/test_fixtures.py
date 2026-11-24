"""Contract tests for the pinned CommonMark fixture preparation helpers."""
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

import json
import fixture_table
import canonical_html
import prepare


class FixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = prepare.prepare()

    def test_official_examples_are_complete_ordered_and_preserved_in_generated_data(self):
        self.assertEqual(len(self.cases), 652)
        self.assertEqual([case['example'] for case in self.cases], list(range(1, 653)))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'commonmark.tsv'
            prepare.generate(output, self.cases)
            rows = fixture_table.read(output)
            committed = prepare.ROOT / 'tests/parsing/fixtures/conformance/commonmark.tsv'
            self.assertEqual(output.read_bytes(), committed.read_bytes())
        self.assertEqual(rows, [[case['example'], case['section'], case['markdown'], case['html']]
                                for case in self.cases])

    def test_source_hash_rejects_modified_fixture_in_temporary_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / 'spec.json').write_text('[]', encoding='utf-8')
            with patch.object(prepare, 'DEPENDENCIES', cache):
                with self.assertRaisesRegex(RuntimeError, 'Source hash mismatch: spec.json'):
                    prepare.prepare()

    def test_fixture_transport_encodes_utf8_backslash_and_nul_as_exact_bytes(self):
        value = '雪\\\x00'
        encoded = fixture_table.escape(value)
        self.assertNotIn('\x00', encoded)
        self.assertEqual(json.loads('"' + encoded + '"').encode('utf-8'), value.encode('utf-8'))

    def test_pinned_examples_remain_available_with_an_empty_download_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(prepare, 'DEPENDENCIES', Path(directory)):
                self.assertEqual(prepare.prepare(), self.cases)

    def test_missing_source_and_bundle_still_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(prepare, 'DEPENDENCIES', Path(directory) / 'cache'), \
                    patch.object(prepare, 'BUNDLED', Path(directory) / 'bundle'):
                with self.assertRaisesRegex(RuntimeError, 'Missing fixture'):
                    prepare.prepare()


class CanonicalHtmlTests(unittest.TestCase):
    def test_void_element_xhtml_spelling_is_equivalent(self):
        self.assertEqual(canonical_html.canonical('<p>a<br />b</p>'),
                         canonical_html.canonical('<p>a<br>b</p>'))

    def test_attribute_order_and_entities_are_normalized(self):
        left = '<a z="&amp;" a="&#x41;">x&amp;</a>'
        right = '<a a="A" z="&">x&</a>'
        self.assertEqual(canonical_html.canonical(left), canonical_html.canonical(right))

    def test_inline_and_code_whitespace_and_paragraph_trailing_newline_are_preserved(self):
        canonical = canonical_html.canonical
        self.assertNotEqual(canonical('<p><code>a  b</code></p>'),
                            canonical('<p><code>a b</code></p>'))
        self.assertNotEqual(canonical('<p>a  b</p>'), canonical('<p>a b</p>'))
        self.assertNotEqual(canonical('<p>tail\n</p>'), canonical('<p>tail</p>'))

    def test_emphasis_nesting_is_preserved(self):
        self.assertNotEqual(canonical_html.canonical('<em><strong>x</strong></em>'),
                            canonical_html.canonical('<strong><em>x</em></strong>'))

    def test_unicode_spaces_are_not_html_formatting_whitespace(self):
        self.assertNotEqual(canonical_html.canonical('<li>\u2003</li>'),
                            canonical_html.canonical('<li></li>'))

    def test_list_paragraph_unwrapping_is_only_enabled_in_relaxed_mode(self):
        tight = '<ul><li>x</li></ul>'
        loose = '<ul><li><p>x</p></li></ul>'
        self.assertNotEqual(canonical_html.canonical(tight), canonical_html.canonical(loose))
        self.assertEqual(canonical_html.canonical(tight, relax_list_paragraphs=True),
                         canonical_html.canonical(loose, relax_list_paragraphs=True))

    def test_different_link_text_or_url_remains_different(self):
        canonical = canonical_html.canonical
        baseline = canonical('<p><a href="/one">label</a></p>')
        self.assertNotEqual(baseline, canonical('<p><a href="/two">label</a></p>'))
        self.assertNotEqual(baseline, canonical('<p><a href="/one">other</a></p>'))


if __name__ == '__main__':
    unittest.main()
