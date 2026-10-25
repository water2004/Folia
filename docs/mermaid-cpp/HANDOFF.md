# Mermaid C++ continuity

## Current checkpoint - 2026-10-25, Class paints and lollipop interfaces verified

- Paint batch 8aa88d5c connects style/classDef/cssClass/inline ::: assignments
  to fill, border, stroke width/dash and text colors, retaining statement order,
  direct overrides, divider paints and static/abstract member typography.
  All four Windows standalone/integrated Debug/Release jobs passed:
  https://github.com/zhao0349/Folia/actions/runs/40207510779
- Interface parser ece83aea and measured SVG 157f54fb retain independently
  allocated ids/labels/class ownership for each relationship, including repeated
  names. DirectWrite label measurements feed transparent rectangle anchors;
  SVG uses original circle markers and 13.5-unit endpoint shortening.
  Final head 157f54fb passed all four Windows jobs:
  https://github.com/zhao0349/Folia/actions/runs/40208084657
  This run used codex/mermaid-cpp-class-interfaces to preserve the earlier run.
- Both integrated configurations passed FoliaTests, FoliaWindowsEditorTests and
  both Mermaid suites. Class parser observations now cover 20 successes/eight
  failures; 58 Class SVG comparisons cover 29 diagrams/both themes, including
  paints, interface labels, transparent anchors, circle geometry and paths.
  Existing 92 Flowchart, 82 Sequence and 96 browser/DirectWrite comparisons
  passed. Tolerances unchanged. Both functional batches met exit conditions;
  do not expand their verification matrix. No acceptance failure remains.
- Actual Release evidence is in build/mermaid-cpp/class-paints-verified and
  class-interfaces-verified (class-browser/comparison.json, text-comparison.json,
  pie-svg and native-preview.png). Six paint and four interface previews were
  visually inspected. No local native build or WinUI/GPU/DPI acceptance claimed.
- Contribution equivalents f884283f/cf7fda2f/bdb081dd are on feat/mermaid-cpp-port
  in build/mermaid-cpp/entry-contribution. Runtime/tests match the verified head;
  this documentation checkpoint follows it. No AGENTS.md difference against
  PR base 508423f2. Delivery-triggered reruns are separate from completed
  implementation validation. PR #1 stays open/unmerged:
  https://github.com/water2004/Folia/pull/1
- Next functional boundary: Class htmlLabels=false measured SVG-label spacing.
  Four original observations are in build/mermaid-cpp/class-svg-label-next/
  original.json (members and classifiers, both themes). Start from shapeUtil
  addText/createText's SVG path and per-run native font bounds. Preserve member
  classifiers and compartment layout. Exit on those inputs and affected checks,
  then continue remaining required Class configuration/layout and family work.
- Folia still uses Rust in the application. Default Class ELK, styled typography,
  remaining families and app integration remain. Local MSVC/CMake are unavailable;
  no arbitrary-font parity claim. No uncommitted implementation candidate or
  live local browser/build/agent remains.

## Previous checkpoint - 2026-10-25, Class common paints in progress

- Deliver direct/class-defined fill, border, stroke width/dash and text color
  through style, classDef, cssClass and inline ::: assignments. Preserve
  ClassDB statement ordering, member classifiers and measured geometry.
- Exit: representative parser observations and default/dark SVG comparisons,
  plus affected Windows checks. Then continue the next required Class boundary.
  No generic CSS engine, typography expansion or application switch in this batch.
- Candidate is unverified; previous accepted evidence follows below.

## Previous checkpoint - 2026-10-25, Class note emphasis and compact namespaces verified

- Note emphasis b1ce03a2 delivers measured bold/italic runs, left-aligned explicit
  lines, note colors and attachments. Long notes retain ClassDB nowrap behavior.
- Compact namespaces 5213e3d4 connects class.hierarchicalNamespaces=false to
  the measured rendering projection: omit implicit ancestors, flatten explicit
  groups, use qualified-id titles and retain class/note ownership.
- Correction 11b33fdf rounds intrinsic SVG viewport dimensions up while keeping
  precise viewBox bounds. This prevents CSS width rounding from scaling font
  rasterization down. Final verification head 11b33fdf passed all four Windows
  standalone/integrated Debug/Release jobs:
  https://github.com/zhao0349/Folia/actions/runs/40203191310
- Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. Forty-eight Class SVG comparisons cover 24 diagrams/both themes,
  including both batches. Existing 92 Flowchart, 82 Sequence and 96 real
  browser/DirectWrite comparisons passed. Tolerances unchanged. Both batches
  met exit conditions; do not expand their verification matrix.
- Final Release evidence and eight visually inspected new previews are in
  build/mermaid-cpp/class-note-compact-verified (class-browser/comparison.json,
  text-comparison.json, pie-svg and native-preview.png). Earlier run 40202478829
  exposed the 0.35px long-note width difference; it is superseded by the final
  verified head. Diagnostic transformed SVGs in class-note-viewport-check were
  only a focused diagnosis, not build evidence. No acceptance failure remains.
- Contribution equivalents a72cf41b/8bc62383/ab4a5dc3 are on feat/mermaid-cpp-port
  in build/mermaid-cpp/entry-contribution. Runtime/tests match the verified head;
  this documentation checkpoint follows it. No AGENTS.md difference against
  PR base 508423f2. Delivery-triggered reruns are separate from completed
  implementation validation. PR #1 stays open/unmerged:
  https://github.com/water2004/Folia/pull/1
- Next functional boundary: common Class direct/class-defined paints. Four
  original observations are in build/mermaid-cpp/class-style-next/original.json
  (class-direct-style and class-defined-style, both themes). Start from
  ClassDB.setCssStyle/defineClass/setCssClass, classDiagram.jison style statements
  and shape style resolution. Preserve member classifiers and measured geometry.
  Exit on these representative inputs and affected checks, then continue the
  remaining required Class configuration/layout and family chain.
- Folia still uses Rust in the application. Default Class ELK, remaining required
  families/configuration and app integration remain. Local MSVC/CMake are
  unavailable; no WinUI/GPU/DPI or arbitrary-font parity claim. No uncommitted
  implementation candidate or live local browser/build/agent remains.

## Previous checkpoint - 2026-10-25, Class note emphasis in progress

- Deliver measured bold/italic note runs with explicit lines, left alignment,
  existing colors and attachments; preserve ClassDB nowrap for long notes.
- Scope/exit: note-emphasis and note-nowrap in both themes, affected Class SVG
  comparisons and existing Windows standalone/integrated checks. Then advance
  to the next required Class configuration/layout boundary. No app switch.
- Candidate is unverified; prior accepted evidence follows below.

## Previous checkpoint - 2026-10-25, Class emphasis and relationship wrapping verified

- Emphasis 8b5d304c delivers Class label/member/relationship font runs through
  DirectWrite and SVG spans, including relative bold weights inside class titles,
  while preserving member classifiers and escaped angle brackets.
- Wrapping 5924ba97 connects long relationship labels to createText's original
  200px default using native line-break opportunities, preserving styles and
  spaces across lines. The existing Dagre adapter consumes the measured boxes.
- Corrections 298bc947/045c6872 retain rounded font ascent/descent for CSS
  half-leading and shapeUtil's centered text-group bounds for mixed font heights.
  Final verification head 045c6872ded4c1db1a6f753707ba7e79f07ef51e passed:
  https://github.com/zhao0349/Folia/actions/runs/40200486830
- All four standalone/integrated Debug/Release jobs passed. Integrated jobs
  passed FoliaTests, FoliaWindowsEditorTests and both Mermaid suites. Forty
  Class SVG comparisons cover 20 diagrams/both themes, including emphasis runs
  and wrapped relationships. Existing 92 Flowchart, 82 Sequence and 96
  browser/DirectWrite comparisons passed. Tolerances unchanged. Both batches
  met their exit conditions; do not expand their verification matrix.
- Release evidence and four visually inspected new previews are in
  build/mermaid-cpp/class-emphasis-wrap-verified (class-browser/comparison.json,
  text-comparison.json, pie-svg and native-preview.png). Earlier runs
  40199353436/40199922642 exposed the two mixed-font height issues; superseded
  by the final verified head. Diagnostic outputs remain in class-emphasis-diagnostic
  and class-emphasis-metrics-diagnostic. No acceptance failure remains.
- Contribution equivalents 22e352d3/e7a5ffde/153efbed/2dfdb772 are on
  feat/mermaid-cpp-port in build/mermaid-cpp/entry-contribution. Runtime/tests
  match the verified head; this documentation checkpoint follows it. No
  AGENTS.md difference against PR base 508423f2. Delivery-triggered reruns are
  separate from completed implementation validation. PR #1 stays open/unmerged:
  https://github.com/water2004/Folia/pull/1
- Next functional boundary: measured Class note emphasis. Four original
  observations are in build/mermaid-cpp/class-note-display-next/original.json:
  note-emphasis and note-wrapped, both themes. Despite the latter probe name,
  the original long note stays on one line: ClassDB specifies white-space: nowrap.
  Preserve that behavior; do not introduce an automatic note-wrapping requirement.
  Start from ClassDB.getData, note.ts/labelHelper and the new Class measured run
  adapter. Preserve note left alignment, explicit lines, colors and attachments.
  Exit on these representative inputs and affected checks, then continue the
  remaining required Class configuration/layout and family chain.
- Folia still uses Rust in the application. Default Class ELK, remaining required
  families/configuration and app integration remain. Local MSVC/CMake are
  unavailable; no WinUI/GPU/DPI or arbitrary-font parity claim. No uncommitted
  implementation candidate or live local browser/build/agent remains.

## Previous checkpoint - 2026-10-25, Class namespaces and display lines verified

- Namespace c4285f6f delivers measured plain container titles, nested/qualified
  ownership, original padding/title margins and recursive Dagre geometry with
  class compartments, notes and cross-container relationships intact.
  https://github.com/zhao0349/Folia/actions/runs/40195483652
- Display batch b523acf3 delivers explicit label/relationship line breaks and
  decoded entities. Member text uses its original escaped source, retaining
  literal angle brackets, generics and static/abstract classifiers. Correction
  8ee4c092 normalizes member break tags before parsing; a0291ede updates the
  fixture count. Final verification head:
  a0291ede753aedb27c7f2456cbf1b2ca75f145c9
  https://github.com/zhao0349/Folia/actions/runs/40197420681
- Both runs passed all four standalone/integrated Debug/Release jobs. Integrated
  jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid suites. Both
  batches met their exit conditions; do not expand their verification matrix.
  The Class parser corpus has 14 successes/seven errors; 36 Class SVG comparisons
  cover 18 diagrams/both themes. Existing 92 Flowchart, 82 Sequence and 96
  browser/DirectWrite comparisons passed. Tolerances unchanged.
- Final Release evidence is in build/mermaid-cpp/class-display-verified. Four new
  previews were inspected at class-display-runtime-verified/pie-svg/native-preview.png;
  all 36 final SVGs are byte-identical to those outputs. Namespace evidence and
  six inspected previews are in class-namespace-verified. Earlier 40196170419
  exposed canonical member break spelling; 40196801428 exposed the stale fixture
  count. Those failures are superseded by the final verified head above.
- Contribution equivalents f31f36c7/1173764c/32e94c46/208b68eb are prepared on
  feat/mermaid-cpp-port in build/mermaid-cpp/entry-contribution. Runtime, tests,
  pins, workflow and public docs match the verified head; this documentation
  checkpoint follows it. No AGENTS.md diff against PR base 508423f2. Delivery-
  triggered reruns are separate from completed implementation validation.
  PR #1 stays open/unmerged: https://github.com/water2004/Folia/pull/1
- Next functional boundary is common Class emphasis. Six original observations
  from class-display-next.mjs are in build/mermaid-cpp/class-display-next/original.json;
  display-lines/entities are now covered, leaving display-emphasis/both themes.
  Start from shapeUtil.addText, createText, handle-markdown-text, ClassMember.text
  and the existing measured Flowchart styled-run adapter. Preserve member
  classifier parsing and escaped angle brackets; use per-run native font metrics.
  Exit on that representative input and affected checks, then continue automatic
  wrapping and the remaining required Class/family chain.
- Folia still uses Rust in the application. Rich/automatically wrapped labels,
  default ELK, remaining required families/configuration and app integration remain.
  Local MSVC/CMake are unavailable; no WinUI/GPU/DPI or arbitrary-font parity claim.
- No failed acceptance check, uncommitted implementation candidate or live local
  browser/build/agent remains. Resume at the measured emphasis boundary above.

## Previous checkpoint - 2026-10-25, measured Class namespaces in progress

- Deliver measured namespace containers using the existing unified Dagre
  recursive adapter, preserving class compartments, notes and cross-container
  relationships. Scope: the three existing namespace inputs in default/dark.
- Exit: original/native container, label, viewport and relationship comparisons
  plus affected Windows standalone/integrated Debug/Release checks. Then advance
  to the next required Class label/configuration boundary; no broader matrix.
- Candidate implementation is local and unverified. Prior verified code and
  contribution state are recorded below. No application dependency switch.

## Previous checkpoint - 2026-10-25, Class notes and namespace parsing verified

- Note batch ae7e8694 delivers owned standalone/attached declarations, literal
  multiline text and measured note SVGs with original padding, theme colors,
  line positions and dotted attachments preceding ordinary Dagre relationships.
  Missing attachment targets remain independent notes. Verification:
  https://github.com/zhao0349/Folia/actions/runs/40191965170
- Namespace parser batch 60088e16 retains labels, nested/qualified ids, implicit
  ancestors, parents and ordered class/note membership. Three representative
  namespace inputs and two common failures extend the Class parser corpus to
  13 successes/seven errors. Namespace SVG remains an explicit error until the
  measured container adapter. Final verification head:
  60088e16f9f13e290fc9ce0df0745108542cff50
  https://github.com/zhao0349/Folia/actions/runs/40192256291
- Both runs passed all four standalone/integrated Debug/Release jobs. Integrated
  jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid suites. Both
  batches met their exit conditions; do not expand their verification matrix.
  The 26 Class SVG comparisons cover 13 diagrams/both themes, including the new
  note-only, attached/multiline and LR relationship cases. Existing 92 Flowchart,
  82 Sequence and 96 browser/DirectWrite comparisons passed. Tolerances unchanged.
- Release evidence is in build/mermaid-cpp/class-notes-verified and
  class-namespace-parser-verified. Six note previews in the former directory's
  pie-svg/native-preview.png were visually inspected. All 26 Class SVGs in the
  final parser batch are byte-identical to those inspected outputs.
- Contribution equivalents 9db5ffab/89aff4bc are on feat/mermaid-cpp-port in
  build/mermaid-cpp/entry-contribution. Runtime/tests/pins match the verified
  head; this documentation checkpoint follows it. No AGENTS.md diff against
  PR base 508423f2. Delivery-triggered reruns are separate from the completed
  implementation validation. PR #1 remains open/unmerged:
  https://github.com/water2004/Folia/pull/1
