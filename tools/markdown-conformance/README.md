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
cmd /c tests\parsing\run_commonmark.bat
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
cmd /c tests\parsing\run_commonmark.bat --output build\my-commonmark-report --timeout 180
cmd /c tests\parsing\run_commonmark.bat "*some Boost.UT test name pattern*"
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

## GitHub Flavored Markdown tests

Run cmd /c tests\parsing\run_gfm.bat (CTest: FoliaGfmTests). Reports default to
build/gfm-conformance/; --output and positional Boost.UT filters work as above.
The formal runner checks 1,426 paired cases from separate sources:

| Corpus | Cases | Native mode |
|---|---:|---|
| All paired examples on the published GFM page | 677 | CommonMark baseline; each GFM extension section enables only that extension |
| Pinned test/spec.txt | 672 | CommonMark baseline; extension examples follow their explicit tags or upstream section; two upstream-disabled task-list examples run with task lists enabled |
| Pinned test/extensions.txt | 30 | GFM extensions plus footnotes |
| Pinned test/smart_punct.txt | 16 | CommonMark mode; upstream --smart HTML typography option is an explicit semantic skip |
| Pinned test/regression.txt | 26 | CommonMark; per-example extension tags are honored |
| Pinned table style-attribute option spec | 1 | Tables; test projection emits the upstream `style` alignment attributes |
| Pinned full-info-string option spec | 4 | CommonMark; test projection uses the preserved code info string for `data-meta` |

The published page retains all IDs 1–677, including examples 176 and 188 whose
expected HTML is empty. The repository's 672 cases remain a distinct corpus.
Each record has a corpus-qualified ID, original numeric ID, source path and line
range, source strings, upstream enable/disable tags, license, and native profile.
The native executable uses collision-free numeric IDs; reports restore the
original IDs and provenance. Arrows in repository specs map to tabs as upstream
does, and intentional CR characters are preserved. Files are SHA-256 locked in
gfm-sources.lock.json.

The pinned repository also registers C API, normalizer, pathological, entity,
roundtrip, fuzz, and corpus-option tests. gfm-sources.lock.json inventories the
complete test/ and api_test/ trees and exact CTest registration. The
pathological suite contains 21 generated adversarial inputs; entity coverage
contains 2,125 API lookups; API lifecycle tests and fuzz inputs have no paired
HTML oracle, so the formal HTML runner reports them as separate migration
targets rather than inventing expected output. Roundtrip tests reuse the
extension source cases and add a Markdown serialization step.

The published page's baseline examples and the upstream CommonMark baseline
remain CommonMark; extension mode is enabled per section or upstream example
tag. This preserves the published non-extension autolink examples' historical
CommonMark interpretation. Smart punctuation is an optional HTML typography
mode; all 16 exact upstream expectations remain in the report as explicit
semantic skips because Folia does not apply `--smart` output substitutions.
Table alignment style attributes and full-info-string `data-meta` are handled
by test projection options over the existing AST and preserved block-source
ranges. The three tagfilter examples (`published-gfm:657`, `cmark-spec:652`,
`extensions:22`) are explicit HTML presentation skips because Folia does not
serialize GitHub's tagfilter output. `extensions:20` is also an explicit skip
because upstream supplies no HTML oracle for that case. All native
source/CST/save assertions still execute for every skipped case. Upstream-
disabled inputs and `<IGNORE>` expectations remain visible in reports.

The nine reviewed nested-strong cases and published examples 649–650 retain
their pinned GFM expectations and remain strict differences. The policy also
records their separate CommonMark 0.31.2 reference expectations. For the nine
strong-emphasis cases, the loader verifies exact Markdown and expected HTML
against the hash-locked CommonMark examples. The malformed-comment references
use the pinned CommonMark HTML-comment rule and explicit expected HTML. A
`reference_difference_verified` result is neither a strict pass nor a skip: it
means the actual projection matched that reviewed current reference and native
input, lossless-source, and exact-save invariants passed. Every other mismatch
remains an unresolved difference and fails the formal runner. Reports retain
the original GFM expected HTML and strict-difference row.

## md4c diagnostic reference

After the official suite completes with strict semantic failures, run the
pinned md4c comparison on just those failing example IDs:

```bat
cmd /c tests\parsing\run_md4c_reference.bat
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
