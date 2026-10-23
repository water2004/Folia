# Markdown syntax conformance

## Current checkpoint (2026-10-23, expanded GFM validation in progress)

User requested all finer upstream cases and fixes. Starting Git checkpoint is
984558a4. Production syntax/consumer fixes plus focused tests are committed as
**5f455590**; corpus/runner integration is committed as **73032708**. Formal GFM corpus is now 1426:
published 677, repository spec 672 (including two upstream-disabled task cases),
extensions 30, regressions 26, smart punctuation 16, table-style option one,
full-info option four. Every record has qualified ID, original numeric ID,
pinned source/hash/license, profile and original/decoded tab spelling.

Latest formal run: `build/gfm-conformance/expanded-final/results.json`, exit 0:
**1395 strict pass, 20 explicit unscored HTML/no-oracle cases, 11 independently
verified reference differences, zero unresolved differences**. Source checks:
1426/1426 lossless CST, exact save and exact fixture-input binding. Nine older
GFM nested-strong expectations conflict with exact same Markdown in pinned
CommonMark 0.31.2; two old HTML-comment examples conflict with its current rule.
Original GFM expectations and strict_diff remain visible; these eleven are
neither passes nor skips. Policy validates independent modern expectations and
all source invariants. Smart typography 16, tagfilter three, upstream <IGNORE>
one are explicit unscored cases; none can hide a native/source failure.

Production changes: table row syntax gaps + owner IDs (no body copies),
remapping/formatting/history consumers; table bounds; TableCell code-escape
projection with source-mapped hidden backslash; item-based task-list semantics
and existing editing/export consumers; bounded bare-autolinks; exact one/two
strikethrough delimiter runs; footnote first-line spaces. Test projection uses
existing AST/CST for footnotes, style attrs and full code-info HTML attributes.
NUL fixture truncation was fixed with byte-counted views, independently checked
against full fixture input. Python contracts: **36 passed**.

Current validation:
- Core **638 tests / 59625 assertions**, exit 0,
  `build/gfm-expanded/core-final.log` (all 2125 named entities executed).
- CommonMark **652/652 strict, lossless and exact save**, exit 0,
  `build/markdown-conformance/gfm-expanded-final/results.json`.
- Windows editor **80 tests / 3429 assertions**, exit 0,
  `build/gfm-expanded/platform-final.log`.
- Formal GFM final **1395 strict pass / 20 unscored / 11 independently
  verified reference differences / zero unresolved**, native all source checks
  1426/1426. `build/gfm-conformance/expanded-final/results.json`, exit 0.
- Python main contracts **36 passed**, isolated generated runner **11 passed**.

Separate full-size generated-input suite is NOT complete; all 21 exact upstream
inputs and predicates are retained. Full run `build/gfm-expanded/generated-third.json`
ran all cases without abnormal exits: **10 pass / 11 fail** (four semantics,
seven 10s timeouts). This is separate from the fully resolved formal corpus.
Generated tests are isolated per process; do not turn timeouts into skips.

Capacity fixes committed as **d3e25b1c** (inline scopes/scans/RAII),
**dc50d694** (table recognition + source-range lookup), **9ccb4185** (NUL display):
- Inline bracket close index/read-only nested-link preflight, eligible emphasis
  opener categories and pair lookup; one shared dispatch engine with explicit
  emphasis frames. Real matched nesting is preserved; unmatched opener errors
  become independent source-preserving spans instead of deep error tails.
- InlineCstNodes remains vector-compatible, with iterative copy/destruction;
  node aggregate status/noexcept move/shared metadata/NodeIds are preserved.
  Symbol derivation uses iterative inline DFS. Ownership focus at 130000 levels
  passed 6 assertions (`owner-focus.log`). Exact upstream nested emphasis65000
  passed in 3.797s, then 3.422s; openers/closers65000 passed about1s each.
- Incomplete link right-parenthesis index; parser source-range tail lookup avoids
  repeated linear scans; pipe-less single-column tables/VT+FF marker whitespace,
  while setext precedence is preserved. Root reviewed these candidates.
