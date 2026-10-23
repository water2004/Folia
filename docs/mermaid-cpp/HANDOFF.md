# Mermaid C++ continuity

## Current checkpoint - 2026-10-23, PR #1 review fixes verified

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
