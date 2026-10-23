"""Verify and prepare the published GFM page and pinned cmark-gfm test corpora."""
from __future__ import annotations

import hashlib
import importlib.util
import io
import json
import re
import urllib.request
from html.parser import HTMLParser
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = Path(__file__).resolve().parents[2]
CACHE = REPO_ROOT / 'build' / 'dependencies' / 'markdown-conformance' / 'gfm'
LOCK_PATH = HERE / 'gfm-sources.lock.json'
FENCE = chr(96) * 32

SPEC_CORPORA = (
    ('cmark-spec', 'spec.txt', 'commonmark', 'CC BY-SA 4.0'),
    ('extensions', 'extensions.txt',
     'table strikethrough autolink tagfilter footnotes tasklist', 'CC BY-SA 4.0'),
    ('smart-punctuation', 'smart_punct.txt', 'smart', 'BSD-2-Clause'),
    ('regressions', 'regression.txt', '', 'BSD-2-Clause'),
    ('table-style-option', 'extensions-table-prefer-style-attributes.txt',
     'table table-prefer-style-attributes', 'CC BY-SA 4.0'),
    ('full-info-option', 'extensions-full-info-string.txt',
     'full-info-string', 'CC BY-SA 4.0'),
)
NATIVE_ID_BASE = {
    'cmark-spec': 10000,
    'extensions': 20000,
    'smart-punctuation': 30000,
    'regressions': 40000,
    'table-style-option': 50000,
    'full-info-option': 60000,
}


def _lock() -> dict:
    return json.loads(LOCK_PATH.read_text(encoding='utf-8-sig'))


def _commonmark_cases(fetch: bool) -> list[dict]:
    spec = importlib.util.spec_from_file_location('gfm_reference_commonmark_prepare', HERE / 'prepare.py')
    if spec is None or spec.loader is None:
        raise RuntimeError('Cannot load the pinned CommonMark fixture verifier')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.prepare(fetch=fetch)


def _source(name: str, fetch: bool) -> bytes:
    lock = _lock()
    entry = next(source for source in lock['sources'] if source['name'] == name)
    path = CACHE / name
    expected = entry['sha256']
    if not path.exists():
        if not fetch:
            raise FileNotFoundError(f'{path} is missing; rerun with fetch=True')
        request = urllib.request.Request(
            entry['url'], headers={'User-Agent': 'Folia GFM conformance fixture loader'})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected:
            raise ValueError(
                f"{entry['url']}: SHA-256 mismatch: expected {expected}, got {actual}")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    data = path.read_bytes()
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected:
        raise ValueError(f'{path}: SHA-256 mismatch: expected {expected}, got {actual}')
    return data