- The next measured namespace stage already has six official SVG observations
  in build/mermaid-cpp/class-namespace-svg-next/original.json. Start with
  ClassDB.getData (namespace groups precede classes/notes), clusters.rect and
  the existing flowchart_clusters.inc recursive layout adapter. Use measured
  plain group titles, original group padding/title margins and parent rank
  spacing; preserve class compartments, notes and cross-container relations.
  Exit on the three existing namespace inputs/both themes and affected checks,
  then continue the remaining required Class/family chain.
- The parser observations are also retained in
  build/mermaid-cpp/class-namespace-next/original.json. Rich Class labels, default
  ELK, remaining required families/configuration and application integration
  remain. Folia still uses Rust in the application. Local MSVC/CMake remain
  unavailable; no local native build or WinUI/GPU/DPI acceptance is claimed.
  The comparison corpus uses controlled fonts, not arbitrary-font parity.
- No failed acceptance check, uncommitted implementation candidate or live local
  browser/build/agent remains. Continue at the measured namespace boundary above.

## Previous checkpoint - 2026-10-25, Class titles and self relationships verified

- Title batch 622e2f97 connects plain diagram titles through DirectWrite to
  original insertTitle placement, theme typography, state.titleTopMargin and
  padded viewport bounds. Normal and wide titles cover both default/dark themes.
- Self-relationship batch 59babe2e reuses prepare_flowchart_graph's three layout
  segments and a shared compact-loop router. Class keeps its rectangle clipping,
  relationship markers, main labels and terminal multiplicity. Representative
  inputs cover a single loop and an LR loop alongside an ordinary relationship.
- Corrections 7bbbf8b5/ec262213 use SVG font ascent/descent for title bounds and
  the original rounded-coordinate path selection for terminal labels. The first
  runs found those two concrete differences; their failures are superseded, not
  accepted. No tolerance change or additional boundary matrix was introduced.
- Final verification head ec2622137468beb7edede167da45d64f64857813 passed all four
  standalone/integrated Debug/Release jobs:
  https://github.com/zhao0349/Folia/actions/runs/40189914812
  Both integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. The 20 Class SVGs across ten diagrams/both themes compare title bounds,
  compartments, text, ordinary/self paths, markers and multiplicity. Existing
  92 Flowchart, 82 Sequence and 96 browser/DirectWrite comparisons also passed.
  Both requested batches met their exit conditions; do not expand validation.
- Final Release evidence is in build/mermaid-cpp/class-title-self-verified:
  class-browser/comparison.json, pie-svg, text-comparison.json and
  native-preview.png. The preview isolates the eight new title/self outputs as
  images; all were visually inspected. Earlier diagnostic evidence remains in
  class-self-diagnostic; class-title-work/class-self-work hold local references.
- Contribution equivalents af30a28f/ffcd28ac/bb15d81c/1cf5258d are on
  feat/mermaid-cpp-port in build/mermaid-cpp/entry-contribution. Runtime, tests,
  pins, workflow and public docs match the verified head. This checkpoint follows
  the tested code. No AGENTS.md diff against PR base 508423f2. Delivery-triggered
  reruns are separate from the completed implementation validation above.
  PR #1 stays open/unmerged: https://github.com/water2004/Folia/pull/1
- Next concrete Class batch: owned note declarations and measured note SVG,
  starting with the pinned classDiagram.jison/classDb.ts note handling and the
  unified note shape/relationship placement. Then continue required namespace
  containers and remaining Class/family work. Preserve the explicit Dagre
  boundary; default ELK remains unported and must not be silently substituted.
- Folia still uses Rust in the application. Local MSVC/CMake are unavailable;
  no local native build or WinUI/GPU/DPI acceptance is claimed. The Class corpus
  uses controlled fonts, not arbitrary-font parity. No local browser/build/agent,
  failed acceptance check or uncommitted implementation candidate remains.

## Previous checkpoint - 2026-10-25, Class parser and measured SVG verified

- Parser commits b46c6486/ef11e206 met their exit conditions: all four Windows
  standalone/integrated Debug/Release jobs passed at
  https://github.com/zhao0349/Folia/actions/runs/40184783701
  Eight successful source-matched Class inputs and four common parse errors
  cover declarations, labels, generics, annotations, ordered attributes/methods,
  classifiers, ordinary relationships, multiplicity and direction.
  Contribution equivalents f9703e90/1bb104e2 are pushed; PR #1 is open/unmerged.
- SVG implementation is complete for this batch: explicit classic/Dagre with
  measured plain class/annotation/member/method text, original compartments,
  hideEmptyMembersBox/padding/spacing, classifiers, ordinary relationship paths,
  markers and multiplicity. Root fontFamily selects the measured font, theme
  fontSize selects its size, and absent htmlLabels retains the original true
  default. There is no guessed character-count measurement or layout swap.
- The original Class default is ELK. The native entry supports the official
  explicit Dagre option and returns an error for unported ELK. Titles, self
  relationships, namespaces/notes and rich/wrapped labels remain separate work.
- Final verification head cc1d319999a630eb3c3adefc6d00bb5a6394b472 passed at:
  https://github.com/zhao0349/Folia/actions/runs/40187334232
  All four standalone/integrated Debug/Release jobs passed. Integrated jobs
  passed FoliaTests, FoliaWindowsEditorTests and both Mermaid suites. All 12
  Class SVG comparisons pass with unchanged tolerances (0.2 geometry, 0.25
  advance/origin, 2.5 vertical text center), alongside the existing 92 Flowchart,
  82 Sequence and 96 browser/DirectWrite measurement comparisons. This batch met
  its exit conditions; no further verification expansion is needed.
- Final downloaded evidence and inspected previews are in
  build/mermaid-cpp/class-svg-verified (class-browser, pie-svg,
  native-preview.png). All 12 SVGs are byte-identical to the inspected runtime
  outputs from 181af12f; the final test-only correction compares HTML and SVG
  advance origins/widths consistently, retaining vertical glyph bounds.
  Earlier 40185649721/40186124687/40186785644 failures
  exposed missing htmlLabels default, wrong font source, solid dash style and
  mismatched text measurement definitions; superseded by the current head.
- Contribution commits 3d85a149 through adeb0f6a deliver this SVG batch through
  build/mermaid-cpp/entry-contribution; runtime, tests, pins, workflow and public
  docs match the verification branch. No AGENTS.md diff against PR base
  508423f2. This documentation checkpoint follows the tested code. Delivery-
  triggered reruns are separate from the completed implementation validation.
  PR #1 remains open/unmerged; never merge:
  https://github.com/water2004/Folia/pull/1
- Next concrete batch is Class titles, then self relationships. Four official
  SVGs across two inputs/both themes are ready in
  build/mermaid-cpp/class-next-svg/original.json, generated by
  build/mermaid-cpp/class-next-svg.mjs. Start with utils.ts insertTitle,
  setupViewPortForSVG, the unified Dagre self-loop preparation and merge paths,
  and the existing flowchart_graph adapter. Add title measurement to
  ClassMeasurements and the platform adapter, then extend the Class SVG viewBox
  using the original title geometry. Exit on those representative outputs
  and affected checks; then continue the remaining required Class/family chain.
- Folia still uses Rust in the application. Local MSVC/CMake remain unavailable;
  no local native build or WinUI/GPU/DPI acceptance is claimed. The Class corpus
  uses Arial 16px with 11px multiplicity labels. No local browser/build/agent is
  running. No failed acceptance check or uncommitted implementation candidate
  remains; resume at the title/self relationship boundary above.

## Previous checkpoint - 2026-10-25, participant grouping and lifecycle verified

- Grouping commit 71b85336 delivers owned box membership, CSS color/title parsing,
  measured/wrapped group titles, group margins, header offsets and backgrounds
  following footer geometry. Original box grammar rejects incomplete groups and
  conflicting participant membership. Verification run:
  https://github.com/zhao0349/Folia/actions/runs/40182169493
- Lifecycle commit acbfdabb delivers create/destroy message indices, late-entry
  headers, sender/receiver message endpoints and centered labels, destroyed
  participant footer positions and clipped lifelines, including no-footer output.
  Duplicate creation and unrelated lifecycle messages return DB errors.
  Verification head: acbfdabb5cc372ac342c979ac6714f89be2e512d, run:
  https://github.com/zhao0349/Folia/actions/runs/40182462146
- Both runs passed all four standalone/integrated Debug/Release jobs. Both
  functional batches met their exit conditions. Integrated jobs passed FoliaTests,
  FoliaWindowsEditorTests and both Mermaid suites.
- Grouping compares 74 original/native Sequence SVGs; lifecycle compares 82 SVGs
  across 41 diagrams and both themes. Parser fixtures cover 39 successes and 13
  common errors. Existing 92 Flowchart comparisons and 96 browser/DirectWrite
  measurements passed in all four jobs. No new verification expansion is needed.
- Native previews inspected in build/mermaid-cpp/sequence-groups-verified and
  sequence-lifecycle-verified (native-preview.png, pie-svg and comparison JSON).
  Previews isolate each SVG as an image. sequence-groups-work and
  sequence-lifecycle-work hold local original references.
- Contribution equivalents 31c5dfd5/f27e267b are prepared locally on
  feat/mermaid-cpp-port in build/mermaid-cpp/entry-contribution for delivery.
  Runtime, tests, pins and workflow match the verified head; this documentation
  checkpoint follows them. No AGENTS.md diff against PR base 508423f2.
  Delivery-triggered reruns are separate from the completed verification above.
  PR #1 remains open/unmerged; do not merge:
  https://github.com/water2004/Folia/pull/1
- Next functional batch after these exit conditions: start the required Class
  family with owned class/member/method and relationship parsing, then measured
  class boxes and original family layout. Two source-matched official DB inputs
  are saved in build/mermaid-cpp/class-next/original.json, generated by
  build/mermaid-cpp/class-next.mjs.
  Start with class/parser/classDiagram.jison, classDb.ts and classTypes.ts;
  the selected renderer is classRenderer-v3-unified.ts. Pin newly used files when
  promoting the oracle. Exit the first batch on those representative members,
  inheritance/aggregation, multiplicity and direction values plus common parse
  failure checks; continue the source-to-SVG chain without a generic layout swap.
- Limits: Sequence rich labels/further configuration, other required families and
  application integration remain. Folia still uses Rust in the application.
  Local MSVC/CMake are unavailable; no local native build or WinUI/GPU/DPI
  acceptance is claimed. The comparison corpus uses Arial 16/20.
- No failed acceptance check, uncommitted implementation candidate or live local
  build/browser/agent remains. Continue in the existing contribution worktree
  noted above, using the Class observations as the next concrete starting point.

## Previous checkpoint - 2026-10-25, Sequence participant metadata and shapes verified

- Commits dba432ed/347187cd/88a2212e deliver owned YAML type/alias metadata,
  multiline metadata and explicit alias precedence; classic boundary, control,
  entity, database, collections and queue shapes with measured labels, original
  transforms, lifeline starts and mirrored footer heights. The old unsupported
  metadata assertion now checks incomplete metadata instead.
- Verification head: 88a2212e495208b0f10f62245fd5a9f98d5f7566, run:
  https://github.com/zhao0349/Folia/actions/runs/40180399371
  All four standalone/integrated Debug/Release jobs passed. This batch met
  its exit conditions; no additional validation expansion is needed.
  Each job compared 68 original/native Sequence SVGs across 34 diagrams
  and both themes. The parser oracle covers 32 successes and 8 expected errors.
  Existing 92 Flowchart comparisons and 96 browser/DirectWrite measurements pass.
  Both integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. Source/shape validation is bounded to the representative batch inputs.
- Native default/dark previews inspected in
  build/mermaid-cpp/sequence-participants-verified/native-preview.png; the folder
  also contains native SVGs and comparison reports. Previews isolate each SVG
  as an image so repeated marker IDs cannot interact between gallery cells.
  sequence-participants-work contains the local official references.
- Contribution equivalents 04a78f2c/4f826160/313d7a1a are prepared in
  build/mermaid-cpp/entry-contribution for delivery. Runtime, tests, source
  pins and workflow match the verification head. No AGENTS.md diff against
  PR base 508423f2. This documentation checkpoint follows the tested code.
  Delivery-triggered reruns are separate from the completed verification above.
  PR #1 remains open/unmerged; do not merge:
  https://github.com/water2004/Folia/pull/1
- Next functional batch: participant box grouping, followed by lifecycle.
  The next stage has started with six official observations across three diagrams/two themes are saved in
  build/mermaid-cpp/sequence-grouping-next/original.json, generated by
  build/mermaid-cpp/sequence-grouping-next.mjs. Start with sequenceDiagram.jison
  box_section, sequenceDb.ts addBox/parseBoxData, sequenceRenderer.ts
  calculateActorMargins/addActorRenderingData and svgDraw.js drawBox.
  Preserve measured group titles, group margins, header offset, background order
  and footer-dependent box bounds. Exit on the two representative grouping inputs
  and affected checks, then implement create/destroy against the third input.
- Limits: rich labels, grouping/lifecycle, other required families and application
  integration remain. Folia still uses Rust in the application. Local MSVC/CMake
  are unavailable; no local native build or WinUI/GPU/DPI acceptance is claimed.
  The comparison corpus uses Arial 16/20, not arbitrary-font parity.
- No failed acceptance check, uncommitted implementation candidate or live local
  build/browser/agent remains. Earlier metadata-only run 40180154744 also passed
  all four jobs; cancelled run 40180321129 is superseded by the verified head.
  Resume in the existing contribution worktree noted above.

## Previous checkpoint - 2026-10-25, Sequence numbering, visibility and arrows verified

- Commit 5ebec914 delivers autonumber start/step, off/resume, configured numbers,
  number discs/arrow offsets, hideUnusedParticipants and mirrorActors. Hidden
  message numbers still increment; notes/activation events do not. Participant
  filtering follows original spacing calculation; footers control viewport height.
  All four standalone/integrated Debug/Release jobs passed:
  https://github.com/zhao0349/Folia/actions/runs/40177654987
- Commit 1eac2966 connects the already-parsed cross/async arrows to SVG, retaining
  ordinary/self/activation endpoints, original markers and default/dark paints.
  All four standalone/integrated Debug/Release jobs passed:
  https://github.com/zhao0349/Folia/actions/runs/40178016776
- Final verified head: 1eac2966f93494e16b12206238633612a32a75c8. Each job compared
  64 original/native Sequence SVGs across 32 diagrams and both themes, including
  displayed numbering/discs, footers, participants, notes, activations, control
  bounds, paths and marker paints. The parser oracle covers 28 successes and
  7 expected errors. Existing 92 Flowchart comparisons and 96 browser/DirectWrite
  measurements passed. Integrated jobs passed FoliaTests, FoliaWindowsEditorTests
  and both Mermaid suites. Both batches met their exit conditions.
