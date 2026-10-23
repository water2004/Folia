# Markdown syntax conformance

## Current checkpoint (2026-10-23)

User requested the full finer GitHub tests and fixes. Formal corpora are integrated;
two full-size generated parser capacity cases remain. Preserve unified block tree,
block-local authoritative source/lossless CST, selection/history and WYSIWYG.
No second parser/model, depth cap, source-specific fast path or runtime dual path.

### Verified commits and results

Starting984558a4. Commits5f455590 (syntax/source consumers),73032708 (full pinned
corpora),d3e25b1c (inline scopes/scans/RAII),dc50d694 (tables/ranges),9ccb4185
(derived NUL display),4feba6e9 (checkpoint),edc6be39 (iterative BlockNode ownership/
walk/index),cd9bade5 (recognizer-local completion),**0eeab9f4** (iterative serializer).
No MSI/push/version change. Only root builds, sequentially **parallel4**; agents
write ignored candidates only. Link console runtime. After abnormal native exit,
stop broad evaluation and use bounded focus; no outstanding abnormal exit now.

Full GFM corpus **1426**: published677, cmark-spec672 (incl2 upstream-disabled
retained/run), extensions30, regressions26, smart16, style-option1, full-info4.
Pinned cmark-gfm27d942c8b0a62d192f616e5bf3578f4b6a89e180. Qualified IDs, original
Markdown/HTML, hashes/license/profile/tab provenance and NUL byte lengths retained.
Latest all exit0 after0eeab9f4:
- GFM **1395 strict pass /20 unscored /11 verified reference differences /
  0 unresolved**; all1426 exact input/source/save checks pass.
  `build/gfm-conformance/serializer/results.json`.
- Reference differences:9 old nested-strong and2 old HTML-comment expectations
  conflict pinned CommonMark0.31.2 for identical input/current rule. Original
  expected HTML and strict_diff remain visible; neither pass nor skip.
- Unscored:16 optional smart typography,3 tagfilter,1 upstream IGNORE/no HTML
  oracle. Source/native checks still run; no implementation failure is hidden.
- CM **652/652** strict/source/save,
  `build/markdown-conformance/serializer/results.json`.
- Core **672 tests /491236 assertions**, `build/gfm-expanded/core-serializer.log`.
- Windows **80 tests /3429 assertions**, `platform-serializer.log`.
- Python main36 + isolated-runner11 pass (unchanged tools). All2125 entities run
  in core. Remaining short log paths are under `build/gfm-expanded/`.

Generated parser capacities retain all21 exact upstream sizes/predicates.
Latest `nondeep-current.json`: **19pass /0fail /2not_run**. Known earlier timeouts
quotes50000/lists1000 are deliberately not rerun until fixed. Do not claim21pass.
Other prior eleven failures resolved; no fixture size/predicate weakened.

A: BlockVec remains std::vector; iterative BlockNode copy/destruction and shared
walk/path/index verified at50000 depth (10 assertions), plus full regressions.
B: Parser removed serializer import/four subtree ending probes. Transient
Completion{Empty,Other,LineEnding} comes from local recognizers; sequence composition
handles empty-tail facts.24 differential fixtures/93 assertions passed. Central
formatter-mirroring switch remains rejected.
D: Public serializer facade + sole block formatting action visitor + line/event
writer replace the entire old recursive ProjectedText chain. External local-block
reparse uses public fragment API. Before replacement,2110 tests/2132 assertions
compared exact text/ordered maps across652CM+1426GFM+locals/real table edits;
HTML parity4/25 passed. Logs `serializer-parity-all1.log`, `serializer-html-focus3.log`.
Canonical direct-AST serializer tests passed quotes50000=4.73s/list1000=2.62s
(`serializer-canonical-deep.log`). These are separate from pending parser cases.
Vector-reference invalidation and next-line source-boundary mapping were fixed
before the passing evidence; neither is unresolved. No old formatter ships.

### Live work and exact next action

No root native handle running. No candidate CMake cache hook remains (cleared
with `-U CMAKE_PROJECT_folia_INCLUDE`, log `clear-candidate-hook.log`).
Tracked C parser work remains untouched; only test-projector integration/checkpoint edits follow D.
Find deliveries with `rg --no-ignore` under `build/gfm-expanded/deep-blocks/`.

