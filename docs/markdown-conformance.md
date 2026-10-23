# Markdown syntax conformance

## Current checkpoint (2026-10-23)

Task: user requested all finer GitHub tests plus fixes. Formal integration/fixes
are complete; two generated deep-block capacity cases remain in active work.
Keep the established unified block tree, block-local source/lossless CST, source
positions, transactions and WYSIWYG behavior. No second parser/model or depth cap.

### Verified commits and results

Starting checkpoint984558a4. Commits5f455590 (syntax/source consumers),73032708
(pinned complete corpora),d3e25b1c (inline scope/scanning/RAII),dc50d694 (single-column
table recognition/source ranges),9ccb4185 (derived NUL display),4feba6e9 (checkpoint),
edc6be39 (iterative block ownership/walk/index),cd9bade5 (local completion facts).
No MSI, push or version change requested. Root alone builds, sequentially,
**parallel4**. Tests link console runtime; stop broad evaluation after abnormal
native exit, then diagnose a bounded focus. Do not count unrun cases as passes.

Formal corpus **1426**: published677, cmark spec672 (incl2 upstream-disabled
retained cases), extensions30, regressions26, smart16, style-option1, full-info4.
Pinned cmark-gfm27d942c8b0a62d192f616e5bf3578f4b6a89e180. Every fixture retains
qualified ID, input/HTML, source/hash/license/profile/tab transform provenance.
Input binding is independently checked; C++ views include byte length for NUL.

Latest regression after canonical Batch D integration (all exit0):
- GFM **1395 strict pass /20 unscored /11 independently verified reference
  differences /0 unresolved**; all1426 lossless/exact-save/input checks pass.
  `build/gfm-conformance/serializer/results.json`.
- Nine old nested-strong expectations conflict with exact same Markdown in pinned
  CM0.31.2; two old HTML comments conflict its current rule. Original GFM HTML and
  strict_diff stay visible; verified differences are neither passes nor skips.
- Unscored: smart typography16, tagfilter3, upstream IGNORE/no HTML oracle1.
  Source/native assertions still run. Policy cannot hide implementation failures.
- CommonMark **652/652 strict/source/save**,
  `build/markdown-conformance/serializer/results.json`.
- Core **672 tests /491236 assertions**, `build/gfm-expanded/core-serializer.log`.
- Windows editor **80 tests /3429 assertions**, `platform-serializer.log`.
- Python main36 + isolated-runner11 contracts pass. Entity table2125 cases execute
  in core. Logs above are under `build/gfm-expanded/` unless explicitly named.

Capacity tests keep all21 original full-size inputs and upstream predicates,
profile pure CommonMark+table. Latest `build/gfm-expanded/nondeep-current.json`:
**19 pass /0 fail /2 not_run**. Two known earlier timeouts were deliberately not
repeated until fixed: quotes50000 and lists1000. Final full21 not yet passed.
Prior `generated-third.json` was10pass/11fail; its other eleven issues are resolved.
Original emphasis65000 now2.875s; image/link1600009.750s; code4999 runs/12.5MB1.750s;
comments3000001.453s; table300008.859s; references499997.797s. No abnormal exit.

Accepted repairs: one inline dispatch with explicit matched-emphasis frames and
eligible opener index; scoped bracket/read-only link preflight; cached code runs,
HTML terminators, angle stops and suffix necessary-end test. Unclosed emphasis
error spans are independent but source-preserving; complete nesting stays real.
InlineCstNodes is vector-compatible with iterative copy/destruction, aggregate
node/noexcept move/NodeIds/shared metadata preserved. Inline symbols DFS is
iterative. Table source stays gap syntax+owner IDs, cells own bodies only; setext
precedence and exactly3 paste inline reparses remain verified. NUL replacement
occurs only in derived display/HTML, not source/save/ranges/math input.

### Current live work and exact next action

Batch A BlockNode ownership/walk/index committed **edc6be39**:
`ast.ixx`, `block_tree.ixx`, `document.ixx`, isolated ownership test/CMake.
BlockVec/std::vector interface unchanged. Exact50000-layer copy/move/path/index/
clear/RAII focus passed10 assertions, `block-owner-focus.log`. Core661/491096,
Windows80/3429 and CM652/GFM1426 all retain their verified results after this
change (`core-block-owner.log`, `platform-block-owner.log`, reports under
`build/markdown-conformance/block-owner/`, `build/gfm-conformance/block-owner/`).