- Native previews were inspected in build/mermaid-cpp/sequence-numbering-verified
  and sequence-arrows-verified (native-preview.png, pie-svg and comparison JSON).
  sequence-numbering-work and sequence-arrows-work contain local originals.
- Contribution equivalents 20c97644/19709eaf are on feat/mermaid-cpp-port; this
  documentation checkpoint follows them. Runtime, tests, pins and workflow match
  the verified head. No AGENTS.md diff against PR base 508423f2 or upstream/master
  e9b48e4a. PR #1 remains open/unmerged; do not merge:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from the completed verification above.
- Next functional batch: Sequence participant metadata and measured shapes.
  Four original observations across two diagrams/two themes are saved in
  build/mermaid-cpp/sequence-participants-next/original.json, generated by
  build/mermaid-cpp/participants-probe.mjs. Start with sequenceDiagram.jison
  actor_with_config, sequenceDb.ts addActor and svgDraw.js drawActorTypeBoundary,
  Control, Entity, Database, Collections and Queue. Reuse existing owned YAML
  config parsing for type/alias; preserve family-specific geometry and measured
  labels. Exit on these representative metadata/ordinary/activated messages and
  affected checks, then continue required participant/family/application work.
- Limits: rich labels, extended participants, remaining required families and
  application integration remain. Folia still uses Rust in the application.
  Local MSVC/CMake remain unavailable; no local native build or WinUI/GPU/DPI
  acceptance is claimed. The corpus uses Arial 16/20, not arbitrary-font parity.
  No failed acceptance check, uncommitted candidate, local build/browser/agent
  or CI watcher remains active. Resume in the existing contribution worktree
  build/mermaid-cpp/entry-contribution.

## Previous checkpoint - 2026-10-25, Sequence display text verified

- Commits 3f3791af/d0388a9a/b1e26d45/852648c9 deliver explicit HTML breaks,
  protected Mermaid character entities and automatic wrapping for actors,
  messages, notes and nested control labels. Participant spacing precedes final
  message/note/control widths; wrapped actors can raise the shared actor row.
  Sequence uses DirectWrite glyph coverage bounds for integer wrap decisions.
- Verified head 852648c909d5e07777fac6eea900bf87221f7860 passed all four Windows
  standalone/integrated Debug/Release jobs:
  https://github.com/zhao0349/Folia/actions/runs/40176214793
  Each job compared 50 original/native Sequence SVGs across 25 diagrams and
  default/dark themes, including displayed lines, boxes, notes, paths, activation,
  nested bounds and paints. The Sequence parser oracle covers 23 successes and
  6 expected parse/DB errors. Existing 92 Flowchart comparisons, source/SVG checks
  and real browser/DirectWrite comparisons passed. Integrated jobs also passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites.
- Native default/dark previews were inspected in
  build/mermaid-cpp/sequence-display-verified/native-preview.png. The same folder
  contains final native SVGs and comparison reports. sequence-wrap-work contains
  local original references. Earlier runs 40175444332, 40175629943 and
  40175927751 and sequence-wrap-first are superseded diagnostics for entity
  protection and wrap-boundary glyph measurement, not final acceptance.
- Contribution equivalents 738b4185/a450a195/da33ae6e/a53eb7af are prepared on
  feat/mermaid-cpp-port; this documentation checkpoint follows them. Runtime,
  tests, pins and workflow match the verified head. No AGENTS.md diff against
  PR base 508423f2 or upstream/master e9b48e4a. PR #1 remains open/unmerged:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from completed implementation checks.
- Next functional batch: Sequence numbering and participant visibility/footer
  options. Eight original observations across four diagrams/two themes are in
  build/mermaid-cpp/sequence-numbering-next/original.json, generated by
  sequence-numbering-next.mjs. Start with sequenceDiagram.jison autonumber,
  sequenceDb.ts sequenceIndex, and sequenceRenderer.ts numbering visibility,
  drawMessage, drawActors and footer handling. Off hides numbers but messages
  still advance the counter; notes/activation events do not advance it. Exit on
  those representative parser events and displayed numbers/participants plus
  affected checks, then continue required participant/family/application work.
- Limits: rich labels, numbering, extended participants, remaining required
  families and application integration remain. Folia still uses Rust in the
  application. Local MSVC/CMake are unavailable; no local native build or
  WinUI/GPU/DPI acceptance is claimed. The comparison corpus uses Arial 16/20,
  not arbitrary-font parity. No failed acceptance check, uncommitted candidate,
  local build, browser, agent or CI watcher remains active. Continue in the
  existing contribution worktree build/mermaid-cpp/entry-contribution.

## Previous checkpoint - 2026-10-25, Sequence control regions/backgrounds/titles verified

- Control-region commits 96252223/566aeafa passed all four Windows standalone/
  integrated Debug/Release jobs on codex/mermaid-cpp-sequence-controls:
  https://github.com/zhao0349/Folia/actions/runs/40165172369
  Delivered loop/opt/alt, par/and, critical/option and break regions with nested
  bounds, section separators, bracketed single-line labels and configured boxes,
  including contained notes, activations and self calls. Each job compared 32
  original/native Sequence SVGs across 16 diagrams and default/dark themes.
- Background/title commits 34687f6b/62d011b6 passed all four Windows standalone/
  integrated Debug/Release jobs on codex/mermaid-cpp-sequence-backgrounds:
  https://github.com/zhao0349/Folia/actions/runs/40165741294
  Delivered nested/transparent rect backgrounds behind the diagram and plain
  titles with the original viewport placement and root textColor. The final
  corpus compares 38 Sequence SVGs across 19 diagrams and both themes.
- Both runs passed the existing 92 Flowchart comparisons, source/SVG checks and
  real browser/DirectWrite comparisons. Integrated jobs passed FoliaTests,
  FoliaWindowsEditorTests and both Mermaid suites. Native previews were inspected
  in build/mermaid-cpp/sequence-controls-verified and sequence-backgrounds-final.
  Earlier run 40165376228 and sequence-backgrounds-verified are superseded
  diagnostics for the corrected dark-title paint mismatch, not final evidence.
- Contribution equivalents bbd61fa2/721c3ec7 and 64dda5c5/ca022820 are delivered
  on feat/mermaid-cpp-port. Implementation, tests, pins and workflow match verified
  head 62d011b63e7b305b610925950c2f4fe5f1c4a776. This documentation checkpoint
  follows them. No AGENTS.md diff against PR base 508423f2 or upstream/master
  190eb958. PR #1 remains OPEN/unmerged; do not merge:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from the completed checks above.
- Next functional batch: Sequence multiline/entity/wrapped display text. Eight
  original observations across four diagrams and two themes are ready in
  build/mermaid-cpp/sequence-display-next/original.json, generated by
  sequence-display-next.mjs. Start with utils.ts::{wrapLabel,breakString,
  calculateTextDimensions}, sequenceRenderer.ts::{calculateLoopBounds,
  buildMessageModel,buildNoteModel,adjustLoopHeightForWrap} and svgDraw.js::drawText.
  Preserve original line measurement, actor spacing and nested region bounds;
  use real DirectWrite widths. Exit on those representative displayed lines,
  boxes and paths plus affected checks, then move to numbering/participants and
  the remaining required family/application chain.
- Limits: current control labels fit a single plain line. General wrapping,
  multiline/entity/rich labels, numbering, extended participants, remaining
  required families and app integration remain. Local MSVC is unavailable;
  no local native build or WinUI/GPU/DPI acceptance is claimed. Folia still uses
  Rust in the application. No failed acceptance check, uncommitted implementation
  candidate, local build, browser, agent or CI watcher remains active. Continue
  in the existing contribution worktree build/mermaid-cpp/entry-contribution.

## Previous checkpoint - 2026-10-24, Sequence notes SVG and control-region parser verified

- Notes/activation/human-actor SVG: e1e870ed/e23bcd3a, validation branch
  codex/mermaid-cpp-sequence-notes. All four Windows standalone/integrated
  Debug/Release jobs passed:
  https://github.com/zhao0349/Folia/actions/runs/40103155651
- Control-region parser: 3e0a8320/d24e8a5a, validation branch
  codex/mermaid-cpp-sequence-regions. All four Windows standalone/integrated
  Debug/Release jobs passed:
  https://github.com/zhao0349/Folia/actions/runs/40103351239
  Fixtures cover
  22 successes and 6 common parse/DB errors, including nested regions, branch
  sections, activation/notes inside regions and incomplete editing states.
- Each job compared 20 original/native Sequence SVGs, including
  left/right/over notes, nested activation, clipped message endpoints and human
  actors in default/dark; the existing 92 Flowchart and browser/DirectWrite
  comparisons passed. Integrated jobs passed FoliaTests, FoliaWindowsEditorTests
  and both Mermaid suites. Downloaded native previews were inspected.
- Contribution equivalents 9b4f6dca/95d5cd26 and fe25739d/8a9821ae are delivered
  on feat/mermaid-cpp-port. Runtime, tests, source pins and workflow match
  d24e8a5a41c73fbbe1f7f6211d45b2387b8c8e0b. This documentation checkpoint
  follows them. No AGENTS.md diff against PR base 508423f2 or upstream/master
  81bed653. PR #1 remains OPEN/unmerged; do not merge:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from the completed checks above.
- No failed acceptance check, uncommitted implementation candidate, local build,
  browser, agent or CI watcher remains active. Continue in the existing
  contribution worktree build/mermaid-cpp/entry-contribution.
- Next functional batch is measured control-region SVG. Three representative
  loop, alt/else and nested opt/loop inputs in both themes are already measured
  in build/mermaid-cpp/sequence-controls-next/original.json, generated by
  sequence-controls-next.mjs. Start with sequenceRenderer.ts::{getLoopWidths,
  adjustLoopHeightForWrap,bounds.updateBounds,addSectionToLoop} and
  svgDraw.js::{drawLoop,drawLabel}; preserve nested bounds and measured bracketed
  labels. Exit on representative region/section/activation diagrams and affected
  checks, then continue the remaining required family/application chain.
- Evidence: build/mermaid-cpp/sequence-notes-verified (Debug native previews),
  sequence-regions-verified (Release comparisons) and sequence-notes-work
  (local original SVGs). No local MSVC build or WinUI/GPU/DPI acceptance is
  claimed. Folia still uses Rust in the application. Control-region rendering,
  wrapping/multiline/entity/rich labels, numbering, titles, extended participants,
  remaining required families and the application switch are still outstanding.

## Sequence parser and measured SVG verified (previous checkpoint)

- Delivered owned Sequence actors/messages through shared preprocessing and
  configuration: aliases, implicit participants, common arrows, self messages,
  notes, nested activation, wrap flags and plain title/accessibility metadata.
  Fourteen successful original/native parser/DB cases and three common failure
  cases passed. Source commits 377eef4e/2d07c483/e753cad8 passed all four Windows
  standalone/integrated Debug/Release jobs:
  https://github.com/zhao0349/Folia/actions/runs/40100026843
- Delivered the first measured Sequence source-to-SVG path through render_mermaid:
  classic rectangular participants, mirrored footers, lifelines, centered plain
  single-line messages, solid/dotted/open/bidirectional arrows, curved/right-angle
  self calls and configured spacing. Original actor-margin/message-height
  algorithms consume real DirectWrite widths and font ascent/descent. Browser
  generic sans-serif is explicitly mapped to Arial like the native resolver;
  the corpus uses Arial at 16/20 units, not an arbitrary-font parity claim.
- SVG source commits db9a1173/7e4f31b3/f49be6ab met their exit conditions at
  f49be6abae19141b06244001eb8c2b17a2a25e6d. All four Windows jobs passed:
  https://github.com/zhao0349/Folia/actions/runs/40100933496
  Each job compared 12 official/native Sequence SVGs across six diagrams and
  default/dark themes: actor boxes, lifelines, text, paths, marker geometry,
  paints and viewports. The 92 existing Flowchart comparisons and other source,
  SVG and real browser/DirectWrite checks passed. Integrated jobs also passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites. Downloaded final
  native previews were inspected. No local MSVC build or WinUI/GPU/DPI acceptance
  is claimed; Folia still uses Rust in the application.
- Both batches are pushed to feat/mermaid-cpp-port. Contribution equivalents:
  4cf16ea5/f75f0bee/947ddcac and deb7f20a/3d28e944/453efa56. Runtime, tests,
  pins and workflow match the final verified head. This documentation checkpoint
  follows those commits. No AGENTS.md diff against PR base 508423f2 or
  upstream/master 81bed653. PR #1 remains OPEN/unmerged; do not merge:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from completed implementation checks.
- Next functional batch: connect the already-parsed notes, activation events and
  human actor figures to measured Sequence SVG. Original functions are
  sequenceRenderer.ts::{buildNoteModel,drawNote,activationBounds,newActivation,
  activeEnd} and svgDraw.js::{drawActorTypeActor,drawActivation}. Keep activation
  events in message order; use their geometry to clip message endpoints. Exit on
  representative note/self-call/activation request-response diagrams in both
  themes and the affected native checks, then advance Sequence control regions
  and the remaining required family/application chain. Do not reopen passed
  Flowchart or plain Sequence stages without relevant changes or regressions.
- Next-stage original observations are already saved in
  build/mermaid-cpp/sequence-notes-next/observations.json, generated by
  sequence-notes-next.mjs. Promote those representative references first.
  Final evidence: build/mermaid-cpp/sequence-svg-verified (comparison.json under
  sequence-browser, native-preview.png under pie-svg); sequence-svg-work contains
  local original references. The earlier theme-test setup run 40099571308 and
  SVG-style run 40100367601 are superseded. Diagnostic copies in sequence-svg-first
  and sequence-svg-diagnostic are not final verification; do not rerun them.
- Limits: Sequence notes/activation/human shapes have parser support only;
  wrapping, multiline/entity/rich labels, titles and additional Sequence options
  still require SVG adapters. Control regions, numbering, extended participants,
  other required families and the app dependency switch remain. No failed check,
  uncommitted implementation candidate, local build, browser, agent or watcher
  remains active. Continue from this checkpoint and the existing contribution
  worktree build/mermaid-cpp/entry-contribution.

## Common HTML/Markdown labels verified (historical)

- Delivered common HTML node/edge normal, strong and emphasis runs, including
  nested strong/emphasis, explicit breaks, real DirectWrite wrapping and
  inherited label fonts. Owned parser labels are preserved. Exit conditions met:
  default/dark connected/grouped comparisons of characters, per-character styles,
  lines, boxes and paths plus the affected native suites. Group-title styling,
  arbitrary HTML and non-HTML layout mode are outside this batch.
- Batch 1 source commits 54a34fc4/b03c4fa9 passed all four Windows jobs:
  https://github.com/zhao0349/Folia/actions/runs/40096863186
  Each job compared 86 official/native SVGs; integrated jobs passed FoliaTests,
  FoliaWindowsEditorTests and both Mermaid suites. Contribution equivalents
  6ca16262/1591b6f4 are pushed to feat/mermaid-cpp-port. Implementation, tests,
  pins and workflow match the verified head; no AGENTS.md diff against either
  PR base 508423f2 or upstream/master 81bed653. Do not merge PR #1.
