from __future__ import annotations
import hashlib
import json
import re
import urllib.request
from html.parser import HTMLParser
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO_ROOT = Path(__file__).resolve().parents[2]
CACHE = REPO_ROOT / 'build' / 'dependencies' / 'markdown-conformance' / 'gfm'
LOCK_PATH = HERE / 'gfm-sources.lock.json'
EXT_SECTIONS = {
    'Tables (extension)',
    'Task list items (extension)',
    'Strikethrough (extension)',
    'Autolinks (extension)',
    'Disallowed Raw HTML (extension)',
}


def _lock() -> dict:
    return json.loads(LOCK_PATH.read_text(encoding='utf-8-sig'))


def _source(name: str, fetch: bool) -> bytes:
    lock = _lock()
    entry = next(source for source in lock['sources'] if source['name'] == name)
    path = CACHE / name
    expected = entry['sha256']
    if not path.exists():
        if not fetch:
            raise FileNotFoundError(f'{path} is missing; rerun with fetch=True')
        request = urllib.request.Request(entry['url'], headers={'User-Agent': 'Folia GFM conformance fixture loader'})
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read()
        actual = hashlib.sha256(data).hexdigest()
        if actual != expected:
            raise ValueError(f"{entry['url']}: SHA-256 mismatch: expected {expected}, got {actual}")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    data = path.read_bytes()
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected:
        raise ValueError(f'{path}: SHA-256 mismatch: expected {expected}, got {actual}')
    return data


class _PublishedPageParser(HTMLParser):
    """Extract paired code samples from the official published GFM page."""
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
                    'upstream_tags': [], 'upstream_disabled': False,
                    'source_markdown_raw': None,
                }
                self.current['source_lines'] = {'start': self.getpos()[0], 'end': None}
                self.example_depth = 1
        if tag == 'code' and self.current is not None:
            classes = (attributes.get('class') or '').split()
            if 'language-markdown' in classes:
                self.code_field = 'markdown'
            elif 'language-html' in classes:
                self.code_field = 'html'

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
                if self.current['markdown'] and self.current['html']:
                    self.current['source_lines']['end'] = self.getpos()[0]
                    self.cases.append(self.current)
                self.current = None

    def handle_data(self, data: str) -> None:
        if self.heading is not None:
            self.heading.append(data)
        if self.current is not None and self.code_field is not None:
            self.current[self.code_field] += data


def prepare(fetch: bool = False) -> list[dict]:
    """Return every current published GFM extension example in official page order."""
    page = _source('published.html', fetch).decode('utf-8')
    parser = _PublishedPageParser()
    parser.feed(page)
    cases = [case for case in parser.cases if case['section'] in EXT_SECTIONS]
    expected_ids = _lock()['published_example_ids']
    if [case['example'] for case in cases] != expected_ids:
        raise ValueError('Incomplete, duplicated or reordered published GFM extension fixtures')
    return cases


def load(fetch: bool = False) -> list[dict]:
    return prepare(fetch=fetch)



if __name__ == '__main__':
    import argparse
    from prepare import generate
    ap = argparse.ArgumentParser(description='Verify and extract published official GFM extension cases')
    ap.add_argument('--fetch', action='store_true')
    ap.add_argument('--output', type=Path)
    args = ap.parse_args()
    cases = prepare(args.fetch)
    if args.output:
        generate(args.output, cases, 'GFM 0.29 published extensions')
    print(f'Verified published GFM extensions: {len(cases)} cases')
