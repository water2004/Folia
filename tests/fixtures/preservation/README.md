# Test and fixture preservation records

These records protect different parts of the portable test refactor. They are
checked in alongside the retained data, not generated during ordinary tests.

| File | Scope |
| --- | --- |
| `registered-cases.json` | The master baseline's 2,811 literal Boost.UT registrations with multiplicity, all 120 historical Windows-runner test/support hashes, and two exact portable import additions |
| `fixture-integrity.json` | The current portable data inventory: exact byte sizes, SHA-256 values and TSV record counts |
| `math-fixture-migrations.json` | Nineteen retained numbered migration reports, with original/current file hashes and the verification results recorded when those migrations were performed |

## Routine checks

From the repository root:

```sh
python tests/tools/check_inventory.py
python tests/tools/check_fixtures.py
python -m unittest discover -s tests/tools -p test_fixtures.py
```

The `tooling` suite includes both preservation test modules. The optional
`python tests/tools/check_inventory.py --windows-baseline` audit verifies the
Windows files recorded for this refactor without building the Windows SDK tests.
Registration checks preserve repeated test names as repeated registrations;
corpus records are checked separately by their readers and the fixture guard.
When behavior changes, `replaced_by` can name the current regression that
replaces a historical test. The original name and file remain recorded.

The 120-file baseline includes SDK-free policy tests historically run by the
Windows runner. The audit retains every original hash: 118 files are unchanged
under its existing CRLF-to-LF policy, and the LaTeX/snippet session tests each
have one explicit module import added for portable compilation.
`portable_import_additions` names those two paths and exact import lines. Each
line must occur exactly once; only that line is removed before comparing the
remaining source to its original hash. This checks all original assertions,
inputs, helpers and registration text without accepting other edits. Windows
SDK test files remain unchanged.

## Current byte checkpoint

The guard discovers data under portable `tests/**/fixtures/`,
`tests/mermaid_regression/cases/`, and `tools/**/tests/fixtures/`. It excludes the
SDK suites (`app`, `platform`, `encoding`), code, READMEs and this preservation
directory. The frozen upstream Markdown corpus remains data even where its
original filename ends in `.inc`.

The inventory is sorted by repository-relative path. Each entry records the
unmodified file bytes; TSV counts exclude blank and comment lines just as the
shared native reader does. Exact hashes also cover comments, record order,
trailing whitespace, CR bytes and binary raster observations. New or missing
files, altered rows, invalid inventory shapes, duplicate paths/JSON keys and
noncanonical or symlink paths fail visibly. No path is silently substituted.

`.gitattributes` fixes portable fixture text to LF across checkouts. The frozen
upstream Markdown files preserve physical CR and significant whitespace with
`-text`; PNG and RGBA observations also remain binary. The guard does not
normalize those bytes before comparing them.

After deliberately updating a fixture from its independent source, review the
data and run its consumers before refreshing the checkpoint:

```sh
python tests/tools/check_fixtures.py --refresh
python tests/tools/check_fixtures.py
```

Commit the refreshed checkpoint with the reviewed fixture update. Refreshing
records current bytes; it does not establish that changed expectations are
correct, preserve earlier test coverage or make a failing candidate pass.

## Historical migration evidence

The numbered reports retain the original field-by-field comparisons, counts and
source-serializer results. During their consolidation, all 43 recorded original
blobs were checked against master commit
`2ba389aa34a5c2f23500dc552b9fab9c434a0cdd`, and all 133 recorded current fixture
files were rehashed, with zero mismatches. The compiler comparisons and official
serializer runs were not repeated by that consolidation.

That file covers the nineteen available numbered reports only. Later package,
scene and metadata migrations also preserve their source provenance and schemas
with their fixtures; the numbered-report file is not an exhaustive proof of the
whole refactor. Current byte integrity, historical migration evidence and native
behavior tests are complementary checks with separate scopes.