- Batch 2 source commits f8c78448/f0f4464b retain markdown parser labels/types and
  convert
  common star/underscore strong/emphasis, nesting and escaped punctuation to
  the same measured runs. Original parser fixtures, 8 original Markdown display
  observations and 92 browser SVG comparisons across 46 diagrams and two themes
  passed in all four standalone/integrated Debug/Release jobs:
  https://github.com/zhao0349/Folia/actions/runs/40097244193
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites; the existing source/SVG and real browser/DirectWrite checks passed.
  Contribution equivalents are 9f472293/0a919913. Runtime, tests, pins and
  workflow match verified head f0f4464b3013b9bb0814e50eb80bb829b77487ee exactly.
  This documentation checkpoint follows those commits. Both batches are
  delivered on feat/mermaid-cpp-port; PR #1 remains OPEN/unmerged:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from the completed verification above.
- Downloaded native HTML and Markdown previews were inspected. Ignored evidence
  lives in build/mermaid-cpp/rich-html-verified and rich-markdown-verified;
  rich-html-work/rich-markdown-work contain local original-browser observations.
  Initial runs 40096440489 (trailing-space wrapping) and 40097022130 (derived
  Dagre fixtures) are superseded by the fixes above. Do not repeat those runs.
- Next functional batch: Sequence actor/message DB, followed by its measured
  source-to-SVG adapter. Original source entry points are
  diagrams/sequence/{sequenceDiagram.ts,sequenceDb.ts,parser/sequenceDiagram.jison,
  sequenceRenderer.ts}. Two initial official request/response and activation/
  note/self-message observations are in build/mermaid-cpp/sequence-next.json,
  generated by sequence-next.mjs against the pinned original package. Promote
  representative parser/DB fixtures and implement owned actors/messages first;
  exit on original/native actor order, aliases, signals, notes and activation
  comparisons plus affected native tests, then advance directly to measured SVG.
  Keep the existing family inventory. Do not reopen passed Flowchart stages
  without a relevant regression or explicit project need.
- Limits: common node/edge HTML-label mode and default Markdown auto-wrap only.
  Group-title rich text/typography, remaining HTML/Markdown features, HTML titles,
  other style properties, shape metadata, other families and the application
  dependency switch remain. Folia still uses Rust in the app. No WinUI/GPU/DPI
  acceptance is claimed. Local MSVC/CMake remain unavailable; Windows CI is the
  native-build authority. No local build, agent or browser remains active.
  No failed acceptance check remains; no CI watcher is running locally.

## Flowchart paints and label typography verified (historical)

- Delivered common class/direct node, cluster and edge paints through
  render_mermaid: fill, stroke, text color, line width and dash patterns.
  Class styles precede direct styles. Colored point markers change fill/stroke;
  circle/cross keep the theme fill and change stroke. Marker selection retains
  the original first stroke declaration while paths use the final declaration.
  Dark group titles now use the theme's separate title color.
- Delivered node/edge label font family, positive pixel size, normal/bold or
  numeric 100-900 weight, and normal/italic style. DirectWrite wraps/measures
  with those settings; SVG retains the same resolved family and parameters.
  Existing Dagre, classic shapes, groups, paths and title placement remain.
- Both batches met their exit conditions. Paint head
  7fe29b269f404bbf9780caa73fe4d13099d7c4b5 passed all four Windows jobs:
  https://github.com/zhao0349/Folia/actions/runs/40094547510
  Font head 10172a532673edf45c9f761c352c028a98c5ca7c passed all four:
  https://github.com/zhao0349/Folia/actions/runs/40094859877
  Each run covers standalone/integrated Debug/Release. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites. Final jobs
  compared 80 original/native SVGs across 40 diagrams and two themes, including
  class/direct paint precedence, colored markers, label fonts, displayed lines,
  node/group boxes, positions and paths. Existing source/SVG and real browser/
  DirectWrite checks passed. Downloaded native previews were inspected.
- Source commits: 38b60703/7fe29b26 (paints), 15d6406d/10172a53 (fonts).
  Contribution equivalents 4e025129/d8a4fb2a/8a80a610/57e79544 are delivered on
  feat/mermaid-cpp-port in build/mermaid-cpp/entry-contribution. Runtime, tests,
  pins and workflow match the final verified head; this documentation checkpoint
  follows them. Contribution diffs against PR base 508423f2 and upstream/master
  81bed653 contain no AGENTS.md change. PR #1 includes both batches; keep
  OPEN/unmerged, DO NOT merge: https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from completed implementation checks.
- Next concrete batch: common mixed normal/strong/emphasis node and edge labels.
  Start from rendering-util/{handle-markdown-text.ts,createText.ts}, original
  HTML/Markdown observations, the retained parser label types, and the current
  DirectWrite/SVG label boundary. Keep owned source labels and add measured
  styled runs, with representative wrapping/explicit breaks in connected and
  grouped default/dark diagrams as the exit condition. Then advance remaining
  required families and the application call chain. Do not reopen passed
  geometry/paint/whole-label font stages without a relevant regression.
- Limits: group-title font overrides (original clusters apply styles after
  container sizing), mixed HTML/Markdown labels, HTML titles, other style
  properties, shape metadata, other required families and the application
  dependency switch remain. Folia still uses Rust in the application. No
  WinUI/GPU/DPI acceptance is claimed.
- No local build, agent, browser or CI watcher remains active. No failed check
  remains. Ignored evidence: build/mermaid-cpp/styles-verified and fonts-verified
  (downloaded Release artifacts, comparison.json and native-preview.png);
  styles-work/fonts-work contain original observations. The font validation
  branch codex/mermaid-cpp-font-styles remains at 10172a53. Local native builds
  remain unavailable; Windows CI supplies native verification.

## Classic markers and measured titles verified (historical)

- Delivered parsed circle/cross start/end markers through render_mermaid,
  including bidirectional and group-connected links in default/dark. Original
  marker reference points, dimensions, outlines and paints are retained; point
  marker strokes now match too. Classic circle/cross use marker positioning,
  not neo path offsets. Existing shape/cluster clipping and labels stay intact.
- Delivered plain frontmatter diagram titles using real 18px DirectWrite
  metrics and theme text color. Original insertTitle centers the title over
  the graph and places its baseline at -titleTopMargin; viewport bounds then
  expand to include ordinary/wide titles and custom margins on grouped graphs.
- Both batches met their exit conditions. Marker head
  f48643f3f3d798b91290673ff7e49655f08966b6 passed all four Windows jobs:
  https://github.com/zhao0349/Folia/actions/runs/40092570628
  Final title head ca5d84c236132951d646e551bbd4a4e2caecf51c also passed all four:
  https://github.com/zhao0349/Folia/actions/runs/40092789707
  Each run covers standalone/integrated Debug/Release. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites. Final jobs
  compared 66 original/native SVGs across 33 diagrams and two themes, including
  marker geometry/references/paint, title anchors/size/color and viewport bounds,
  plus existing labels, shapes, groups and paths. Existing source/SVG and real
  browser/DirectWrite checks passed. Downloaded native previews were inspected.
- Small source commits: 903800e3/f48643f3 (markers), 4f4c7426/ca5d84c2 (titles).
  Contribution equivalents 05dbc4b7/92d0f083/5a41153b/301c2d04 are pushed to
  feat/mermaid-cpp-port in build/mermaid-cpp/entry-contribution. Preview-only
  follow-up e4ff1664/df955e07 isolates SVGs as images in composite screenshots
  so their marker IDs do not collide; its 66-case comparison passed locally.
  Runtime code, native tests, pins and workflow match the final verified head.
  This documentation checkpoint follows them. Upstream PR #1 contains both
  batches; keep OPEN/unmerged, DO NOT merge:
  https://github.com/water2004/Folia/pull/1
  Contribution diffs against PR base 508423f2 and upstream/master 81bed653 have
  no AGENTS.md change. Delivery-triggered reruns are separate from the completed
  implementation validation above.
- Next concrete batch: common explicit node/edge/class styles already parsed
  by Flowchart. Start with shapes/handDrawnShapeStyles.ts::compileStyles and
  styles2String, the classic shape/cluster paint paths and edgeMarker.ts colored
  markers. Preserve compiled-class then direct-style precedence. Scope the first
  batch to actual fill/stroke/text-color/line-width/dash properties in connected
  and grouped diagrams; exit on representative default/dark source-to-SVG
  comparisons and affected native checks. Then continue needed styled labels,
  other required families and the application call chain. Do not reopen passed
  shape/layout/plain-label/marker/title stages without a relevant regression.
- Limits: custom styles, styled HTML/Markdown labels and HTML titles, shape
  metadata, other required families and application dependency switching remain.
  Folia still uses Rust in the application. No WinUI/GPU/DPI acceptance claimed.
- No local build, agent, browser or CI watcher remains active. No failed check
  remains. Ignored evidence: build/mermaid-cpp/markers-verified and titles-verified
  (downloaded Release artifacts, comparison.json and native-preview.png);
  markers-work/titles-work contain original-browser observations. Local native
  builds remain unavailable; Windows CI supplies the native verification.

## Measured display labels verified (historical)

- Delivered quoted/entity plain labels, explicit breaks and automatic wrapping
  through render_mermaid. Parser labels remain unchanged while the separate
  display adapter derives lines. DirectWrite supplies actual UTF-16 line breaks,
  widths and baselines; the existing shapes, Dagre and clusters consume those
  boxes. Node labels use wrappingWidth, edges use createText's 200px default,
  and plain group titles retain createLabel's unbounded width.
- Exit conditions passed at 2866962929106c59962a95d1181c9c3471646d8f:
  https://github.com/zhao0349/Folia/actions/runs/40067659200
  All four standalone/integrated Windows Debug/Release jobs succeeded.
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. Each job compared 54 original/native SVGs across 27 diagrams and two
  themes, including actual display lines, label centers, geometry, groups,
  paints and sampled paths. Existing source/SVG and browser/DirectWrite checks
  passed. Downloaded native previews were inspected locally.
- Source batches: 2e8d066c/b5ccaee5 (display labels), a806ae47 (wrapping),
  28669629 (test-session initialization). Contribution equivalents
  719b9ad1/adee4d44/5fa16088/b2516bd2 are pushed to feat/mermaid-cpp-port in
  build/mermaid-cpp/entry-contribution. Implementation/tests/workflow match the
  verified head exactly. This documentation/provenance checkpoint follows them;
  the added createLabel.js source hash was also verified locally.
  Upstream PR #1 includes the batches and its description is updated; confirmed
  OPEN/unmerged. The contribution diff against both PR base 508423f2 and
  upstream/master contains no AGENTS.md change. DO NOT merge:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from the completed validation above.
- Next concrete batch: connect the already parsed arrow_circle/arrow_cross
  markers to SVG. Follow rendering-elements/{markers.js,edgeMarker.ts} and
  utils/lineWithOffset.ts, retaining shape/cluster clipping and label placement.
  Exit on representative start/end/bidirectional markers in connected/grouped
  default/dark diagrams and affected native checks. Then continue required
  diagram titles/styles, rich labels, other families and the app call chain.
  Do not reopen passed shape/layout/label stages without a relevant regression.
- Limits: common/numeric entities and plain labels in the tested HTML-spacing
  mode; styled HTML/Markdown, custom styles, additional markers, diagram titles,
  other required families and application dependency switching remain. Folia
  still uses Rust in the application. No WinUI/GPU/DPI acceptance is claimed.
  Original and native multiline group titles may overlap a child at default
  margins; existing subGraphTitleMargin controls this spacing. The batch retains
  the original placement rather than changing cluster layout semantics.
- No local build, agent or browser remains active. Final ignored evidence:
  build/mermaid-cpp/labels-verified, including flowchart-browser/comparison.json
  and native-preview.png. labels-work/wrapping-work hold original observations.
  labels-initial and run 40067362147 are superseded: the new core test failed
  before label assertions because its session lacked Folia's theme initialization;
  28669629 fixed it and the final run passed. Run 40067601320 was superseded by
  the corrected head. No failed acceptance check remains. Local MSVC/CMake are
  unavailable; native builds use Windows CI. Its integrated Release job took
  about 17 minutes, consistent with recent successful runs.

## Measured classic shapes verified (historical)

- Delivered all classic node shapes accepted by the current native Flowchart
  parser through render_mermaid. New adapters cover stadium/subroutine/cylinder,
  hexagon, both slanted parallelograms, both trapezoids, asymmetric notch and
  double circle. They retain original measured dimensions, sampled stadium arcs,
  side frames, cylinder caps/label offset, polygon intersections and double rings.
  Existing Dagre placement, recursive groups and self loops remain in use.
- Exit conditions passed at e2527ad9b0bc2a105570d998af2e3937abde31c9:
  https://github.com/zhao0349/Folia/actions/runs/40065755650
  All four Windows standalone/integrated Debug/Release jobs succeeded.
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. Each job compared 44 original-browser/native SVGs across 22 diagrams
  and two themes, including painted outlines, label centers, edge clipping,
  groups, self loops and minNodeWidth: 0. Existing source/SVG and real browser/
  DirectWrite checks passed. Native previews were inspected locally.
- Small source batches: 6c11e387/d411e656 (three measured shapes), then
  0692166f/e2527ad9 (remaining parsed classic shapes). The first batch also
  passed all four jobs in run 40065410266. Contribution equivalents are
  2a01cf32/a583aad3/a1cf0272/28e429cd in build/mermaid-cpp/entry-contribution
  on feat/mermaid-cpp-port. Implementation/tests/pins/workflow match the final
  verified head exactly; this public documentation checkpoint follows them.
  The contribution diff against both PR base 508423f2 and upstream master
  81bed653 contains no AGENTS.md change. Upstream PR #1 includes both batches;
  its description is updated and it is confirmed OPEN/unmerged. DO NOT merge:
  https://github.com/water2004/Folia/pull/1
  Delivery-triggered reruns are separate from the completed validation above.
- Next concrete batch: Flowchart display-label adaptation and measured line
  layout. Start from pinned shapes/util.ts::labelHelper, createText.ts and
  decodeEntities/sanitizeText, then the current platform render_mermaid label
  measurement path. Preserve source/DB labels while deriving displayed text;
  measure actual lines and feed resulting boxes/baselines into existing shapes.
  Exit on representative quoted/entity labels, explicit line breaks and wrapped
  labels in connected/grouped diagrams and affected native checks. Continue
  required styles/titles/markers, other families and the application call chain.
  Do not reopen passed shape/layout stages without a relevant change/regression.
- Limits: plain single-line labels; rich/wrapped labels, custom styles, extra
  markers, diagram titles, shape metadata, other required families and the
  application dependency switch remain. Folia still uses Rust in the app.
  No WinUI packaging, GPU or DPI acceptance is claimed.
