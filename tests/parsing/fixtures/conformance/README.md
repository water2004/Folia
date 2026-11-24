# Official Markdown observations

These files are independent expected data, never generated from Folia output.
The ordinary tests read the committed tables; they do not compile thousands of
string literals or download fixtures during a build.

| Table | Records | Preserved identity |
| --- | ---: | --- |
| `commonmark.tsv` | 652 | CommonMark 0.31.2 example numbers 1–652 |
| `gfm.tsv` | 1,426 | Published GFM plus six cmark-gfm corpora, with all original corpus IDs and native profile IDs |

Each table has four tab-separated columns: integer example ID, section/profile,
Markdown, and expected HTML. String fields use JSON string escaping without the
surrounding quotes. Literal Unicode remains UTF-8; backslashes, tabs, physical CR,
LF and embedded NUL are escaped. Empty strings remain empty columns. The native
reader owns the decoded strings and rejects missing, malformed, duplicate-ID or
incomplete data. No HTML canonicalization happens during fixture loading.

## Provenance and offline verification

`upstream/commonmark/spec.json` is the unchanged official CommonMark file.
`upstream/gfm/` contains the unchanged published page, cmark-gfm source examples,
entity table, and upstream copyright/license notice. Every source is verified
against the existing SHA-256 entries in
`tools/markdown-conformance/sources.lock.json` or `gfm-sources.lock.json`.
Git treats these raw source files as bytes because their physical line endings
and trailing spaces can be part of an example.

CommonMark and the published GFM/specification examples are under CC BY-SA 4.0;
the cmark-gfm test sources retain their per-corpus licenses and `upstream/gfm/COPYING`.
The exact source URLs, versions and revision are in those lock files. Generated
tables preserve the upstream inputs and expectations without changing their
meaning. GFM profiles, disabled upstream examples, deliberate `<IGNORE>` records,
and reviewed reference differences retain their existing handling.

The Python verifier first checks an existing download cache, including rejecting
a corrupted cache. If the cache is absent it uses these committed, hash-verified
source files. Optional md4c reference-source downloads still require `--reference`.

## Deliberate regeneration

From the repository root:

```sh
python tools/markdown-conformance/prepare.py --output tests/parsing/fixtures/conformance/commonmark.tsv
python tools/markdown-conformance/prepare_gfm.py --output tests/parsing/fixtures/conformance/gfm.tsv
python -m unittest discover -s tools/markdown-conformance -p 'test_*.py'
python tests/run.py --suite conformance
```

The tool tests compare every decoded field, original ordering, complete ID set,
and regenerated table bytes. `conformance` also runs the bounded parser and
serializer stress cases; each pathological input retains its own process limit.