class _PublishedPageParser(HTMLParser):
    """Extract every paired Markdown and HTML sample from the published page."""
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.section = ''
        self.heading: list[str] | None = None
        self.current: dict | None = None
        self.example_depth = 0
        self.code_field: str | None = None
        self.cases: list[dict] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = dict(attrs)
        if tag == 'h2':
            self.heading = []
        if self.current is not None and tag == 'div':
            self.example_depth += 1
        if tag == 'div' and 'example' in (attributes.get('class') or '').split() and self.current is None:
            match = re.fullmatch(r'example-(\d+)', attributes.get('id') or '')
            if match:
                self.current = {
                    'example': int(match.group(1)), 'section': self.section,
                    'markdown': '', 'html': '', 'source_lines': None,
                    'markdown_seen': False, 'html_seen': False,
                    'upstream_tags': [], 'upstream_disabled': False,
                    'expected_html_ignore': False,
                }
                self.current['source_lines'] = {'start': self.getpos()[0], 'end': None}
                self.example_depth = 1
        if tag == 'code' and self.current is not None:
            classes = (attributes.get('class') or '').split()
            if 'language-markdown' in classes:
                self.code_field = 'markdown'
                self.current['markdown_seen'] = True
            elif 'language-html' in classes:
                self.code_field = 'html'
                self.current['html_seen'] = True

    def handle_endtag(self, tag: str) -> None:
        if tag == 'code':
            self.code_field = None
        if tag == 'h2' and self.heading is not None:
            title = ''.join(self.heading)
            self.section = re.sub(r'^\s*\d+(?:\.\d+)*\s*', '', title).strip()
            self.heading = None
        if self.current is not None and tag == 'div':
            self.example_depth -= 1
            if self.example_depth == 0:
                if self.current['markdown_seen'] and self.current['html_seen']:
                    markdown_source = self.current['markdown']
                    html_source = self.current['html']
                    self.current['source_markdown_raw'] = markdown_source
                    self.current['source_html_raw'] = html_source
                    self.current['markdown'] = markdown_source.replace('→', '\t')
                    self.current['html'] = html_source.replace('→', '\t')
                    self.current['fixture_transformations'] = (
                        ['U+2192 right-arrow marker converted to U+0009 TAB']
                        if '→' in markdown_source + html_source else [])
                    self.current['source_lines']['end'] = self.getpos()[0]
                    self.current['corpus'] = 'published-gfm'
                    self.current['id'] = f"published-gfm:{self.current['example']}"
                    self.current['native_id'] = self.current['example']
                    self.current['source_path'] = 'https://github.github.com/gfm/'
                    self.current['license'] = 'CC BY-SA 4.0'
                    extension_by_section = {
                        'Tables (extension)': 'table',
                        'Task list items (extension)': 'tasklist',
                        'Strikethrough (extension)': 'strikethrough',
                        'Autolinks (extension)': 'autolink',
                        'Disallowed Raw HTML (extension)': 'tagfilter',
                    }
                    extension = extension_by_section.get(self.current['section'])
                    self.current['native_profile'] = (
                        'published-gfm+' + extension if extension else 'published-gfm')
                    self.current['native_interface_gaps'] = (
                        ['tagfilter HTML output option']
                        if self.current['example'] == 657 else [])
                    self.current['upstream_extensions'] = []
                    self.current['default_extensions'] = []
                    self.current['upstream_extensions'] = [extension] if extension else []
                    self.current['expected_html_ignore'] = (
                        self.current['html'].strip() == '<IGNORE>')
                    self.cases.append(self.current)
                self.current = None

    def handle_data(self, data: str) -> None:
        if self.heading is not None:
            self.heading.append(data)
        if self.current is not None and self.code_field is not None:
            self.current[self.code_field] += data