- No local build, agent or browser remains active. Ignored evidence is under
  build/mermaid-cpp/classic-shapes-verified and classic-polygons-verified,
  each with flowchart-browser/comparison.json and native-preview.png. The
  corresponding *-work directories contain local original-browser observations.
  No failed acceptance check remains. Local MSVC/CMake are unavailable;
  Windows CI supplies native builds and local previews inspect downloaded SVGs.

## Measured recursive cluster SVG verified (historical)

- Delivered recursive Flowchart group rendering through render_mermaid: real
  title measurement, independent/nested directions, parent cluster reinsertion,
  parent rank-spacing + 25, default/dark cluster paints, title margins, external
  group endpoint clipping and contained self loops. Existing Dagre stays intact.
- Exit conditions passed at f7fb19cdf72157a26d34e1175bc58b3cc2983f4f:
  https://github.com/zhao0349/Folia/actions/runs/40054241033
  All four Windows standalone/integrated Debug/Release jobs succeeded.
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. Each job compared 34 original-browser/native SVGs across seventeen
  diagrams and two themes: absolute nested coordinates, group boxes/title
  positions, node/edge labels, paints, sampled paths and viewport containment.
  Existing source/SVG and browser/DirectWrite checks passed. Final native
  group and title-margin previews were inspected locally.
- Source batches: be797b81 (browser observations/pins), 5e325e41 (recursive
  measurement/painting), 33157a3b (title-margin regression case), f7fb19cd
  (corrected label offsets). Contribution equivalents: a8e09b8a, 9d0f1064,
  43159c9d, 61f9f349. The contribution worktree build/mermaid-cpp/entry-contribution
  on feat/mermaid-cpp-port matches the verified implementation/tests/pins/workflow.
  This public documentation checkpoint follows them. Contribution diffs against
  both the reported PR base 508423f2 and fetched upstream master 81bed653 contain
  no AGENTS.md change. Target https://github.com/water2004/Folia/pull/1 remains
  OPEN; DO NOT merge. Delivery-triggered reruns are distinct from the completed
  implementation validation linked above.
- Next concrete batch: measured classic stadium/subroutine/cylinder SVG for
  shapes already parsed by Flowchart. Start with pinned rendering-elements/
  shapes/{stadium,subroutine,cylinder}.ts, labelHelper and their intersection
  functions. Deliver measured outlines and clipped edges; exit on representative
  connected and grouped diagrams in default/dark plus affected native checks.
  Then continue remaining required label/shape adapters, other required families
  and the application call chain. Do not reopen passed cluster/layout stages
  without a related change, actual regression or explicit project requirement.
- Limits: plain single-line group titles; rich/wrapped labels, other shapes/
  styles, diagram titles, other required families and the application dependency
  switch remain. Folia still uses Rust in the application. No WinUI packaging,
  GPU or DPI acceptance is claimed.
- No local build, agent or browser remains active. Final ignored evidence is
  build/mermaid-cpp/cluster-svg-final, including flowchart-browser/comparison.json,
  native-preview.png and title-margins-preview.png. cluster-svg-initial and
  run 40053446633 are superseded. Despite its directory name, cluster-svg-verified
  holds the FAILED run 40053809086 artifact: its only mismatch was the 5px
  recursive label offset in both title-margin themes, resolved by f7fb19cd and
  the final passing run. cluster-svg-work contains local oracle/diagnostic files.
  No failed acceptance check remains. Local MSVC/CMake are unavailable; Windows
  CI supplies native builds and the local browser inspects downloaded outputs.

## Cluster preparation and self-loop SVG verified (historical)

- Delivered owned prepare_flowchart_graph around the retained Dagre engine:
  cluster endpoint anchors, independently directed subgraph extraction and
  three-segment self-loop input, with source indexes and cluster markers.
  Fifteen original prepareLayoutForDagre/mermaid-graphlib observations match
  native output for nested, independent, externally connected and loop graphs.
- Connected graph preparation to measured ungrouped SVG. Self loops now use
  the original dummy labelRect boxes, placement hint, compact logical path and
  label offsets. Ordinary shape intersections also apply the original label
  midpoint correction, fixing the observed decision-graph label offset.
- Exit conditions passed at 13fb6c067f2821e258d28edbdc9106d6839da3a8:
  https://github.com/zhao0349/Folia/actions/runs/39970098266
  All four Windows standalone/integrated Debug/Release jobs succeeded.
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. Each job compared 22 real browser/native SVGs across eleven diagrams
  and two themes, including four self-loop directions, labels, viewport
  containment, node geometry, paints and sampled paths. Existing source/SVG
  and browser/DirectWrite checks passed; final native previews were inspected.
- Source batches: b543c633 (oracle), 753dfdd2 (graph preparation), 29011730
  (self-loop SVG), 13fb6c06 (intersection label correction). Contribution
  equivalents: 7d65a098, b89706d2, 7a406fa8, 700e2dc0. The contribution worktree
  build/mermaid-cpp/entry-contribution on feat/mermaid-cpp-port matches the
  verified implementation/tests/pins/workflow. This documentation checkpoint
  follows it. The upstream diff against 508423f2 contains no AGENTS.md change.
  Upstream PR #1 now includes these batches and its description is updated;
  confirmed OPEN/unmerged after delivery. DO NOT merge. Delivery-triggered
  reruns are separate from the completed validation linked above.
- Next concrete stage: measured recursive cluster SVG using the prepared graph
  tree. Follow measureDagreGraph/renderDagreSubgraph (parent rank spacing + 25,
  parent-cluster reinsertion and title margins), clusters.js, positionNode and
  edge cutPathAtIntersect. Validate representative independent/nested groups
  and external group endpoints, then proceed to remaining label/shape adapters.
  Do not reopen passed graph/layout stages without a related change/regression.
- Limits: grouped SVG is still explicitly rejected; graph extraction alone is
  not cluster painting. Rich/wrapped labels, remaining shapes/styles, titles,
  other required families and the application dependency switch remain.
  Folia still uses Rust in the application. No WinUI/GPU/DPI acceptance claimed.
- No local build, agent or browser process remains active. Final ignored
  evidence: build/mermaid-cpp/cluster-loop-verified, including comparison.json
  under flowchart-browser and native-preview.png. loop-check and run
  39968512435 are superseded diagnostics; its label failure is resolved by the
  final passing run. The intermediate graph-only run 39967598357 was superseded
  by combined validation, not used as final acceptance. No failed check remains.

## Flowchart coordinates and initial measured SVG verified (historical)

- Delivered the retained Dagre graph through crossing reduction/order, BK
  coordinates, compound borders, direction transforms and restored edge paths.
  The 19 original dagre-d3-es 7.0.14 observations match native ranks/orders,
  node/group boxes and paths, including parallel/self edges, cycles and groups.
- Connected source preparation -> real DirectWrite measurement -> native layout
  -> SVG through render_mermaid. Current SVG scope: explicit Dagre/classic,
  default/dark, ungrouped square/rounded/diamond/circle nodes, plain single-line
  labels, basis/linear paths, point/plain arrow ends and basic stroke styles.
  The full Dagre layout and the initial Mermaid SVG adapter have distinct scope.
- Exit conditions passed at 7a45f1bbeb4d9e1fb81e39fc535b347fd607bfbc:
  https://github.com/zhao0349/Folia/actions/runs/39962169371
  All four Windows standalone/integrated Debug/Release jobs succeeded.
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. Each job compared 14 real browser/native SVGs for measured node boxes,
  coordinates, text containment, paint and sampled edge geometry, plus existing
  source/SVG and browser/DirectWrite checks. Final native preview was inspected.
  Earlier Dagre-only validation also passed in run 39956107352 at fbcb56a3.
- Source batches: 4e0d308e (oracle), fbcb56a3 (layout), ebcb6588 (measured SVG),
  followed by focused paint/configuration and test fixes through 7a45f1bb.
  The omitted-htmlLabels failure is fixed using the original root/family/default
  precedence. Test initialization and MSVC lambda failures are resolved; no
  failed acceptance check remains from these superseded diagnostic runs.
- Contribution worktree: build/mermaid-cpp/entry-contribution, branch
  feat/mermaid-cpp-port. Selected implementation head 27d572b8 matches the
  verified Mermaid code, tests, source pins and workflow exactly; this final
  public documentation checkpoint follows it. The diff against upstream
  master 508423f2 contains no AGENTS.md changes. Upstream PR #1 is updated
  with this batch and confirmed OPEN/unmerged; DO NOT merge. Delivery can trigger automatic
  reruns, separate from the completed implementation validation above.
- Next concrete stage: Mermaid cluster endpoint/extraction and independently
  directed subgraphs, then self-loop SVG adaptation, using the original
  rendering-util/layout-algorithms/dagre/{index.js,mermaid-graphlib.js} path.
  Keep the verified Dagre graph; add the required Mermaid adapters around it.
  Continue the needed label/shape/style adapters afterward. Do not reopen
  passed layout/configuration stages without a relevant change or regression.
- Limits: SVG clusters, self loops, rich/wrapped labels, remaining shapes/styles
  and titles still return explicit adapter errors. Other required families and
  the application dependency switch remain outstanding; Folia still uses Rust
  in the application. No WinUI packaging, GPU or DPI acceptance is claimed.
- No local build, agent or browser probe is active. Final ignored evidence is
  build/mermaid-cpp/flowchart-verified (downloaded standalone Debug artifacts),
  with flowchart-browser/native-preview.png and comparison.json. Earlier
  flowchart-check-880 and flowchart-chrome artifacts are diagnostic only.

## Dagre rank pipeline verified (historical)

- Delivered the first native Flowchart layout stage: DFS back-edge reversal,
  compound nesting constraints, default network simplex, edge-label rank
  reservation and rank normalization. rank_flowchart consumes the existing
  owned Flowchart input and caller-supplied edge label boxes. It returns node
  ranks, group intervals and reversal/self-loop/label-rank metadata without
  mutating the input. Parallel edges aggregate only inside the optimizer.
- Exit conditions passed at b80d8b9f7dee79e9d2fcb4de7886197d46ddd4e6:
  https://github.com/zhao0349/Folia/actions/runs/39951056843
  All four Windows standalone/integrated Debug/Release jobs succeeded.
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. All jobs checked original source/SVG oracles and browser/DirectWrite
  measurements. The unchanged dagre-d3-es 7.0.14 rank pipeline supplies the new
  representative chain/branch/cycle/parallel-edge/nested-group observations.
- Two source commits: 5ada03ea (official oracle/source pins), b80d8b9f (native
  ranking, tests, attribution, CI and documentation). Selected contribution
  commits: 14b8d474 and e925802d, followed by this documentation checkpoint.
  Contribution worktree: build/mermaid-cpp/entry-contribution on
  feat/mermaid-cpp-port. Its Mermaid code/tests/docs/workflow match the verified
  tree; the upstream diff against 508423f2 contains no AGENTS.md change.
  Update https://github.com/water2004/Folia/pull/1 and keep OPEN; DO NOT merge.
  Delivery pushes can trigger automatic reruns; the completed evidence above
  is for the implementation head, not those later documentation reruns.
- Next concrete stage: retain the private ranked graph through Dagre long-edge
  normalization and dummy parenting (normalize.js, parent-dummy-chains.js),
  group side borders (add-border-segments.js), crossing reduction (order/), and
  coordinate assignment (position/bk.js and layout.js). These source paths are
  under the locked dagre-d3-es package. Do not reconstruct a second graph from
  the public rank summary or reopen passed configuration/Packet/Pie stages.
  Use the same representative diagrams and explicit measured boxes for the
  next original-layout oracle, then connect actual measurement and SVG.
- Limits: this is Dagre ranking, not complete Mermaid cluster adaptation or
  Flowchart rendering. Group endpoints fail explicitly; independently directed
  cluster extraction, self-loop rendering and coordinates remain pending.
  The new oracle boxes are supplied layout inputs, not font measurements.
  Other required families and the final Rust/application dependency switch
  remain outstanding. No WinUI packaging or DPI acceptance is claimed.
- No local compiler/build/agent is active, and no new ignored implementation
  candidate or downloaded runner was created. The validation branch is
  codex/mermaid-cpp-dagre-rank; the workflow was dispatched explicitly because
  initial branch creation did not start a push run. No test failure is pending.

## Flowchart parser and layout input verified (historical)

- Delivered common Flowchart source through shared preparation/configuration to
  owned nodes, edges and compound-graph layout input. Scope: classic shapes,
  labelled/chained/grouped links, subgraphs, direction and basic style classes.
- Exit conditions passed: original parser/DB/getData observations match native
  output; all four Windows standalone/integrated Debug/Release jobs passed at
  53900985625fd7a883d82c318eefd53c77aea83c:
  https://github.com/zhao0349/Folia/actions/runs/39927427317
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. All jobs checked source/SVG oracles and browser/DirectWrite measurement.
  No WinUI packaging or DPI acceptance is claimed.
- Next stage: official Dagre layout and measured Flowchart SVG; other required
  families and the final application dependency switch remain outstanding.
  Start from flowRenderer-v3-unified.ts and rendering-util/layout-algorithms/
  dagre/{index.js,mermaid-graphlib.js}; the official dependency is dagre-d3-es
  7.0.14 (already pinned in the test-only pnpm lock). Extract its measured-node
  graph layout oracle, then port the required rank/order/position path. Use
  representative chains, branches, cycles and compound groups as exit cases;
  do not reopen completed configuration or Packet/Pie stages without a regression.
- No Flowchart rendering or application integration is claimed by this stage.
- Original parser/DB/getData oracle: 20 representative successful inputs and
  four common parse/DB errors. All source observations matched at 30948524.
  Run 39925896588 exposed dotted infix endings (`A -. text .-> B`), corrected
  in 30948524. Run 39926552864 then exposed incorrect test initialization:
  untouched Flowchart config selects unported redux-color. 53900985 sets the
  explicit default theme used by Folia; public session semantics stay intact.
- Delivery uses worktree build/mermaid-cpp/entry-contribution on
  feat/mermaid-cpp-port, selected commits 047827b2, a88bfa49, 7175d829, f95a016f,
  followed by this documentation-only checkpoint. Source batches were
  b3aa1019, 4ae58955, 30948524 and 53900985.
  Its Mermaid code/docs/fixtures/workflow match the validation branch, and its
  diff from upstream 508423f2 contains no AGENTS.md changes.
  Update https://github.com/water2004/Folia/pull/1 and keep it OPEN; DO NOT merge.
  Delivery pushes may trigger automatic reruns; the completed evidence above
  is for the verified implementation/test head, not those later reruns.
- Stage limits: explicit default/dark appearance; classic syntax and plain/
  quoted labels. Rich text/entity adaptation, shape metadata, explicit edge ids
  and per-edge interpolation commands remain unimplemented; the required
  Flowchart family has not been reduced to the current representative corpus.
