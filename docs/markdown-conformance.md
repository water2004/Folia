# Markdown syntax conformance

## Current checkpoint (2026-10-23, GFM formal tests)

GFM extension **test integration and first full baseline are complete**. This
batch changes test infrastructure only; it does not claim GFM implementation
conformance or modify production parser/editor code.

The published [GitHub GFM 0.29 spec](https://github.github.com/gfm/) has **28**
extension examples: tables 8, task items 2, strikethrough 3, extended autolinks
14, tagfilter 1. `prepare_gfm.py` extracts the paired Markdown and expected HTML
code samples from the hash-pinned published HTML, preserving original IDs,
characters, whitespace, and page line ranges. It never reverse-generates
Markdown from rendered HTML. `gfm-sources.lock.json` records page SHA-256 and
licenses. The pinned cmark-gfm repository spec is older than the published page
(24 extension cases); that drift and its separate 30-case extra corpus are
recorded as provenance, not silently substituted for published expectations.

`tests/support/markdown_conformance.hpp` shares the native source checks between
CommonMark and GFM. Both use the same CST/AST-to-HTML projector and Python formal
runner. The projector now consumes native table alignments/cell CST and task-item
checked state. It cannot invent missing bare-autolink semantics. Generated input
uses one `MarkdownCase` shape, including the existing md4c test consumer.

Run `cmd /c build_gfm_test.bat`. CTest target `FoliaGfmTests` runs the same complete
native/Python path. Current report: `build/gfm-conformance/baseline/results.json`:

| Check | Result |
|---|---:|
| GFM strict semantics | **8 pass / 19 fail / 1 explicit HTML skip** |
| GFM inline CST losslessness | **28/28** |
| GFM character-exact save | **25/28** |
| CommonMark regression | **652/652**, all source checks also pass |
| Core regression | **601 tests / 44280 assertions**, exit 0 |
| Python fixture/runner contracts | **26 passed** |

Only published **#657, Disallowed Raw HTML**, skips the semantic comparison:
Folia's inert HTML presentation does not implement GitHub's tagfilter HTML
serialization. Its native CST/source/save assertions still run (focused #657
run confirms both assertions). `gfm-policy.json` is the explicit reviewed policy;
report records cannot declare their own skips. A skip is never counted as a
pass; parser errors and native failures remain failures.

Established implementation gaps (not skipped): escaped pipes in table code spans
#200; unpiped table continuation #202; task nesting #280; single/triple-tilde
rules #491/#493; all 14 extended autolinks #622–635. Save failures #199/#200/#204
normalize table row delimiters, padding or surplus cells. Full GFM run returns
normal exit 1, all 28 cases reported, no abnormal exit. Next implementation work:
fix exact table source ownership first, then these semantic failures, preserving
the existing block/source/CST/editing model. No production fixes started here.

Logs under `build/markdown-conformance/`: `gfm-baseline-final.log`, `gfm-core.log`,
`gfm-commonmark.log`, `gfm-ctest-commonmark.log`, `gfm-reference-build.log`.
CommonMark report is `gfm-commonmark-regression/results.json`. The md4c target
compiled successfully after the shared fixture shape change; it was not rerun.
No Windows/editor code changed; the prior 80-test Windows result below remains
historical. No live agents/builds. Build parallelism stays **4**. Commits:
**7cc3abc1** (shared runner and pinned published fixtures), **95300483** (GFM
target and native projections). A final focused 7-assertion check confirms the
projector leaves CommonMark plain-text URL CST as text; this check will remain
valid when GFM bare-autolink implementation is added.
No MSI/push/version change requested.

## CommonMark HTML checkpoint (2026-10-23, verified before GFM)

The HTML syntax/projection batch is implemented and committed. Formal
CommonMark: **652/652 strict semantics, 652/652 inline CST losslessness and
652/652 character-exact save**, normal exit 0. Report:
`build/markdown-conformance/html-final/results.json`. Baseline rerun at
`html-phase-start/` reproduced 586/652 before edits. No official expectations,
case omissions, or source-specific branches were introduced.

Shared `markdown_html` owns CommonMark block-start/end and inline-special
recognition; `parser` applies block boundaries before deriving the existing HTML
presentation. Complete supported HTML retains existing editable semantic nodes;
malformed/unsupported types 1–6 retain their established inert presentation,
bounded to the correct CommonMark block. Type 7 remains one existing Paragraph
in HtmlText mode. Inline HTML bodies retain their surrounding syntax mode:
Markdown inline tags allow Markdown children; raw HTML blocks remain HtmlText.

New regression evidence: `html_block_text_alignment_is_semantic_and_lossless`
contains four adjacent `<p>` roots without blank lines. Treating that whole
CommonMark block as plain HtmlText lost each paragraph's alignment. It now uses
the existing HtmlContainer/source-slot model with each semantic root as a child;
render tests verify independent child styles. For containers without element
provenance, the existing HTML semantic serializer emits their children rather
than inventing a `<div>` on Enter. Source edits/split/history tests cover this
case. This is not the rejected type-7 wrapper design: type 7 still has no added
container, outer links/styles stay on their existing CST/semantic owners, and
there is no extra source model or new serializer module.

Formal semantics now use **one** C++ Node projection that emits escaped Markdown
semantics and only parser-classified raw HTML source. Python `run.py` compares
that output using the existing `canonical_html.py`, also used by immutable
official expected HTML. Native Boost.UT retains independent lossless/save
assertions for all 652 cases; the Python stage is mandatory for strict semantic
assertions. CTest is wired through this complete runner. This avoids a second
HTML parser and handles HTML tags spanning multiple Markdown blocks.

Verification on the final implementation:
- Formal runner: **652/652 strict, 652/652 lossless, 652/652 exact save**, exit 0,
  `build/markdown-conformance/html-final/results.json` (all reported; no skips).
- CTest `FoliaCommonMarkTests`: passed through the complete Python/native runner,
  `html-ctest.log`; its per-case report is `build/core/markdown-conformance/`.
- Core: **598 tests / 44248 assertions**, exit 0, `html-core.log`.
- Windows editor: **80 tests / 3429 assertions**, exit 0, `html-platform.log`.
- Fixture/runner Python contracts: **17 tests**, passed. Invalid output records,
  missing HTML projections, altered report-side expectations, incomplete suites,
  per-case errors and filtered semantic mismatches cannot report success.

All logs above are under ignored `build/markdown-conformance/`. Old expectations
that disabled Markdown inside inline HTML were corrected; explicit HtmlText
checks still require literal Markdown. Added source edits/split/undo/redo checks
perform zero full-document parses, serializations and full-tree transaction diffs.

Commit checkpoint: scanner + scanner tests **c1d987f7**; inline syntax and formal
HTML comparison **3f7a930b**; block bounds and editable presentation **bf8a3d42**.
No live agents or builds. Build parallelism remains **4**. No official failures
remain; retain this verified state for the next user task. Ignored
`html-current/` deliveries are historical inputs, not replacements for reviewed
production code. No MSI, push or version change was requested in this batch.

## Previous verified checkpoint (2026-10-22, historical)

User priority 2/3 is verified: the known Tab/lazy-continuation failures and all
21 exact-save failures are fixed. Commits e489a0b0 (column context/layout) and
69f7a062 (container syntax and edit history) contain the implementation.
The formal regression tests accompanying this checkpoint complete the batch.
No live agents or native builds. Build parallelism remains **4**.

| Check | Initial baseline | Current |
|---|---:|---:|
| Strict CommonMark 0.31.2 semantics | 292/652 | **586/652** |
| Inline source/CST losslessness | 652/652 | **652/652** |
| Character-exact document serialization | 571/652 | **652/652** |

All 652 official cases ran, with no omissions, skips or abnormal exit. Against
phase-five-start/ (9a4ff577), new semantic passes are #5/#6/#7/#9/#93/#238/#312;
all 21 former save differences pass. No previously passing semantic, save or
inline-CST assertion regressed. Expected official trees were not weakened.

Next: resume priority 1, the remaining **66 semantic differences** involving HTML
syntax, block boundaries, supported presentation and test projection. Eighteen
carry HTML policy/projection labels; nine carry other extension/projection limits;
39 have no such label. These are not 66 separately established parser bugs.
Inspect each official failure first, then consult pinned md4c implementation.
The earlier UI high-CPU investigation is paused; the pre-existing Math handoff
edits are separate and were not included in these commits.

## Accepted responsibilities

- `model/container_source.ixx`: prefix-only syntax records, attached optionally
  to existing quote/list-item payloads. Anchors identify an existing owner/source
  offset, a generated block Start/End, or a character offset in the existing
  separator field. They are not selection coordinates or cached body text.
- `parsing/parser.ixx`: capture physical prefixes while nested input maps exist;
  bind them to durable anchors. Per-character physical maps are parse-time only.
  Preserve lazy line context so #93's unmarked `===` stays literal paragraph text,
  and indented continuations in #238/#312 remain inside their open paragraph.
- `parsing/serializer.ixx`: resolve source and generated syntax anchors to their
  current output lines. Reuse existing spelling; generate valid prefixes for new
  lines. Prefix lookup is indexed, not a repeated scan of each full paragraph.
  Blank-line separators have explicit ownership, including #320's final quote
  blank and CR/CRLF tight-list sibling gaps.
- `editing/document_container_source.ixx`: local anchor translation, split/join
  transfer and pruning when nodes leave a container. Existing transactions record
  source edits followed by exact payload updates; Undo/Redo never recalculates
  those offsets. Fresh IDs are remapped in paste and local block recognition.
- `BlockItemSpecial.marker` owns the list marker once. An ItemOpening prefix
  record stores only its following syntax suffix, so checkbox/number edits cannot
  replay an obsolete marker. A quote's first prefix now uses the same per-line
  records as later prefixes; the earlier first-only `opening_marker` workaround
  is superseded.
- `parsing/block_source.ixx`: physical tab-stop columns and residual columns are
  distinct from raw source offsets. Partial tabs never become invented source
  characters. Raw code stores line context; generated indentation belongs to its
  derived projection. Source operations snapshot contexts for exact history.
- Render model/builder/layout consume that same derived code indentation with
  empty source spans and explicit affinity. Source keys include generated columns.
  The already-maintained final block index supplies post-transaction queries;
  removed intermediate owners do not cause another full-document index scan.

The established block tree, block-local authoritative inline source/CST, selection
and transaction model remain in place. Ordinary edits do not serialize or parse
the full document or compute a full-tree transaction diff.

## Verification and limits

- Core: **581 tests / 43936 assertions**, exit 0. Includes bounded randomized
  source edit/delete/newline history, mixed prefixes, split/join, list outdent,
  literal lazy-line edits, raw-code column-context history, task/ordered markers,
  CR/CRLF, generated indentation layout/hit testing, and source-key invalidation.
- Windows editor: **80 tests / 3429 assertions**, exit 0.
- Remaining official subset against md4c: **66/66 semantic matches**; 64 exact HTML
  matches and two formatting-only differences (#175/#309). The exact-byte
  reference assertions return normal test-failure code 1, not an abnormal exit.
- Official suite returns normal test-failure code 1 for the remaining 66 HTML
  semantic differences. The application installer was not built in this batch.
- Fixture/generator Python checks previously passed 10 tests; this batch did not
  change those tools. Fixture hashes were checked by the official/reference runs.

Ignored `build/markdown-conformance/` contains current `results.{json,jsonl,md}`,
`prefix-{core,official,platform,md4c}.log`, and `md4c/results.json`.
Immutable comparison reports: `baseline/`, `phase-three-start/` (d5dfc16b),
`phase-four-start/` (c5d8da4c), `phase-five-start/` (9a4ff577).
Candidate deliveries under `candidates/prefix-phase/` are historical inputs:
production includes root fixes that candidates must never overwrite.

## Settled HTML decisions

The shared `parsing/markdown_html.ixx` scanner recognizes complete tag syntax and
source ranges, without body copies, decoding or presentation policy. Block and
inline parsers consume it. Complete unknown/closing tags stay opaque Raw CST
nodes; attribute brackets/delimiters cannot become Markdown links/emphasis.
Type-7 HTML blocks use one existing editable Paragraph in HtmlText mode, ending
at a blank line/EOF and never interrupting an open paragraph. Its source excludes
only the terminal physical EOL, owned by the existing document/separator syntax.
Recognized HTML CST elements/comments project directly in formal tests.

Rejected history: initial type-7 v1 duplicated tag grammar and body text. v2's
extra HTML containers/fragments lost outer anchor/style rendering because the
container renderer walks only children. Accepted v3 keeps existing HtmlText CST
semantics, with real Editor/render regression coverage. Do not restore v1/v2 or
add body copies. See Git through 9a4ff577 for prior batches. The separately tested
adjacent-root container case is recorded in the current checkpoint above.

## Reproduction and provenance

Run `build_commonmark_test.bat`; inspect official failures, then run
`build_md4c_reference_test.bat`. Relevant regression commands are `build_test.bat`
and `build_platform_test.bat`. Compare individual semantic/source/CST pass sets,
not just totals. Stop broad native runs after an abnormal process exit and
use a bounded focused test to diagnose it. Console tests link the repository's
console runtime so CRT reports stay in stderr.

[CommonMark 0.31.2](https://spec.commonmark.org/0.31.2/) (CC BY-SA 4.0);
[md4c](https://github.com/mity/md4c), pinned to
f9e5eb232b95b737373ae706648cb75bd259ed1a (MIT). Fixtures, Unicode tables, licenses
and hashes are recorded in `tools/markdown-conformance/sources.lock.json` and its
README. No production md4c dependency was introduced.