def _parse_spec(data: bytes, corpus: str, filename: str,
                default_extensions: str, license_name: str) -> list[dict]:
    # Upstream opens specs with newline='\\n', preserving intentional CR bytes
    # inside examples, then maps the visible right-arrow marker to a tab.
    text = data.decode('utf-8')
    lines = io.StringIO(text, newline='\n')
    section = ''
    state = 'prose'
    markdown: list[str] = []
    html: list[str] = []
    extensions: list[str] = []
    start_line = 0
    example = 0
    cases: list[dict] = []
    for line_number, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith(FENCE + ' example'):
            state = 'markdown'
            extensions = stripped[len(FENCE + ' example'):].split()
            markdown, html = [], []
            start_line = 0
        elif stripped == FENCE and state in ('markdown', 'html'):
            state = 'prose'
            example += 1
            html_source = ''.join(html)
            disabled = 'disabled' in extensions
            profile = corpus
            case_extensions = list(extensions)
            if corpus == 'cmark-spec' and not case_extensions:
                section_extension = {
                    'Tables (extension)': 'table',
                    'Task list items (extension)': 'tasklist',
                    'Strikethrough (extension)': 'strikethrough',
                    'Autolinks (extension)': 'autolink',
                    'Disallowed Raw HTML (extension)': 'tagfilter',
                }.get(section)
                if section_extension:
                    case_extensions = [section_extension]
            if disabled and corpus == 'cmark-spec':
                case_extensions = ['tasklist']
            if corpus in ('cmark-spec', 'regressions') and case_extensions:
                profile += '+' + '+'.join(sorted(
                    value for value in case_extensions if value != 'disabled'))
            elif corpus == 'cmark-spec' and disabled:
                profile += '+tasklist'
            elif corpus == 'extensions':
                # test/CMakeLists.txt enables the complete extension set globally.
                profile = 'extensions'
            elif corpus == 'table-style-option':
                profile = 'table-style-option+table'
            gaps = {'smart-punctuation': ['smart-punctuation output option']}.get(corpus, [])
            if corpus == 'extensions' and re.search(
                    r'<\s*/?\s*(?:title|textarea|style|xmp|iframe|noembed|noframes|script|plaintext)\b',
                    ''.join(markdown), re.IGNORECASE):
                gaps.append('tagfilter HTML output option')
            if 'tagfilter' in case_extensions:
                gaps = [*gaps, 'tagfilter HTML output option']
            cases.append({
                'id': f'{corpus}:{example}',
                'example': example,
                'native_id': NATIVE_ID_BASE[corpus] + example,
                'corpus': corpus,
                'source_path': f'test/{filename}',
                'source_lines': {'start': start_line, 'end': line_number},
                'section': section,
                'source_markdown_raw': ''.join(markdown),
                'source_html_raw': html_source,
                'markdown': ''.join(markdown).replace('→', '\t'),
                'html': html_source.replace('→', '\t'),
                'fixture_transformations': (
                    ['U+2192 right-arrow marker converted to U+0009 TAB']
                    if '→' in ''.join(markdown) + html_source else []),
                'upstream_extensions': extensions,
                'upstream_disabled': disabled,
                'enabled_upstream': not disabled,
                'expected_html_ignore': html_source.strip() == '<IGNORE>',
                'license': license_name,
                'native_profile': profile,
                'native_interface_gaps': gaps,
                'default_extensions': default_extensions.split(),
                'official_expectation_source': (
                    'the pinned cmark-gfm executable test recipe in test/CMakeLists.txt'),
            })
            markdown, html, extensions = [], [], []
        elif stripped == '.' and state == 'markdown':
            state = 'html'
        elif state == 'markdown':
            if not start_line:
                start_line = line_number - 1
            markdown.append(line)
        elif state == 'html':
            html.append(line)
        elif re.match(r'^#+ ', line):
            section = re.sub(r'^#+\s*', '', line).strip()
    if state != 'prose':
        raise ValueError(f'unclosed example in test/{filename}')
    return cases


def prepare(fetch: bool = False) -> list[dict]:
    """Return every paired case, retaining each source corpus separately."""
    page = _source('published.html', fetch).decode('utf-8')
    parser = _PublishedPageParser()
    parser.feed(page)
    lock = _lock()
    published = parser.cases
    expected_range = lock['published_example_ids_all']
    if [case['example'] for case in published] != expected_range:
        raise ValueError('Incomplete, duplicated or reordered published GFM examples')

    cases = list(published)
    for corpus, filename, extensions, license_name in SPEC_CORPORA:
        raw = _source(filename, fetch)
        extracted = _parse_spec(raw, corpus, filename, extensions, license_name)
        expected_count = lock['counts']['repository_corpora'][corpus]
        if len(extracted) != expected_count:
            raise ValueError(
                f'{filename}: expected {expected_count} examples, found {len(extracted)}')
        if [case['example'] for case in extracted] != list(range(1, expected_count + 1)):
            raise ValueError(f'{filename}: original example IDs are incomplete or reordered')
        cases.extend(extracted)

    _verify_reviewed_reference_differences(cases, fetch)

    expected_total = lock['counts']['expanded_fixture_cases']
    ids = [case['id'] for case in cases]
    native_ids = [case['native_id'] for case in cases]
    if len(cases) != expected_total or len(set(ids)) != len(ids):
        raise ValueError('Expanded GFM fixture set is incomplete or has duplicate corpus IDs')
    if len(set(native_ids)) != len(native_ids):
        raise ValueError('Expanded GFM native case IDs collide')
    if sum(case['upstream_disabled'] for case in cases) != lock['counts']['upstream_disabled_retained']:
        raise ValueError('Upstream-disabled fixture count changed')
    return cases