- No local compiler/build/agent is active. Downloaded runners in flowchart-check
  and flowchart-fixed-check and edited fixtures in flowchart-diagnostic under
  build/mermaid-cpp are diagnostic artifacts from superseded heads, not final
  acceptance evidence.

## Packet/Pie render entry verified (historical)

- Delivered the opt-in folia.platform.mermaid_renderer::render_mermaid entry:
  source -> shared preparation -> Packet/Pie configuration and theme -> family
  parsing/layout -> installed font selection and real DirectWrite measurement
  -> SVG with intrinsic dimensions or an explicit error. The core render_entry
  module also exposes owned intermediate data for platform-independent hosts.
- This phase's exit conditions passed at implementation head
  118e021ae47c531b6a78ed857ec9f663a8d0d5f2:
  https://github.com/zhao0349/Folia/actions/runs/39921739398
  All four standalone/integrated Windows Debug/Release jobs passed. Integrated
  jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid suites; all
  jobs checked the source oracles, existing SVG DOM comparisons and real
  browser/DirectWrite measurements. No WinUI packaging/DPI acceptance is claimed.
- The new original-runtime oracle covers 18 representative source/config/theme
  cases and final SVG dimensions. Native tests cover the family adapter and
  request failures; platform tests exercise actual font resolution, default/dark
  rendering, bottom legends, donuts, data labels and entity display text.
- Packet retains its official family defaults unless themeVariables.packet
  overrides them. Pie consumes effective themeVariables and grammar showData.
  Source entity spelling remains intact through DB deduplication; body titles
  override frontmatter titles and plain accessibility metadata is emitted.
- First run 39920905524 revealed that the official dark theme omits pie12.
  Fix 118e021a preserves an absent palette entry and omits fill/stroke attributes
  exactly as the original D3 draw does. That failure is closed by the full run
  above; no further verification expansion is pending for this batch.
- Small implementation batches: 44fd7570 oracle, e246cbcb core adapter/styles,
  8583c931 Windows source-to-SVG entry, 118e021a absent-palette fix. Selected
  contribution commits are 886a8204, b2c9d4e5, 6da4a668 and 82bfeca2, followed
  by this documentation-only checkpoint. Functional code/fixtures/workflows
  match the verified tree. Contribution diff against upstream base 508423f2
  contains no AGENTS.md change. Upstream PR #1 must remain OPEN; DO NOT merge:
  https://github.com/water2004/Folia/pull/1
- Actual default/dark SVGs were visually inspected: titles, legends, values and
  quote/star entities display correctly. Ignored evidence is under
  build/mermaid-cpp/entry-verified (preview.png and pie-svg/entry-*.svg).
  OLD runners and modified diagnostic fixtures under entry-check and
  entry-platform-check are superseded, not final acceptance evidence.
- Limits: plain-text metadata, numeric/px style sizes, literal colors and
  documented decimal/common named entities. No HTML/DOM/CSS execution or
  event binding layer. README records the supported font-list behavior.
- No live local build, browser or agent. The contribution worktree is
  build/mermaid-cpp/entry-contribution on feat/mermaid-cpp-port. Delivery pushes
  may trigger automatic reruns; the completed evidence is the run linked above.
- Next concrete function stage: start the required Flowchart parser/DB and
  layout-input boundary with representative Folia diagrams and the pinned
  original algorithms. Keep the existing required-family inventory intact.
  Other family renderers and final application integration remain; the active
  application dependency is still Rust. Do not reopen completed Packet/Pie
  checks without a related change or an actual regression.

## Shared frontend verified (historical)

- Delivered an owned preparation API composing normalization, frontmatter,
  directives, configuration scope and final builtin detection for Folia's
  text-to-SVG path. Preserve raw/cleaned/comment-bearing source, title and
  frontmatter line offset. This batch stops before parser/renderer execution.
- Scope: the original preprocessDiagram and processAndSetConfigs sequence,
  using existing native components and default/dark configuration sessions.
  Validate representative Packet/Pie and existing family detection inputs,
  configuration precedence, common incomplete input and request isolation.
  Exit conditions passed: original composed oracle plus native standalone/
  integrated Windows Debug/Release checks. Next: Packet/Pie renderer entry integration.
- Verified code/fixture head: e9b7827e76d37889066b46ff8c5d090dec237472 on
  codex/mermaid-cpp-frontend. Run 39917035248 passed all four Windows jobs:
  https://github.com/zhao0349/Folia/actions/runs/39917035248
  Integrated jobs passed FoliaTests, FoliaWindowsEditorTests and both Mermaid
  suites. All jobs checked source/SVG oracles, 78 native SVG DOM comparisons
  and 96 browser/DirectWrite measurements. No WinUI packaging/DPI acceptance.
- Delivery: normal fast-forward from 31770996 to the verified feature commits
  plus this documentation-only checkpoint on feat/mermaid-cpp-port; update
  https://github.com/water2004/Folia/pull/1 and keep it OPEN. DO NOT merge.
  Upstream base remains 508423f2. No AGENTS.md changes in the contribution.
  Delivery/checkpoint pushes can start automatic reruns; the completed evidence
  above is for the tested code and fixture head, not those later reruns.
- Composed oracle: 49 representative inputs, local check passes. Native
  prepare_diagram implementation and fixture tests compile in Windows CI.
  Init/wrap scanning retains a request-local cursor after malformed JSON;
  independent calls remain isolated. No local native compiler, build or agent.
- Small delivery commits: 9c7d3659 original composition oracle; 81235d0b native
  frontend and request isolation; e9b7827e pre-sanitization oracle capture.
- Initial run 39916287900 exposed four oracle capture mismatches: original
  addDirective mutates the extracted object. Capture extraction before that
  call; effective renderer configuration already matched in every case.
  The unchanged Release runner passes both frontend tests (708 assertions)
  with the corrected fixture; the full passing CI above verifies that correction.
  Downloaded runner: build/mermaid-cpp/frontend-check/FoliaMermaidTests.exe.
- Next adapter evidence: PacketDB.getConfig overlays default/global packet
  settings and adds showBits padding (already done by native layout rendering).
  Pie draw clean-merges default/global pie settings; its style and palette read
  effective themeVariables. Bridge those inputs to existing native renderers,
  including derived HSL/RGB paints and resolved real font measurement. Preserve
  commonDb metadata/entity handling as part of the source-to-render entry work.

## Registry and detector dispatch verified (historical)

- User authorizes public fork zhao0349/Folia pushes and verified updates to
  https://github.com/water2004/Folia/pull/1; keep OPEN, DO NOT merge.
  Verification branch: `codex/mermaid-cpp-registry`. Delivery branch:
  `feat/mermaid-cpp-port`. Upstream base remains `508423f2`.
- Read the pinned original detectType.ts, diagram-orchestration.ts and all
  detector modules. Selected boundary: detector registration, ordered dispatch,
  builtin registration guard/large-feature switch, loader route lookup. Actual
  lazy imports, diagramAPI styles/utils and renderer execution are separate.
- Original oracle executes full detectType/addDiagrams bodies and unchanged
  detector functions. Important behaviors: overwrite keeps order; numeric keys
  enumerate first; Object.entries snapshots callbacks; ELK changes config.layout;
  C4 non-first alternatives are unanchored. No grammar validation at detection.
- Oracle commit `767078c1`: 8,084 detection observations and 39 registry steps;
  local generation/check pass. All 37 detector modules and orchestration pinned.
- Native diagram_registry module implements ordered registration, all builtin
  predicates and once-only setup, config-copy dispatch, callback snapshots and
  loader route lookup. Tests consume original observations and cover ownership,
  isolation, resource budgets and transactional failure. README states limits.
- Verified code head `26a4842991108244635482656c7c8a63da3b5e14`:
  https://github.com/zhao0349/Folia/actions/runs/39910892034 passed all four
  standalone/integrated Windows Debug/Release jobs. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites; standalone jobs
  passed both Mermaid suites. All jobs checked original source/SVG oracles,
  78 native SVG DOM comparisons and 96 browser/DirectWrite measurements.
  Local source-text, preprocessing and detector oracle checks also pass.
- Self-review found no outstanding defects. Native restrictions are mapping
  config inputs and no Object.prototype ids; callback captures/work/side effects
  remain host-owned. Detection failures expose no partial configuration. Builtin
  registration rolls back on budget failure. Loader routes are opaque metadata;
  they do not execute imports or establish renderer availability.
- Small batches: `767078c1` official oracle/source pins; `26a48429` native
  registration, dispatch and tests; this final documentation checkpoint changes
  no tested code or CI. Delivery uses a normal fast-forward from `02870740`.
  Checkpoint/delivery pushes can trigger new CI; recheck live heads and checks
  on recovery rather than claiming those later automatic reruns have completed.
- Next concrete work: establish an original shared-frontend sequencing oracle
  before composing preprocessing, configuration sessions and detection. Renderer
  loader/styles/utils integration, remaining themes and family callbacks, other
  diagram renderers and application dependency replacement remain separate.
  No WinUI packaging or DPI acceptance is claimed.
- No live local builds/agents or uncommitted implementation candidates. No new
  ignored candidate directory or local runner was created for this batch. Local
  MSVC/CMake remain unavailable; native evidence is the Windows run above.

## Configuration sessions verified (historical)

- User authorizes public fork zhao0349/Folia pushes and verified updates to
  https://github.com/water2004/Folia/pull/1. Keep OPEN; DO NOT merge.
- Verification branch `codex/mermaid-cpp-session`; delivery branch
  `feat/mermaid-cpp-port`. Upstream base remains `508423f2`. Baseline delivery
  CI 39901224923 passed before this batch.
- Verified code head `c61bda4e1da00f10a165e6f5ec0ca951464d534b`:
  https://github.com/zhao0349/Folia/actions/runs/39908290555 passed all four
  standalone/integrated Windows Debug/Release jobs. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites; standalone jobs
  passed both Mermaid suites. All jobs checked official source/SVG oracles,
  78 native SVG DOM comparisons and 96 browser/DirectWrite measurement cases.
  This final checkpoint commit changes documentation only, not tested code/CI.
- Implemented original config.ts lifecycle with owned per-session state,
  default/dark re-derivation, scoped appearance, secure directive replay,
  initialize snapshots and reset. No global singleton or renderer integration.
- Oracle executes the complete pinned config.ts in fresh scopes with original
  dependencies: 101 scenarios / 640 steps, with a passing local oracle check.
  Compare full JSON-data state via lossless recursive deltas,
  not selected properties. Defaults omit undefined values and family callbacks;
  those callbacks are never invoked by this session boundary.
- Native restrictions explicitly exclude unported theme derivation,
  non-mapping command inputs, object arrays and containers below depth two
  that would require JS cross-layer identity; secure keys must be string lists.
  Native failures are transactional; caller inputs/getter results own their data.
- Small commits: `5f25dad7` official oracle/default data; `653cacbe` native
  lifecycle and ownership/budget tests; `4354acb7` callback guard and fixture
  transport; `c61bda4e` getter copy semantics. Getters perform assignWithDepth
  copies, preserving skipped null fields and primitive array union. Bounded
  copy failures return expected errors rather than exposing raw stored trees.
- Original defaults include 25 font callbacks. A generated table rejects container overrides that
  would otherwise change assignWithDepth's callable-vs-object behavior; scalar
  replacements remain supported. Fixture JSON escapes directive delimiters and
  line separators solely for transport; decoded session inputs remain exact.
- Next concrete work: original diagram registry/detector dispatch oracle and
  native dispatch. Shared frontend wiring, remaining theme derivations, family
  font callbacks/effective-htmlLabels helpers and renderer theme integration
  remain separate boundaries. Other families and application dependency
  replacement remain pending; no WinUI packaging or DPI acceptance is claimed.
- No live local builds/agents or uncommitted implementation candidates. Local
  MSVC/CMake remain unavailable; native evidence is the Windows CI above.
  Ignored session-review and session-getter-review contain OLD Release runners
  (653cacbe and 4354acb7) and diagnostic fixtures/logs, not the final binary.
  The latter exposed 12 getter mismatches before the fix. Superseded runs
  39905882983 and 39906574476 are not pending acceptance work. A delivery push
  can trigger another automatic run; inspect live checks on recovery rather
  than asserting that the later documentation run has completed.

## Init mapping and clean merge verified (historical)

- User authorizes public fork zhao0349/Folia pushes and verified updates to
  https://github.com/water2004/Folia/pull/1. Keep it OPEN; DO NOT merge.
  Verification branch: `codex/mermaid-cpp-init-merge`; delivery branch:
  `feat/mermaid-cpp-port`. Upstream base remains `508423f2`.
- Baseline rerun 39897662560 passed all four standalone/integrated Windows
  Debug/Release jobs. Oracle commit `cee47ee5`, init mapping `e48c814d`.
- Original detectInit body supplies 1,746 mapping observations with injected
  detector results/errors and call-count checks. Registry dispatch is a later
  boundary. Single directives bypass sanitization; multiple directives do not.
- Original cleanAndMerge supplies 1,210 observations cross-checked against the
  official full bundle. Pin es-toolkit 1.45.1 (22 hashed dependency files), not
  the core package's newer transitive 1.52.0: array-null behavior differs.
- Native APIs own results and preserve inputs. Init mapping invokes the explicit
  detector only for truthy results and propagates errors. Clean merge preserves
  intermediate undefined values, merges arrays by index, converts array-like
  mappings and coerces array lengths. Final undefined/holes/custom array
  properties and init prototype-setter effects fail explicitly. No JS runtime.
- Verified code head `c447511a4f0f8e30eb726c51e60ec3290649fa81`:
  https://github.com/zhao0349/Folia/actions/runs/39899656149 passed all four
  standalone/integrated Windows Debug/Release jobs. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites; standalone jobs
  passed both Mermaid suites. All jobs checked source/SVG oracles, 78 native
  SVG DOM comparisons and 96 same-host browser/DirectWrite measurement cases.
  Local init-merge/config/directive oracles and git diff checks also pass.
- Small code commits: `cee47ee5` oracle, `e48c814d` init mapping, `77b693e0`
  clean merge, `391ee7d1` array length coercion, `c447511a` generated key/value
  budgets. This final documentation checkpoint changes no tested code or CI.
- Next concrete work: original config.ts session lifecycle oracle, then native
  session state. Diagram registry dispatch and renderer integration of derived
  themes follow as separate batches. Other families and application dependency
  replacement remain pending; no WinUI packaging or DPI acceptance is claimed.
- No live local builds/agents or ignored implementation candidates. Local
  MSVC/CMake remain unavailable; native evidence is the Windows CI above.
  Superseded runs 39899131976 and 39899536116 are not pending acceptance work.
  Delivery/checkpoint pushes can trigger another automatic run; inspect live
  checks on recovery rather than asserting that a later documentation run ended.

## Default/dark theme derivation verified (historical)

