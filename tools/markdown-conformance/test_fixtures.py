"""Contract tests for the pinned CommonMark fixture preparation helpers."""
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

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
            output = Path(directory) / 'commonmark_cases.inc'
            prepare.generate(output, self.cases)
            generated = output.read_text(encoding='utf-8')

        rows = re.findall(r'^\{(\d+),((?:"(?:\\x[0-9a-f]{2})*"(?:,|(?=\})))+)', generated, re.M)
        self.assertEqual(len(rows), len(self.cases))
        self.assertEqual([int(example) for example, _ in rows], list(range(1, 653)))

        def decode_literals(encoded):
            literals = re.findall(r'"((?:\\x[0-9a-f]{2})*)"', encoded)
            return [bytes.fromhex(re.sub(r'\\x', '', literal)).decode('utf-8')
                    for literal in literals]

        for case, (example, encoded) in zip(self.cases, rows):
            self.assertEqual(int(example), case['example'])
            fields = decode_literals(encoded)
            self.assertEqual(len(fields), 3)
            self.assertEqual(fields[0], case['section'])
            self.assertEqual(fields[1], case['markdown'])
            self.assertEqual(fields[2], case['html'])

    def test_source_hash_rejects_modified_fixture_in_temporary_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / 'spec.json').write_text('[]', encoding='utf-8')
            with patch.object(prepare, 'DEPENDENCIES', cache):
                with self.assertRaisesRegex(RuntimeError, 'Source hash mismatch: spec.json'):
                    prepare.prepare()

    def test_cpp_literal_encodes_utf8_backslash_and_nul_as_exact_bytes(self):
        value = '雪\\\x00'
        self.assertEqual(prepare.literal(value), '"\\xe9\\x9b\\xaa\\x5c\\x00"')
        encoded = prepare.literal(value)[1:-1]
        self.assertEqual(bytes.fromhex(re.sub(r'\\x', '', encoded)), value.encode('utf-8'))


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
