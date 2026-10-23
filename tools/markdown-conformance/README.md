# CommonMark conformance tests

`FoliaCommonMarkTests` runs the official CommonMark 0.31.2 examples as part of
Folia's C++23 Boost.UT test suite. This is a formal test target, separate from
the ordinary core and Windows editor suites. The fixture preparation step
checks the pinned `spec.json` SHA-256 and verifies that all 652 examples are
present in their official order before a run starts.

The C++ test emits an HTML stream from the parsed Markdown AST and lossless
inline CST. Markdown text and generated attributes are escaped; raw HTML is
passed through only from classified CST ranges or HTML-backed block sources.
The Python runner canonicalizes this stream and the immutable official expected
HTML with the same `canonical_html.py` implementation. This checks CommonMark
semantic output and is not Folia's WinUI HTML renderer. Native Boost.UT
assertions separately retain inline source losslessness and exact Markdown save
checks.
CTest invokes this same complete runner; launching only the native executable
checks source invariants but does not run the HTML semantic comparison.

## Run

From a Windows command prompt or PowerShell session with the Visual Studio C++
tools installed:

```bat
cmd /c build_commonmark_test.bat
```

The wrapper locates the latest Visual Studio installation with the required
C++ tools, verifies/fetches the official fixture, configures `build\core`,
builds only `FoliaCommonMarkTests`, then runs it. A normal run writes:

- `build\markdown-conformance\results.jsonl` — raw per-example records from
  the formal test executable.
- `build\markdown-conformance\results.json` — all 652 fixture rows, including
  missing/unreported cases, full actual and expected trees, first tree
  difference paths, exact save serialization, losslessness, and limitation
  details.
- `build\markdown-conformance\results.md` — summary by CommonMark section,
  strict differences, list-tightness diagnostic counts, and reported
  projection/implementation limitations.
- `build\markdown-conformance\runner.log` — captured standard output and
  error from the test executable.

The runner has a 120-second default timeout. These options can be passed
through the wrapper to `run.py`:

```bat
cmd /c build_commonmark_test.bat --output build\my-commonmark-report --timeout 180
cmd /c build_commonmark_test.bat "*some Boost.UT test name pattern*"
```

`--executable PATH` is also available when calling `run.py` directly. Any
remaining arguments are forwarded to Boost.UT, including positional name
patterns. Filtered runs are clearly marked in the JSON report; cases that were
not run remain unreported and are never counted as passing.

For direct use with an already-built executable:

```bat
python tools\markdown-conformance\prepare.py --fetch
python tools\markdown-conformance\run.py --executable build\core\FoliaCommonMarkTests.exe
```

The formal run returns 1 if either native source assertions or HTML semantic
assertions fail, including filtered runs. Missing or invalid full-suite records
also fail the run. The report retains `native_exit_code` separately. A timeout or
abnormal process exit stops evaluation, retains any partial JSONL records, and
marks the report incomplete.

## GitHub Flavored Markdown extension tests

Run `cmd /c build_gfm_test.bat` (CTest: `FoliaGfmTests`). Reports default to
`build/gfm-conformance/`; `--output` and positional Boost.UT filters work as above.
The five extension sections of the [published GFM spec](https://github.github.com/gfm/)
contain 28 examples. `prepare_gfm.py` reads its paired Markdown/HTML code samples,
preserves the published IDs and source lines, and verifies the page hash in
`gfm-sources.lock.json`. If the page changes, verification fails until its new
contents are reviewed. The older cmark-gfm repository spec and extra corpus are
listed separately in the lock; neither replaces the published examples.

The native test profile enables only GFM extensions over CommonMark. Tables,
alignments, checkboxes and inline formatting project from Folia's actual block
tree/CST into the official semantic comparison. Missing native syntax remains a
formal failure; the projector never parses bare source to invent that syntax.
`tests/parsing/test_gfm_native_projection.cpp` checks this mapping separately.

`gfm-policy.json` explicitly skips the HTML comparison for #657: Folia's inert
HTML presentation does not implement GitHub's tagfilter output spelling.
All 28 native source/CST/save assertions still execute. Skip counts and reasons
are separate from pass/fail/missing counts. A native error or source failure
still fails the run. Current implementation differences are recorded in
`docs/markdown-conformance.md`; the baseline is deliberately a failing formal
test, not a marked-success reference or disabled suite.

## md4c diagnostic reference

After the official suite completes with strict semantic failures, run the
pinned md4c comparison on just those failing example IDs:

```bat
cmd /c build_md4c_reference_test.bat
```

The wrapper checks the official report, fetches the hash-pinned md4c reference,
builds only `FoliaMd4cReferenceTests`, and writes results under
`build\markdown-conformance\md4c\`.

Each record preserves `match`, the formal md4c test's exact HTML string
comparison, and adds `semantic_match` using the same `canonical_html.py`
normalization as the official report. A raw formatting difference can make
`match` false while `semantic_match` remains true; the report labels this
`formatting_only` and includes a first semantic tree difference when one
exists. Exact and semantic totals are separate. The native exit code remains
unchanged, so exact HTML assertion failures can still return 1 even when the
canonical semantic comparison passes. A formatting-only HTML difference does
not show that the Markdown syntax violates CommonMark. md4c is an investigation
aid, not the CommonMark conformance criterion; the expected HTML comes from
the verified official report.

To refresh the JSON and Markdown summaries from an existing md4c JSONL file
without rerunning the executable:

```bat
python tools\markdown-conformance\reference.py --from-jsonl build\markdown-conformance\md4c\results.jsonl
```

The offline mode reuses the previous `results.json` exit code when available.

## Reading results

Strict semantic matches compare normalized HTML trees. The report counts the
optional list tight/loose paragraph projection separately; a case that fails
strict comparison and passes that diagnostic remains a strict failure. Inline
source losslessness and exact Markdown save serialization are reported as
separate checks. The serialization check compares the saved Markdown with the
original source; it does not reload the saved text. Strict differences are
placed into mutually exclusive categories: list-tightness only, an
HTML-policy/projection label, another reported projection limit, or no known
projection limit. The report includes each category's example IDs. Per-case
limitation labels are counted independently so a tree projection or
unsupported rendering detail is not automatically described as a parser
defect.

This comparison uses Folia's normalized test-tree projection. It does not test
the WinUI HTML renderer. The pinned md4c source and commit in
`sources.lock.json` are available for a later failure investigation only; md4c
is not used to define the expected output. The suite is a formal CommonMark
conformance test, but Folia has not yet been shown to achieve 100% conformance.

The official CommonMark `spec.json` fixture is licensed under
[CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/). The pinned
md4c reference files are licensed under the [MIT License](https://opensource.org/license/mit/).
Source URLs, versions, commits, and SHA-256 hashes are recorded in
`sources.lock.json`.

## Shared character data

Production syntax utilities use immutable Unicode 15.1 punctuation/symbol,
space-separator and full-casefold tables, plus the named HTML character-reference
data. Their generators verify the pinned inputs in `sources.lock.json` and embed
the input licenses/provenance in the generated `.inc` files:

```bat
python tools\markdown-conformance\generate_unicode.py --fetch
python tools\markdown-conformance\generate_entities.py
```

The entity generator uses the verified md4c `entity.c` data downloaded by the
reference preparation step. This reuses character mappings only; Folia does not
link the md4c parser in production. Numeric references follow CommonMark's Unicode
scalar rule rather than HTML's Windows-1252 control-character replacements.
The generated tables are reviewed source data, not build products.