- User authorizes public fork zhao0349/Folia pushes and a verified update to
  https://github.com/water2004/Folia/pull/1. Keep it OPEN; DO NOT merge.
  Verification branch: `codex/mermaid-cpp-themes`. Delivery branch:
  `feat/mermaid-cpp-port`. Upstream base remains `508423f2`.
- Verified code head `e77b27b307707039e974b9bda6ffb7fa385e984c`:
  https://github.com/zhao0349/Folia/actions/runs/39896046297 passed all four
  standalone/integrated Windows Debug/Release jobs. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites; standalone jobs
  passed both Mermaid suites. All four checked the official source/SVG oracles,
  78 native SVG DOM comparisons and 96 same-host browser/DirectWrite cases.
  This final checkpoint-only commit changes no tested code or CI.
- Implemented `derive_theme_variables` for default/dark, preserving the exact
  constructor/update order, short-circuit and nullish branches, and final shallow
  object overrides. Owned ConfigValue output omits undefined generated fields.
  Theme ASTs generate checked-in ordinary C++ assignment functions; Khroma 2.1.0
  arithmetic and bounded linear literal scanning are native, without JS runtime.
- Official oracle: 1,218 theme observations and 2,412 color observations.
  Original theme source and official bundle outcomes are cross-checked; actual
  Khroma argument types identify native restrictions. Includes every default
  property override, random colors, nested objects, extremes and long literals.
  Inputs/outputs and intermediate assignments have explicit resource budgets.
- Native restrictions remain explicit: mapping overrides, literal string
  evaluated colors up to 4,096 UTF-16 units, integer THEME_COLOR_LIMIT 0..12,
  and no prototype/method replacement or mutable Khroma channel objects.
  Fixtures retain the original results. See README for the rendering boundary.
- Small implementation commits: `d884ca69` oracle, `a6f90193` color primitives,
  `6176c6fb` oracle inversion default, `e1d31a84` generated theme assignments,
  `98870049` owned derivation, `e77b27b3` linear scanning/source cross-checks.
- Next concrete work: official detectInit/cleanAndMerge and config.ts session
  lifecycle oracles, then renderer/common frontend theme integration. Existing
  Packet/Pie SVG accepts named/hex paints; derived HSL/RGB literals need an
  explicit integration boundary. Other families and application dependency
  replacement remain pending; no WinUI packaging or DPI acceptance is claimed.
- No live local builds/agents or uncommitted candidates. Local MSVC/CMake remain
  unavailable. Ignored focused runners/logs are under build/mermaid-cpp in
  theme-color-check, theme-derive-check and theme-derive-release. The Debug
  artifact needs unavailable debug CRT DLLs (0xC0000135); use Release locally.
  Checkpoint/delivery pushes can trigger new CI; the code evidence is the run
  above, not an assertion that every subsequent documentation run has finished.

## Theme implementation checkpoints (historical; superseded above)

- User authorizes public fork zhao0349/Folia pushes and updating existing
  water2004/Folia PR #1 only after verification. DO NOT merge.
- Working branch `codex/mermaid-cpp-themes`, based on delivery head `26d1b9f5`.
  Baseline Windows CI 39892083467 passed. Delivery branch remains
  `feat/mermaid-cpp-port` until this batch is verified.
- Selected boundary is independent default/dark theme-variable derivation.
  Official oracle has 1,218 theme observations and 2,412 Khroma observations;
  source-map-verified official theme bodies run before native implementation.
  Khroma 2.1.0 source/distribution hashes and ER constants are pinned.
- Preserve constructor/update/override/update/final shallow object-override
  order. Default constructor already calls updateColors; dark does not.
  ConfigValue output omits undefined fields, as JSON serialization does.
  Native input is an owned mapping with no prototype/method replacement or
  mutable Khroma channel objects; THEME_COLOR_LIMIT is bounded to integer 0..12.
- Native color primitives compile on Windows Debug/Release. First CI
  39894860027 exposed an oracle harness mistake: passing null as invert's
  optional weight requested zero inversion. Corrected to omitted/undefined;
  downloaded Release runner passes all 2,295 color observations / 4,594 focused
  assertions locally. No color implementation change was needed for that fix.
- Default/dark assignments are compiled from pinned source AST to ordinary
  C++ functions, checked by generate-themes.mjs. No JS execution in production.
  Native derive_theme_variables implements ordered evaluation, shallow final
  overrides, owned outputs, and bounded intermediate/final trees.
- CI 39895366688 compiles both themes in Debug/Release. Its original fixture
  did not classify all non-string calls into Khroma. The oracle now runs both
  original source and official bundle, compares their outcomes, and observes
  actual color-function arguments. With these corrected restrictions the same
  downloaded Release binary passes the full 1,218-case theme fixture locally
  (612,361 focused assertions). Debug artifact cannot load locally (0xC0000135,
  debug CRT imports); do not rerun it without the debug runtime.
- Further color tests expose extreme exponent underflow; native literal parsing
  now uses a linear scanner instead of recursive std::regex and distinguishes
  underflow from overflow. Adds long malformed/valid literals and Unicode space
  observations. This scanner change still needs fresh native CI.
- Next: run Windows standalone/integrated Debug/Release CI for the corrected
  fixture and scanner, then deliver only the verified head to PR #1.
  No application integration is claimed. No live agents/local builds.
  Ignored Release runners/logs: build/mermaid-cpp/theme-color-check and
  theme-derive-release. Local MSVC/CMake remain unavailable.

## Configuration primitives verified (historical)

- User explicitly authorizes this batch's public fork push and verified update
  of https://github.com/water2004/Folia/pull/1. Keep it OPEN; DO NOT merge.
  Verification branch `codex/mermaid-cpp-config`; delivery branch
  `feat/mermaid-cpp-port`. Upstream base remains `508423f2`.
- Verified code head `6e64cbf1abdf8064633647ba629f1bc6b59e21e5`:
  https://github.com/zhao0349/Folia/actions/runs/39890354498 passed all four
  standalone/integrated Windows Debug/Release jobs. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites; standalone jobs
  passed both Mermaid suites. All jobs checked official source/SVG oracles,
  78 native SVG DOM comparisons and 96 same-host browser/DirectWrite cases.
  This final checkpoint-only commit does not change the verified code or CI.
- Implemented two distinct configuration sanitizers, bounded assignWithDepth,
  diagram config aliases and theme/look/layout selection. Layer precedence is
  directive sum, reversed saved directives, initialize delta, pinned defaults;
  diagram-scoped appearance wins within each layer. These APIs own their output
  and preserve inputs; no shared configuration singleton is introduced.
- Official oracle: 2,940 observations, 720 keys, original schema/AJV generator
  (AJV 8.18.0), original functions in strict mode and source-map-verified themes.
  Local config/directive oracle and diff checks pass. Source/schema/theme files
  are pinned in source-lock.json; generated source tables are checked by CI.
- Small commits: `9ee6ca72` oracle, `fe532427` sanitizers, `d1cb9998` merge and
  appearance, `fecd0435` boundaries, `d4641c49` strict oracle/fixture corrections,
  `6e64cbf1` pre-copy payload validation. First CI exposed CRLF extraction,
  accidental JS input aliases and bare-word fixture decoding errors; final CI
  verifies the fixes. Never catch harness/assertion failures as expected results.
- Explicit dense-tree restrictions: no sparse-array creation, custom array
  properties or inherited prototype-setter effects. Inactive payloads on manual
  ConfigValue objects fail before copying. Original outcomes remain in fixtures.
- Next concrete work: implement detectInit mapping and cleanAndMerge with an
  official oracle, then config.ts session lifecycle and native default/dark
  theme-variable/color derivation. Current appearance API selects names only;
  it expects already-sanitized, merged layers. Keep these as separate batches.
  Full common frontend, other diagram families and application dependency switch
  remain pending. WinUI packaging and DPI acceptance are not established.
- No live local builds/agents or ignored implementation candidates. Local
  MSVC/CMake unavailable; native evidence is the Windows CI above. A checkpoint
  or delivery push may trigger another automatic run; inspect live checks when
  resuming instead of treating the old superseded CI runs as active work.

## Directive interpretation verified (historical, stage 4c)

- User explicitly approved pushing this batch to public fork zhao0349/Folia,
  running CI and updating existing upstream PR #1 after verification. Do not
  merge any PR. This resolves the earlier automatic-approval rejection.
- Working branch `codex/mermaid-cpp-directives` is pushed and preserved at
  `ef0966b2188e3c9cebcb1c9d987dcc8fbceb5ceb`. Local `feat/mermaid-cpp-port`
  was fast-forwarded after checking the remote PR branch remains an ancestor.
  Delivery target: https://github.com/water2004/Folia/pull/1, keep OPEN.
  Upstream base remains `508423f200f815beb7e686d64858501c58cef28b`.
- Implemented isolated `detectDirective` calls with the all/init/wrap filters.
  Original hash-locked `utils.ts` and `regexes.ts` generate 3,279 observations
  across 1,093 inputs. Commit `6e4c1996` adds the oracle; `cda332c1` adds native
  code, tests and CI. Detection/removal share one lexical capture scanner.
- Owned ConfigValue trees retain strict JSON values and UTF-16 code units.
  Syntax errors return absent type/null args and discard previous matches;
  cumulative source/depth/value/string/directive budgets fail explicitly.
  Calls intentionally avoid upstream global-regex lastIndex leakage following
  an exception. The oracle models independent calls, not shared regex state.
- Verified code/documentation head `ef0966b2188e3c9cebcb1c9d987dcc8fbceb5ceb`:
  https://github.com/zhao0349/Folia/actions/runs/39886836325 passed all four
  standalone/integrated Windows Debug/Release jobs. Integrated jobs passed
  FoliaTests, FoliaWindowsEditorTests and both Mermaid suites; standalone jobs
  passed both Mermaid suites. All four checked the source/SVG oracles, 78 native
  SVG DOM comparisons and 96 same-host browser/DirectWrite measurement cases.
  Local directive/source-text/preprocessing/frontmatter oracles and diff checks
  pass. The final checkpoint-only commit does not change this tested code.
- No live local builds/agents or ignored implementation candidates. Local
  MSVC/CMake remain unavailable; native verification above ran in Windows CI.
  Publishing the final documentation checkpoint may trigger an automatic CI
  rerun; consult the live PR checks instead of treating this as a code change.
- Next concrete implementation: pin `utils/sanitizeDirective.ts`,
  `defaultConfig.ts` and its schema/default-key generation, then establish an
  official sanitizer oracle before implementing init configuration handling.
  Follow with detectInit diagram mapping, assignWithDepth/cleanAndMerge and
  config.ts layer/appearance precedence; theme resolution follows that boundary.
  No detectInit, sanitizer, config application, shared frontend or application
  dependency switch is claimed. WinUI packaging/DPI acceptance remains pending.

## PR #1 review fixes verified (historical) - 2026-10-23

- User authorized fixing the review findings, positive tests, CI, upstream
  conflicts and documentation in small commits. Update existing upstream PR #1
  only after verification. DO NOT merge either the upstream PR or a fork PR.
- Review work used codex/mermaid-cpp-yaml-review, based on PR head 74ac2c8c. Commit
  10a43da7 merges upstream 508423f200f815beb7e686d64858501c58cef28b, preserving
  both Mermaid and CommonMark/reference exclusions in the core test glob.
- YAML candidate moves attached-flow-punctuation rejection to ScanTag and
  accepts whitespace-separated empty tagged nodes. Empty non-specific tags are
  null; explicit seq/map empty nodes remain typed. The original source oracle
  now has 515 observations (94 added), including positive space/tab/LF/CRLF,
  comma, nested/anchor/verbatim cases and paired malformed inputs.
- Local official oracle, clean pinned-source apply-check and patch reverse-check
  pass. First CI run 39882584178 exposed yaml-cpp's unrelated rejection of a flow
  comma at column zero after a newline; its context check is now corrected and
  six ordinary flow/block cases supplement the empty-tag regression cases.
  Actions was manually enabled by the user.
- Verified code/documentation head 89c417818f0a91703097a15a80cb3230cfd7080e:
  https://github.com/zhao0349/Folia/actions/runs/39883033095 passed all four
  Windows jobs (standalone/root, Debug/Release). Root jobs passed FoliaTests,
  FoliaWindowsEditorTests and both Mermaid suites. Both build modes also passed
  official source/SVG oracles, 78 native DOM comparisons and 96 browser/DirectWrite
  measurements. This checkpoint-only update does not change tested code or CI.
- Delivery: fast-forward the existing feat/mermaid-cpp-port branch for upstream
  https://github.com/water2004/Folia/pull/1 and keep it OPEN, with no force push or
  merge. Recheck its live head/checks before further work; the run above is the
  evidence for the verified code, and later documentation pushes can trigger CI.
- Next implementation stage, when requested: directive argument interpretation,
  configuration precedence and theme resolution against official oracles.
  No application dependency switch or WinUI/packaged/DPI acceptance is claimed.
- No live local builds/agents. Review evidence and old CI executable are under
  ignored build/mermaid-cpp/review-gemini-20261023. The ignored yaml-cpp reference
  checkout contains the candidate patch; use a fresh source tree for apply-checks.

## Upstream packaging checkpoint (historical) - 2026-10-21

- Upstream PR created: https://github.com/water2004/Folia/pull/1
  ("feat(mermaid): port official Mermaid C++ foundation, Packet, Pie, preprocessing, and YAML frontmatter (stages 1-4b)").
  Head: `zhao0349:feat/mermaid-cpp-port`, Base: `water2004/Folia:master`.
- Packaged all updates up to stage 4b in 6 clean, reviewable stage commits:
  1. `feat(mermaid): port official source text scanners and Packet diagram (stage 1)`
  2. `feat(mermaid): port Pie grammar, D3 geometry, and layout (stage 2)`
  3. `feat(mermaid): connect DirectWrite font measurement and themed Pie SVG (stage 3)`
  4. `feat(mermaid): port common text preprocessing and directive removal (stage 4a)`
  5. `feat(mermaid): parse native YAML configuration and frontmatter metadata (stage 4b)`
  6. `docs(mermaid): record merged stage 4b and next configuration boundaries`
- Excluded fork-specific workflow (`.github/workflows/mermaid-cpp.yml`), keeping
  the PR strictly scoped to native modules, platform DirectWrite measurement,
  fixtures, oracles, documentation, and root build integration (72 files total,
  +25,000 / -1 lines). Status is mergeable with zero conflicts on upstream `af8c5ede`.
- All agent continuity documents (`docs/mermaid-cpp/HANDOFF.md`, `README.md`,
  `source-lock.json`) are included.
- No live agent/local build handles. Ignored reference is `.reference/yaml-cpp`.
- Next concrete action: directive argument interpretation (`%%{init: ...}%%`),
  config precedence (diagram-scoped overrides within layers), and default/dark
  theme resolution with official runtime oracles before common frontend wiring
  to Packet/Pie.

## Stage 4b merged checkpoint (historical; superseded above) — 2026-10-21

