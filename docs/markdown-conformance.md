# Markdown syntax conformance

## Current checkpoint (2026-10-23)

Full GitHub/cmark-gfm corpus integration and fixes are complete. No live agents,
builds, unresolved abnormal exits, temporary CMake hooks or pending candidates.
Canonical code is committed; next work should start from a new concrete failure,
not repeat the completed architecture or capacity investigations. No MSI, push or
version change in this task (version remains0.2.3).

Latest implementation commits:567a5bf7 (five shared leaf-source modules plus25
focused tests),32a553ee (callout source-prefix editing),2954da8c (one shared-source
container frame parser and integration regressions). Formal runner partial/filtered
report fixes are df0ff17f; corpus provenance is73032708.

### Final verified results

All native builds were sequential with **parallel4**. Final code, not an overlay:
- CommonMark0.31.2: **652/652 strict, lossless and exact save**.
  `build/markdown-conformance/frames-final/results.json`.
- GFM: **1395 strict pass /20 unscored /11 verified reference differences /
  0 unresolved**. All1426 exact input, source/CST and save checks pass.
  `build/gfm-conformance/frames-final/results.json`.
- Original generated parser capacities: **21pass /0fail /0not_run**,
  `build/gfm-conformance/frames-final-pathological.json`.
  Includes quotes50000=6.047s and lists1000=6.531s; original sizes/predicates
  unchanged, each process bounded30s and exactly one test matched.
- Core **738 tests /491735 assertions**, `core-frames-final.log`.
- Windows editor **80 tests /3429 assertions**, `platform-frames-final.log`.
- Last Python verification (tools unchanged afterward): main40 + isolated-runner11
  pass. Core includes all2125 named entities. Short log paths use
  `build/gfm-expanded/`. All final native runs exited0.

Full1426 corpus: published677, cmark-spec672 (incl2 upstream-disabled retained/run),
extensions30, regressions26, smart16, style1, full-info4. Pinned cmark-gfm commit
27d942c8b0a62d192f616e5bf3578f4b6a89e180. Qualified IDs, original Markdown/HTML,
hashes, licenses, profiles, tab provenance and embedded-NUL lengths retained.
11 reference differences are9 old nested-strong and2 old HTML-comment expectations
that conflict with pinned CommonMark0.31.2 for identical input/current rule. Original
strict_diff stays visible; these are neither strict passes nor skips.20 unscored
are16 optional smart typography,3 HTML tagfilter,1 upstream IGNORE/no HTML oracle;
their source/native checks still run. No failure is hidden by projection changes.

### Accepted implementation and verification boundaries

- Parser borrows one root UTF32 source/physical-line table. Flat container frames
  stage read-only line offers; accept commits once. Leaves materialize their own
  authoritative source once; inline CST finalizes once after definitions. No nested
  recursive Parser, per-frame body copies, per-character remap chains, depth cap,
  alternative model/parser or source-specific shortcut.
- Five helpers own physical source/indexing, logical leaf spans/accumulation,
  code/math source contexts, HTML source maps and final CST-based image promotion.
  Parser owns grammar/container transitions. Prefix binding uses final leaf owners
  without replacing each frame's syntax point. Lazy records commit only on accept.
- Terminal Item fragment EOL belongs to the container; read-only next-line/
  nonblank lookahead determines this before owner construction. Internal code EOL
  and quote-only code EOL remain owned. Default and explicit ownership registration
  use the same clipped end; consumed block ranges remain full. No post-close CST
  mutation/reparse or index patch-up. Tests cover CM5/6/7/273/274 and rendered code.
- Callout's first body retains absent header-gap metadata; source transforms generate
  quote prefixes through unwrap. Actual CR of a CRLF header stays at its established
  title source boundary. Enter, merge, unwrap, exact save and completion pass.
- BlockNode ownership/walk/index and serializer are iterative. Serializer facade,
  sole formatting visitor and line/event writer replace the recursive formatter.
  Earlier serializer parity2110tests/2132assertions and HTML parity4/25 passed;
  final parser21 capacity runs now cover the complete path too.
- Preserve the unified block tree, block-local authoritative source/lossless CST,
  one selection coordinate, stable untouched IDs, source/tree transactions and
  WYSIWYG. Initial numeric ID ordering is not invariant. Ordinary edits still
  perform zero full-document parses/serializations/tree diffs; clipboard single-line
  paste needs one fragment CST plus one edited-owner CST (2 reparses).

Final regressions repaired list padding/tab context, thematic rule/progress,
empty/sibling list boundaries, nested prefix anchors, table preflight distance,
callout editing/CRLF, and terminal list code source ownership. Earlier C3/C6 failures
and intermediate candidate manifests are historical, not outstanding work.

## Historical checkpoints

These are provenance only; resume from the current checkpoint above.
- 28 published extension examples: first baseline8 strict/19fail/1HTML skip;
  integration commits7cc3abc1/95300483. The expanded corpus supersedes that count.
- CommonMark HTML reached652/652 strict/source/save in c1d987f7, 3f7a930b,
  bf8a3d42. Earlier586/652 and source-save failures are resolved.
- Tab/lazy continuation and21 save gaps were fixed in e489a0b0/69f7a062.
  Prefix anchors, physical tab/residual-column mapping and exact undo metadata
  remain accepted. Do not restart those audits without new regression evidence.

## Accepted responsibilities

- `model/container_source.ixx`: prefix-only syntax records, attached optionally
  to existing quote/list-item payloads. Anchors identify an existing owner/source
  offset, a generated block Start/End, or a character offset in the existing
  separator field. They are not selection coordinates or cached body text.
- `parsing/parser.ixx`: stage physical prefixes over the borrowed root source;
  bind accepted records to durable anchors through the shared ownership index.
  Preserve lazy line context so #93's unmarked `===` stays literal paragraph text,
  and indented continuations in #238/#312 remain inside their open paragraph.
- `parsing/serializer.ixx` is the public facade; `serializer_visitor.ixx` owns
  formatting and `serializer_writer.ixx` resolves source/generated anchors to their
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

## Reproduction and provenance

Run `tests/parsing/run_commonmark.bat` and `tests/parsing/run_gfm.bat`; inspect official failures before running
`tests/parsing/run_md4c_reference.bat`. Relevant regression commands are `tests/run_core.bat`
and `tests/platform/run.bat`. Compare individual semantic/source/CST pass sets,
not just totals. Stop broad native runs after an abnormal process exit and
use a bounded focused test to diagnose it. Console tests link the repository's
console runtime so CRT reports stay in stderr.

Generated cases use the `FoliaGfmPathologicalTests` CMake target (parallel4), then:

```bat
python tools\markdown-conformance\run_gfm_pathological.py --executable build\core\FoliaGfmPathologicalTests.exe --report build\gfm-conformance\pathological.json --timeout-seconds 30
```

[CommonMark 0.31.2](https://spec.commonmark.org/0.31.2/) (CC BY-SA 4.0);
[md4c](https://github.com/mity/md4c), pinned to
f9e5eb232b95b737373ae706648cb75bd259ed1a (MIT). Fixtures, Unicode tables, licenses
and hashes are recorded in `tools/markdown-conformance/sources.lock.json` and its
README. No production md4c dependency was introduced.