- Generated adapter now uses the actual upstream profile (pure CM + table),
  contiguous regex repetitions/search vs anchored predicates, and direct iterative
  CST projection for the deep emphasis fixture. Runner aggregates multiple UT
  suite summaries; abnormal exits stop evaluation. Python runner **11 passed**.
- Indexed code runs/HTML terminators (escaped run starts/clipped scope ends),
  necessary right-parenthesis preflight and shared angle-autolink stopper index
  eliminate repeated suffix scans. NUL visible text/render/HTML sinks replace
  U+0000 with U+FFFD without touching source/save/ranges/math input. Render uses
  its existing source-attachment traversal; no extra rendering pass.
- Read-only table recognition now validates header+delimiter before constructing
  cells. This preserves the existing paste inline-reparse count (3), rather than
  parsing rejected ordinary paragraphs again. Focus25 assertions passed.

Completed checks after these fixes:
- CommonMark after deep emphasis, before latest code/link/table/NUL edits:
  **652/652**, `build/markdown-conformance/gfm-deep/results.json`.
- `build/gfm-expanded/fourth-focus.json`: image/link160000 pass11.203s;
  nested brackets50000 pass0.141s; tables30000 pass8.922s; references49999 pass8.063s.
- `fifth-focus.json` after latest scans/profile/NUL: original deep emphasis65000
  pass3.172s; bracket-paren80000 pass0.203s; embeddedNUL pass0.015s; increasing
  ticks4999 runs/12.5MB pass1.829s; angle-links30000 pass0.281s; comments300000
  pass1.453s. Native NUL render/query/source tests31 assertions passed.
- All capacity paths except quotes50000/lists1000 now have successful native
  focused evidence (a final combined report is still pending).

Final regression on these three commits (all exit0):
- Core661 tests /491096 assertions, `build/gfm-expanded/core-scaled-final2.log`.
- CommonMark652/652 strict/source/save,
  `build/markdown-conformance/gfm-scaled-final/results.json`.
- GFM1426:1395 strict pass/20 unscored/11 verified reference differences/0 unresolved,
  all source/save/input checks pass, `build/gfm-conformance/scaled-final/results.json`.
- Windows editor80 tests /3429 assertions, `platform-scaled-final.log`.
- Main Python36 + isolated runner11 contracts pass, `python-*-scaled-final.log`.
No live native builds. Root alone builds sequentially with **parallel4**.
Live candidate-only work:
- Luna `gfm_reference_scaling`: parser completion metadata Batch B, ignored
  `build/gfm-expanded/block-completion/`. Root approved replacing parser's four
  serializer probes with transient per-node ending facts, no AST fields or
  production serializer fallback; must be differential-tested before integration.
- Luna `gfm_deep_blocks_design`: serializer event/writer algorithm design only,
  ignored `build/gfm-expanded/deep-block-design/`; no implementation authorized yet.
- Sol `gfm_emphasis_scaling` delivered UNINTEGRATED Batch A BlockNode iterative
  copy/destruction + block walks/index, `build/gfm-expanded/deep-blocks/`. Keeps
  BlockVec/std::vector interface unchanged; candidate tests50000-level lifecycle.
  Parser/serializer frame implementation not started. Quotes50000/lists1000 remain
  unresolved: recursive ParseInput copies/maps and subtree serializer probes are
  O(depth*source). Root has read FRAME_DESIGN.md and owns integration decisions.
All agents are candidate-only, no native builds/Git. Other agents idle.

Next: verify model Batch A in isolated target, then review Batch B producer-local
completion facts (the first central formatter-mirroring switch was rejected).
Batch C/D are candidate work, not verified/integrated. Finish quote/list capacity
within existing model/responsibilities: no depth cap, source-specific branch,
reduced scale, parallel old/new parser, or softened oracle. Final combined19-case
report is pending; original full21 still has two unresolved deep-block cases.
Ignored candidates live under `build/gfm-expanded/`; use `rg --no-ignore`.
No MSI/push/version change requested.

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