- Sol `gfm_emphasis_scaling`: sole container engine in `parser/`. Latest
  `parser.candidate.ixx` has actual frame dispatch/open-close, blank routing,
  synthetic lazy-prefix staging; old recursive quote/list/footnote/image/parse_block
  methods removed. Still NOT compile-ready. Review deltas in
  `parser.container-transitions.inc`, `parser.frame-dispatch.inc`,
  `parser.simple-leaves.inc`, `ENGINE_DELTA.md`; not old interface-review snapshots.
- Luna `gfm_code_math_leaf`: isolated `leaf-assembly/parser.leaves.ixx` based on
  checksum6A0573ED...EE894981, audits/assembles actual paragraph/reference/table/
  code/math/HTML methods+imports while Sol owns container deltas. Never overwrite
  newer engine wholesale; merge leaf delta then engine delta.
- Luna `gfm_html_leaf`: producing COMPLETE HTML semantic/source-view method .inc
  for assembly (previous actualhunks still had prose for helper adaptations).
Test-only projector overlay is integrated in tests/support/gfm_capacity_projection.hpp,
tests/parsing/test_gfm_capacity_projection.cpp and test_gfm_pathological.cpp. Canonical
bounded4tests/30assert passed (`projector-canonical.log`); pathological target rebuilt.
Exact21 fixture inputs/predicates unchanged. No deep parser cases rerun yet.

Frozen pure helpers compiled and passed **28tests/176assertions** in isolated
FoliaLeafSourceTests (`leaf-helper-final.log`): source_session8/37, accumulator2/8,
code4/15, HTML3/17, image7/69, projector4/30. Snapshot is
`leaf-helper-verify/snapshot/`; root owns it. Code helper explicitly imports source
session and delegates range/append mapping to shared accumulator. Test-only include,
missing type using, Boost U32 printer, RenderBlockKind::Text and discarded document
trailing-EOL fixture errors were corrected. No helper runtime failure remains.

Accepted C decisions: one borrowed root UTF32/physical-line table, flat frame
arena, staged read-only offers then accept0 once, segmented leaf source materialized
only once, global prefix/ownership binding; no per-frame body/UTF8/per-char maps.
Only accepted lazy continuation commits synthetic prefixes. Same-line math partial
close uses accept_leaf_prefix; suffix is not logical line-start. Consumed closing
EOL can lie outside owner source while inside parser range. Root ranges use shared
push_leaf_range/ownership index, not endpoint-only remapping after indexing.
Block grammar fixes boundaries before all definitions/final inline CST. No image/
table/paragraph alternative trees. Sole-image Paragraph promotes only without CR/LF;
multiline stays Paragraph+inline image. Raw HTML type7 fixes full boundary first;
only one sole complete img CST uses existing image factory/envelope. Other type7
stays HtmlText Paragraph. No second HTML parse. Initial numeric NodeId order is
not invariant; uniqueness, stable untouched IDs on edit, anchors/history/selection
are mandatory. HTML slots remain existing leaf-local representation.

New focused serializer review evidence: old dynamic sibling separator recognizes
only final LF, while D complete_block currently treats loneCR as terminal too.
Parsed-corpus explicit separators mask this case. Luna gfm_deep_blocks_design is
preparing native focused cases and correction under ignored serializer/. Do not
claim dynamic-separator parity until that case is reproduced/fixed. Prefix grammar
must still recognize CR/LF/CRLF; only sibling separation follows prior LF rule.

Next: commit verified test-projector integration separately. Reproduce/fix that
small dynamic-separator case via focused native test. Await actual leaf assembly
+ Sol engine delta, inspect one-parser branch coverage, then first native C build.
Remaining engine reviews: collapsed item-opening prefixes, terminal prefix
completion, loose-item blanks, footnote context. No C parser integration/pass claim
yet. After fullC works run original quotes50000/lists1000 semantics/source/save/
lifecycle, then CM/GFM/core/Windows and full21. Keep checkpoint current.

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