def _verify_reviewed_reference_differences(cases: list[dict], fetch: bool) -> None:
    policy = json.loads((HERE / 'gfm-policy.json').read_text(encoding='utf-8'))
    reviewed = policy.get('reviewed_reference_differences', {})
    by_id = {case['id']: case for case in cases}
    if not set(reviewed).issubset(by_id):
        raise ValueError('Reviewed reference difference points to a missing fixture')
    commonmark_by_id = None
    commonmark_by_markdown = None
    if any(entry.get('kind') == 'current-commonmark-reference' for entry in reviewed.values()):
        commonmark = _commonmark_cases(fetch)
        commonmark_by_id = {case['example']: case for case in commonmark}
        commonmark_by_markdown = {case['markdown']: case for case in commonmark}
    for case_id, entry in reviewed.items():
        fixture = by_id[case_id]
        if entry.get('kind') == 'current-commonmark-reference':
            reference = commonmark_by_id.get(entry.get('commonmark_example'))
            exact_source = commonmark_by_markdown.get(fixture['markdown'])
            if (reference is None or exact_source is None
                    or exact_source['example'] != reference['example']
                    or exact_source['html'] != entry.get('current_expected_html')
                    or exact_source['markdown'] != fixture['markdown']):
                raise ValueError(f'{case_id}: pinned CommonMark reference no longer matches policy')
        elif entry.get('kind') == 'current-commonmark-html-comment-reference':
            if (fixture['markdown'] != entry.get('current_expected_markdown')
                    or not entry.get('source_url', '').endswith('#html-comment')):
                raise ValueError(f'{case_id}: CommonMark HTML-comment policy source changed')
        else:
            raise ValueError(f'{case_id}: unknown reviewed reference difference kind')
        if fixture['html'] == entry.get('current_expected_html'):
            raise ValueError(f'{case_id}: reviewed references must identify a real expectation difference')
        fixture['reviewed_reference_difference'] = entry


def native_groups(cases: list[dict]) -> dict[str, list[dict]]:
    groups: dict[str, list[dict]] = {}
    for case in cases:
        groups.setdefault(case['native_profile'], []).append(case)
    return groups


def literal(value: str) -> str:
    return '"' + ''.join(f'\\x{byte:02x}' for byte in value.encode('utf-8')) + '"'


def string_view(value: str) -> str:
    """Emit byte-counted views so embedded U+0000 is never C-string-truncated."""
    return f'std::string_view{{{literal(value)},{len(value.encode("utf-8"))}}}'


def generate(path: Path, cases: list[dict]) -> None:
    output = [
        '// Generated from hash-verified published GFM and cmark-gfm sources.',
        'static constexpr MarkdownCase gfm_all_cases[] = {',
    ]
    for case in cases:
        section = f"{case['native_profile']}|{case['id']}|{case['section']}"
        values = [section, case['markdown'], case['html']]
        output.append('{' + str(case['native_id']) + ',' + ','.join(map(string_view, values)) + '},')
    output.extend(['};', ''])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(output), encoding='utf-8')


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--fetch', action='store_true')
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    cases = prepare(fetch=args.fetch)
    if args.output:
        generate(args.output, cases)
    from collections import Counter
    print(f'Verified pinned GFM fixtures: {len(cases)} cases')
    for corpus, count in sorted(Counter(case['corpus'] for case in cases).items()):
        print(f'  {corpus}: {count}')
