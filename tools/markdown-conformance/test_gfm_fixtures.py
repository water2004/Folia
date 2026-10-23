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
import prepare

_spec = importlib.util.spec_from_file_location('prepare_gfm', TOOLS / 'prepare_gfm.py')
prepare_gfm = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
_spec.loader.exec_module(prepare_gfm)

EXPECTED_IDS = [198, 199, 200, 201, 202, 203, 204, 205, 279, 280,
                491, 492, 493, 622, 623, 624, 625, 626, 627, 628, 629,
                630, 631, 632, 633, 634, 635, 657]
EXPECTED_SECTIONS = {
    'Tables (extension)': 8,
    'Task list items (extension)': 2,
    'Strikethrough (extension)': 3,
    'Autolinks (extension)': 14,
    'Disallowed Raw HTML (extension)': 1,
}


def decode_cpp_literal(literal: str) -> str:
    if not (literal.startswith('"') and literal.endswith('"')):
        raise AssertionError(f'not a generated C++ string literal: {literal!r}')
    body = literal[1:-1]
    if re.search(r'(?<!\\)\\(?!x[0-9a-f]{2})', body):
        raise AssertionError(f'unexpected non-byte escape in generated literal: {literal!r}')
    return bytes.fromhex(re.sub(r'\\x', '', body)).decode('utf-8')


class PublishedFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cases = prepare_gfm.prepare()

    def test_all_official_extension_cases_are_paired_complete_and_in_page_order(self):
        self.assertEqual([case['example'] for case in self.cases], EXPECTED_IDS)
        counts = {}
        for case in self.cases:
            counts[case['section']] = counts.get(case['section'], 0) + 1
            self.assertIn(case['section'], EXPECTED_SECTIONS)
            self.assertIsInstance(case['markdown'], str)
            self.assertIsInstance(case['html'], str)
            self.assertTrue(case['markdown'])
            self.assertTrue(case['html'])
            self.assertGreater(case['source_lines']['start'], 0)
            self.assertGreaterEqual(case['source_lines']['end'], case['source_lines']['start'])
            self.assertIn('upstream_tags', case)
            self.assertFalse(case['upstream_disabled'])
        self.assertEqual(counts, EXPECTED_SECTIONS)

    def test_published_strikethrough_491_is_current_page_content(self):
        case = next(case for case in self.cases if case['example'] == 491)
        self.assertEqual(case['markdown'], '~~Hi~~ Hello, ~there~ world!\n')
        self.assertEqual(case['html'],
                         '<p><del>Hi</del> Hello, <del>there</del> world!</p>\n')

    def test_every_generated_cpp_record_decodes_to_its_exact_three_fixture_strings(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'gfm_cases.inc'
            prepare.generate(output, self.cases, 'GFM 0.29 published extensions')
            generated = output.read_text(encoding='utf-8')

        row_re = re.compile(
            r'^\{(\d+),((?:"(?:\\x[0-9a-f]{2})*")(?:,"(?:\\x[0-9a-f]{2})*")*)\},$',
            re.MULTILINE,
        )
        rows = row_re.findall(generated)
        self.assertEqual(len(rows), len(self.cases))
        for case, (example, literals) in zip(self.cases, rows):
            encoded_fields = re.findall(r'"(?:\\x[0-9a-f]{2})*"', literals)
            self.assertEqual(int(example), case['example'])
            self.assertEqual([decode_cpp_literal(value) for value in encoded_fields],
                             [case['section'], case['markdown'], case['html']])

    def test_missing_published_example_is_rejected(self):
        page = (prepare_gfm.CACHE / 'published.html').read_bytes()
        self.assertIn(b'id="example-657"', page)
        page = page.replace(b'id="example-657"', b'id="example-absent"', 1)
        with patch.object(prepare_gfm, '_source', return_value=page):
            with self.assertRaisesRegex(ValueError, 'Incomplete, duplicated or reordered'):
                prepare_gfm.prepare()

    def test_modified_cached_page_fails_its_pinned_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / 'published.html').write_bytes(b'<html>modified</html>')
            with patch.object(prepare_gfm, 'CACHE', cache):
                with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
                    prepare_gfm.prepare()


class PublishedHtmlParserTests(unittest.TestCase):
    def test_escaped_entities_space_spans_tabs_and_line_endings_are_preserved(self):
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
        self.assertEqual(case['example'], 42)
        self.assertEqual(case['section'], 'Autolinks (extension)')
        self.assertEqual(case['markdown'], '<x&y> a\tb\r\nc\n')
        self.assertEqual(case['html'], '<p>&lt;x&amp;y&gt; a&b</p>\r\n')
        self.assertEqual(case['source_lines'], {'start': 2, 'end': 8})


if __name__ == '__main__':
    unittest.main()