Batch B local completion facts committed **cd9bade5**, all24 differential
fixtures/93 assertions passed (`completion-focus3.log`) and full regressions above.
Parser no longer imports serializer or reserializes subtrees for ending probes.
Transient Completion{Empty,Other,LineEnding} is produced locally by recognizers;
nested facts transfer with the existing range pass. The centralized formatter
switch remains rejected. No completion build is running.

Current candidates are ignored beneath `build/gfm-expanded/deep-blocks/`:
- Sol `gfm_emphasis_scaling`: Batch C source session/flat container frame engine
  in `parser/`. Shared Leaf API v1 is frozen in LEAF_ENGINE_CONTRACT.inc. Leaf
  methods are delivered (paragraph/image, table, code/math, HTML); container
  dispatch/close/lazy/blank separator routing still incomplete, not integrated.
Batch D is now integrated UNCOMMITTED in serializer.ixx (public facade), new
serializer_writer.ixx (line/event planning and iterative emission), new
serializer_visitor.ixx (single block grammar action stack); the old recursive
ProjectedText formatter chain is removed. document_block_reparse now calls the
public fragment API. Writer/visitor/HTML tests are ordinary core tests; deep
serializer direct-AST cases are a separate bounded target (not ordinary core).
Before replacement, isolated candidate passed2110 tests/2132 assertions including
all652+1426 exact text/ordered maps plus edited tables; HTML parity4/25 also
passed. Logs serializer-parity-all1.log and serializer-html-focus3.log.
Root fixed vector-reference invalidation and next-line boundary mapping before
those passes; neither remains unresolved. No runtime dual formatter is shipped.
Live native build **75731**, `serializer-canonical-deep.log`, is building/running
the canonical FoliaDeepSerializerTests with CTest60s per case. Finish before a new
native build. Temporary candidate CMake cache hook is removed by this build's
configure (`-U CMAKE_PROJECT_folia_INCLUDE`); no hook should remain afterward.

Ignored candidate roots remain under `build/gfm-expanded/deep-blocks/` (find with
`rg --no-ignore`). Sol and Luna leaf owners are completing C dispatch and adapter
contract fixes; none are integrated. No agents run native builds or tracked/Git
edits. `test-projection/` contains an iterative two-case native AST HTML streamer
and bounded1/2/8-depth comparison tests. First comparison differed only in generated
HTML formatting; exact tag gaps/list separators are normalized only in the bounded
comparison. Latest normalization is not run yet; helper not integrated.

Accepted C decisions: no unresolved image/table/paragraph alternative trees.
Block grammar fixes leaf boundaries before final inline CST/definitions.
Only complete sole-image Paragraphs with no CR/LF promote to existing atomic
ImageBlock. Multiline remains Paragraph+inline image. Setext/table precedence
and image+text paragraphs follow normal block grammar. Raw HTML type7 determines
its full block boundary first; only a sole complete img HTML CST (surrounding
whitespace only) invokes existing ImageBlock factory/envelope/safe dimensions.
Other type7 stays HtmlText Paragraph. No second HTML parse or early line consume.
Fresh NodeId numeric allocation order is not invariant; unique IDs, stable
untouched IDs on edit, source anchors, selection and history remain mandatory.
Partial same-line math closes use accept_leaf_prefix, retain suffix with logical
line-start false; complete closing EOL is consumed but outside owner source.

Next: commit verified D separately; compile frozen C helper snapshot using
`build/gfm-expanded/build-leaf-helper.bat` (isolated FoliaLeafSourceTests, root
alone, parallel4). That configure enables ignored
`deep-blocks/leaf-helper-verify/snapshot-hook.cmake`; unset cache hook with CMake
`-U CMAKE_PROJECT_folia_INCLUDE` when done. Snapshot includes source-session,
accumulator, HTML/code/image helpers and bounded projector comparison tests.
Fix helper compiler/runtime failures before broad runs. Sol's parser.candidate.ixx
now has actual frame dispatch/open-close, but is explicitly NOT COMPILE-READY:
leaf hunks/simple leaves/callout/blank routing still incomplete. No C parser
integration or capacity-pass claim. Original quotes50000/lists1000 parser cases
must pass unchanged semantics/source/save/lifecycle;19/21 currently pass, two
known deep cases notrun. Keep this checkpoint current; old messages never override.

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
