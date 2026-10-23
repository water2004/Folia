# Mermaid C++ continuity

## Current checkpoint - 2026-10-23, registry and detector dispatch verified

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