- Stage 4b is merged: https://github.com/zhao0349/Folia/pull/5, merge commit
  357d5bb1a3426ccde32d5c798061f07b0f81188f on fork master. Eight commits
  preserved on the feature branch codex/mermaid-cpp-frontmatter.
- Native YAML configuration trees and frontmatter extraction:
  `parse_config_yaml` exposes owned null/bool/binary64/UTF-16/list/map values
  matching js-yaml 4.3.0 JSON_SCHEMA. `extract_front_matter` dedents captured
  fence indent and returns remaining diagram text with structured metadata.
- Pinned yaml-cpp 0.9.0 grammar backend at 56e3bb550c91fd7005566f19c079cb7a503223cf
  with checked-in patch for UTF-8 N/NBSP escapes, rejecting escaped apostrophes,
  handling empty sequence/map nodes, and rejecting empty explicit tags in flow
  collections.
- Verification: 421 total oracle observations pass in native C++ tests. 12 cases
  document explicit native safety rejection of surrogate and out-of-range Unicode
  escapes. Windows Debug and Release CI (runs 39561034820 and 39561028639) pass
  native suites, platform measurement, SVG oracles, and DOM comparisons.
- No live agent/local build handles. Ignored reference is .reference/yaml-cpp.
  No application switch or upstream PR.
- Next concrete action: directive argument interpretation (`%%{init: ...}%%`),
  config precedence (diagram-scoped overrides within layers), and default/dark
  theme resolution with official runtime oracles before common frontend wiring
  to Packet/Pie.

## Stage 4b checkpoint (historical; superseded above) — 2026-10-21

- Branch codex/mermaid-cpp-frontmatter, fork PR https://github.com/zhao0349/Folia/pull/5.
  User explicitly reconfirmed this public fork as the push/PR/merge destination.
- Pushed batches: 9718acb7 (315 source observations), dd3fbc31 (native YAML/config
  values and frontmatter), 685620a4 / 36dda685 / a19a4153 / 5db1f184 (empty tag
  refinement, escape patch, and flow collection boundary). Windows CI runs 39561034820
  and 39561028639 pass native tests (all 421 oracle observations), platform tests,
  SVG and measurement comparisons in Debug/Release.
- Final refinement adds 421 total observations, empty tagged-node handling, large
  scalar/resource checks, and a yaml-cpp patch (correct UTF-8 for N/NBSP escapes,
  reject escaped apostrophe, handle empty sequence/mapping nodes, and reject empty
  explicit tags in flow collections). Patch --check and oracle pass.
  12 cases document explicit rejection of surrogate/out-of-range Unicode escapes;
  actual official results are retained. README states budgets and this divergence.
- YAML grammar: pinned yaml-cpp 0.9.0, 56e3bb550c91fd7005566f19c079cb7a503223cf.
  Separate resolver follows js-yaml 4.3.0 JSON_SCHEMA; config trees own their data.
  No silent config dropping, renderer wiring, editor changes or app switch.
- PR #5 was merged into master at 357d5bb1 with eight commits preserved.
- No live agent/local build handles. Ignored reference is .reference/yaml-cpp;
  stage-4b verification lives in Actions. No upstream PR.

## Stage 4a checkpoint (historical; superseded above) — 2026-10-21

- Stage 4a is merged: https://github.com/zhao0349/Folia/pull/4, merge commit
  4ec7eea49d260656f737672d87df905be6da4da2 on fork master. Four commits
  preserved (prior stage closeout, oracle, implementation, boundary docs).
- Added native UTF-16 cleanupText and removeDirectives primitives in
  folia.mermaid.source_text. Original function bodies and directiveRegex supply
  1,221 runtime observations, checked independently and in order. Tests include
  malformed/unclosed constructs, Unicode, large adversarial inputs and ownership.
- Final head aaf0f33d00d226e0480f94966421db49d64ae1de passed Windows
  Debug/Release CI 39553912829: source oracles, native/platform suites,
  Packet/Pie and SVG comparisons, and same-host Edge/DirectWrite measurement.
  No local MSVC build was performed. Measurement limits remain as in stage 3.
- Review: self-review completed, no code issues found or unresolved threads.
  Copilot hit its quota and supplied no independent code review.
- Boundary decision: this is a text-primitive substage, NOT a full common
  frontend. Config extraction must precede directive removal. No family renderer
  discards config, no editor source is changed, and no application switch exists.
  YAML JSON_SCHEMA, directive args, config precedence, default/dark themes and
  remaining diagram families still need implementation and official oracles.
- Current branch: codex/mermaid-cpp-frontmatter, based on merged fork master;
  only this closeout checkpoint is added. Prior stage branches are preserved.
  No live build/agent/process handles or uncommitted implementation candidates.
  Stage-4a evidence is in Actions 39553912829; earlier local ignored evidence
  remains under build/mermaid-cpp. No upstream PR has been submitted.
- Next concrete action: establish official runtime observations for
  diagram-api/frontmatter.ts (dedent + js-yaml JSON_SCHEMA + supported metadata
  conversion) and select an explicit native YAML boundary before implementation.
  Do not replace YAML with a line parser or silently ignore unsupported config.
  Then implement directive argument interpretation and appearance precedence
  before wiring shared preprocessing to Packet/Pie. The inspected config.ts
  gives diagram-scoped values precedence within each layer; it is not a shallow
  last-object-wins merge. Continue small pushed commits and verified stage PRs.

## Stage 3 checkpoint (historical; superseded above)


- Stage 3 is merged: https://github.com/zhao0349/Folia/pull/3, merge commit
  8b8dc27aa27521530208a5efce7cdb40391bc04c on fork master. Eight small commits
  preserved. Reviewed head 721b20f3f459beb703abaa31b67abc9d417ef354 passed final
  Debug/Release CI 39548169982. No upstream PR or app dependency switch.
- Implemented: explicit-error DirectWrite measurement; native ink/advance
  union bounds; measured-Pie API sharing resolved font/theme with SVG; full
  family Pie SVG; shared checked XML encoding with Packet regression coverage.
- Verification: both native suites, 80 official SVG cases (78 finite DOMs and
  2 deliberate non-finite empty-left rejections), all seven Pie style rules,
  and 96 real same-host Edge/DirectWrite comparisons. Local Release focused
  Pie tests passed 330 assertions; local SVG gallery visually inspected.
- Measurement limits: Arial/Segoe UI/Times New Roman at 17/25 SVG units;
  max advance error 0.03076171875, bounds error 2.14892578125. Regression
  ceilings 0.0625/2.5 apply only to this corpus, not arbitrary fonts/text/sizes.
  Native outline bounds are not Chromium hinted bounds. Primary family must
  be installed; glyph fallback is OS-dependent. No estimates or text-specific
  compensation. Theme input currently requires named/hex literal paints.
- Review status: self-review and source/DOM/native checks completed. Copilot
  could not review because quota was exhausted; it supplied no code findings.
- Current branch: codex/mermaid-cpp-shared-config, based on the merged fork
  master. This branch contains only the stage-closeout checkpoint; next-stage
  implementation has NOT begun. Prior stage branches remain preserved.
- No live local process, agent or pending required build. Ignored evidence is
  under build/mermaid-cpp: browser-text.tsv, native-text.tsv,
  text-comparison.json, pie-svg/, pie-preview.png, downloaded tests-60/ and
  probe-89/ executables. Browser profiles are isolated there. No candidate
  implementation files are uncommitted. Local MSVC remains unavailable.
- Next concrete action: inspect the pinned official common preprocessing,
  config and default/dark theme resolution boundaries needed by Packet/Pie;
  establish source/runtime oracles before the next small implementation batch.
  Keep the measured-width error limits explicit. Full app/WinUI/DPI acceptance,
  shared frontend, other families and upstream submission remain pending.

## Established scope and historical stage results

- User scope: replace Folia's Mermaid dependency with a C++ port based on the
  original official Mermaid JS/TS. Do not port the Rust rewrite. Website and
  release tooling are excluded; this is unrelated to the MathJax migration.
- Upstream base: `water2004/Folia` master `72e2d7c7`. Fork: `zhao0349/Folia`.
  Stage-3 branch (now preserved): `codex/mermaid-cpp-measurement`, based on the merged fork
  master. Completed branches `codex/mermaid-cpp-official` and
  `codex/mermaid-cpp-pie` are preserved. Remotes: origin=fork, upstream=author.
- Source pin: Mermaid 12.2.0, commit
  `1fb7869866dd619168d840291c9c54b4f129758b`, ignored checkout at
  `.reference/mermaid-js`. `source-lock.json` records file hashes.
- Text scanners `086e9b44` with fixes `d67f05f3` and `b851abd8` pass Windows
  Debug/Release CI `39543613360` (862 source observations).
- Number formatting `b3adc989` passes Windows Debug/Release CI `39543875561`
  with 9,695 actual JavaScript binary64 observations. All are pushed.
- Packet parser/layout `60691de3`, SVG `157e380b`, and complete official CSS
  fixture `535e5ab5` are pushed. Their Node oracles pass: 254 parser/layout
  cases, 55 SVG cases. Native CI `39544210209` passed Debug and Release.
  Earlier run `39544084249` failed on the missing CSS fixture header, now fixed.
- Fork stage PR: https://github.com/zhao0349/Folia/pull/1, head `535e5ab5`.
  PR-event CI `39544508181` passed. PR merged with preserved small commits at
  `396d814f2d6e03b3767560cab97f8cec794b3a0c`. Upstream has no PR yet.
- Shared lexer extraction is the first Pie-stage batch: common metadata,
  hidden tokens and string conversion move out of Packet without changing its
  public API. CMake discovers the new module; existing Packet fixtures are the
  required regression check. Commit `8ece2acf` passed Debug/Release regression
  CI `39544334238`.
- Pie raw parser/DB `09ad39f6`, D3 geometry `733a3295` and parser tests
  `3675ec45` are pushed; native CI `39544894952` passed Debug/Release.
- Geometry has 86 official-runtime cases / 618 sectors, exact paths and
  1e-12 centroid tolerance for platform libm. Parser oracle has 291 cases.
- Pie layout `337e41d4` adds explicit measurement inputs and 40 official
  renderer observations. Push CI `39545148134` and PR CI `39545380450` both
  passed Debug/Release, including Packet regression tests. This tests
  arithmetic only; real text measurement, final themed SVG output and host
  integration remain pending.
- Second fork PR https://github.com/zhao0349/Folia/pull/2 merged at
  `df9753da2a46aae851724397fce7acdbb98e7548`, preserving five small commits.
- All three prior agents have delivered their assigned files and are idle.
  No background local build is running. All active verification is in Actions.
- Historical next action (superseded by the checkpoint above): implement the real text-measurement boundary and Pie
  SVG output on the measurement branch. First establish a browser/native
  measurement comparison using exact resolved fonts and UTF-16 display text.
  Do not claim a completed Pie renderer or application replacement yet.

## Measurement evidence recorded before this stage

- Pie's official renderer measures both every legend and the title using
  `getBoundingClientRect().width`; title width can make viewBox X negative.
- `src/platform/text/dwrite_measurer.ixx` currently returns end hit-test
  advance and estimates with character counts on failures. Do not reuse that
  fallback for Mermaid. Keep Windows/DirectWrite code in the platform layer
  and return an explicit error if measurement fails.
- `pie_legend_text()` supplies the exact strings to measure after first-label
  deduplication, including showData suffixes. `PieMeasurements` requires one
  width per populated section and a title width, in SVG user units.
- Local browser executables verified on 2026-10-21: Edge at
  `C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe` (157.0.4341.10),
  Chrome at `C:/Program Files/Google/Chrome/Application/chrome.exe`
  (155.0.8059.81). No browser measurement probe has been run yet. Use an
  isolated ignored test profile; do not touch the user's browser profile.
- Existing oracle packages include a browser-loadable Mermaid distribution
  at `tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs`.
- There are no uncommitted implementation candidates or live local build
  processes at this handoff. Prior agents have completed their assignments.

## User's incremental persistence instruction — 2026-10-21

Historical policy from 2026-10-21 (the current no-merge instruction above takes
precedence): persist every completed small part as a commit pushed to the user's fork.
For each completed stage, open a PR into `zhao0349/Folia` and merge when
verification and review support it. Preserve the small commits with a merge
commit. Only after the entire dependency replacement is complete should a PR
be submitted to `water2004/Folia`. The user explicitly authorized this workflow
to avoid losing work when usage quota interrupts a session.

## Acceptance and remaining scope

Upstream PR #1 is open for the foundation stages; the user authorized updating
that existing PR after review fixes and verification, and explicitly prohibited
merging it on 2026-10-23. The application still uses the Rust Mermaid
dependency; current C++ work is foundational and must not be described as a
completed replacement. The user has authorized implementation, GitHub Actions,
fork/branch pushes and updates to that PR; no repeated approval is needed.

Folia's current caller has no diagram-family whitelist. The existing dependency
inventory is 23 families (listed in README). Packet is the first native family,
not an agreed reduction of scope. Shared configuration/preprocessing/themes,
the other families, text measurement, app integration and full validation remain.

Keep the original family layout algorithms. Do not substitute a generic layout,
character-count text measurement, an embedded JS runtime, or a Rust fallback and
claim the requested C++ replacement is complete. Ordinary SVG normalization is
a separate Rust dependency and does not disappear just because Mermaid moves.

## Build environment

Use PowerShell 7. Local MSVC/CMake/Ninja are unavailable. The restored
`.github/workflows/mermaid-cpp.yml` defines standalone and integrated root builds
with VS, CMake 4.3.5, Ninja, the forked Boost.UT module and the console runtime.
Root jobs restore locked MathJax build inputs and verify existing source hashes;
they do not change MathJax implementation or application dependency selection. CI uploads
test executables. Set `FOLIA_MERMAID_FIXTURES` to the local absolute fixture
directory before running a downloaded executable.

Local Node and pnpm are in the Codex runtime dependency directory. Node 24 can
import the source-text oracle's TypeScript directly. Packet tools use the exact
packages in `tools/mermaid-cpp/oracle/pnpm-lock.yaml`. Only node_modules and
build outputs are ignored; fixtures and source provenance are committed.

UT expression formatting cannot stream UTF-16 characters. Wrap UTF-16 string
comparisons in `bool(...)`, or compare their hexadecimal snapshots. A source
oracle passing is distinct from native C++ tests passing.

## Verified boundaries

- Detection comments and preprocessing comments intentionally differ. Do not
  replace the current released TS scanners with legacy regex examples.
- Raw Packet grammar differs from full MermaidAPI preprocessing, entity
  encoding and DOM sanitization. Keep these validation stages distinct.
- Official Packet silently truncates fields beyond 10,000 rows and permits
  parsed Infinity; native rejects these cases with explicit errors.
- Packet default renderer always emits a title text node, even when empty.
  Negative coordinates in an empty diagram can be legitimate.
- Number formatting must use JavaScript shortest significant digits; exact
  fixed integer formatting differs for values above the safe integer range.
