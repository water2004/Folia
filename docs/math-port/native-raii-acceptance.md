# Native RAII math acceptance

These gates replace JS-reflection/identity compatibility acceptance. The pinned
MathJax 4.1.3 commit, 38 packages, NewCM and extensions remain unchanged. Earlier
green corpora establish reference data, not acceptance for rewritten code.

## A. Architecture and build graph

- Every math implementation file has a keep/rewrite/remove/regenerate decision,
  a native responsibility, Source provenance and a named owner.
- Name files and modules by their responsibility, retaining the upstream owner
  where it still applies. Do not use native/new/typed suffixes or prefixes to
  keep competing implementations beside the canonical owner. Retire obsolete
  files after accounting for their remaining consumers; renaming is not removal.
- Start from the pinned upstream file and family responsibility. Each proposed
  new file/module must explain why an existing owner cannot contain the work,
  identify actual consumers, and identify the implementation it replaces.
  Native numerical family work replaces the canonical Common wrapper files;
  it must not coexist with a competing placement or Scene wrapper hierarchy.
- Audit both the active dependency graph and obsolete code remaining on disk.
  Record missing production callers explicitly. Passing isolated numerical
  tests cannot establish that CommonWrapper or the renderer is connected.
- The active `folia_math` graph contains no Context/ObjectRef/Descriptor,
  prototype services, universal `Value` call/construct machinery, arbitrary
  payloads, identity registries or manual request retirement scopes.
- Check transitive imports and linked source lists, not only spelling searches.
  Inspect implementation for renamed equivalent object-heap machinery.
- Both backends share one completed mathematical layout and placement. Source-required
  package subtree measurements may repeat during compilation using the same
  Common implementation. SVG depends on completed Scene,
  never the TeX parser or mutable MML/wrappers. Scene contains no raw SVG DOM,
  parser pointers or callbacks into a request. The future D2D boundary is the
  same Scene interface; implementing that backend is not this SVG gate.
- Production C++23 modules use `import std;`; math is platform independent.
  Windows console runners link `folia_console_test_runtime`, with stderr-only
  CRT/abort reporting. At most four compiler jobs and one build at a time.

## B. Ownership and mathematical structures

- Test MML create/link/unlink/replace/move/clone, token text, attribute
  own/default/inherited precedence, reparenting, cycle rejection and foreign
  document rejection. Incomplete/error math nodes retain their semantic role.
- Instrument actual request-owned allocator/node/wrapper lifetime in tests.
  Every return, exception, TeX error and cancellation returns transient live
  counts and outstanding bytes to baseline after retained results are accounted
  for. Inspect ownership as well as counters; do not redefine the counters to
  hide retained storage.
- Repeat mixed render and scene-only requests in measured phases (at least
  192 requests, then a longer run). With fixed semantic session state, there
  is no per-request growth in live nodes, wrappers, transient allocations,
  renderer caches or package scratch. RSS is corroborating evidence, since an
  allocator may retain freed capacity. Report time and cleanup cost too.
- Test persistent macros, labels, references and package state separately.
  Their intentional updates match Source. No persistent object borrows a
  request; frozen Scenes and SVG bytes survive later renders and destruction
  of their Renderer.
- Exercise errors/cancellation at real pipeline stages using bounded test
  observation seams, without adding production behavior switches. Verify
  cleanup and the Source-defined state of the next request.

The historical request-lifetime observation batch (`54be844a`, following architecture lock
`77445af9`) instruments actual MmlNode/MmlDocument/CommonWrapper and DOM base
construction/destruction with allocation-free typed counters. Parallel4
`request-lifetime-build.log` and `request-lifetime-diagnostic-build.log` pass.
The pending request UT runs 12 stateless formulas for 192 + 768 mixed Scene/SVG
requests, actual error/recovery paths and retained Scene use after InputOwner
destruction. Both focused executions exit normally with failure: 1 test /
10 assertions, 5 pass and 5 fail. This is diagnostic evidence, not acceptance;
the UT and its CMake registration remain uncommitted pending the ownership fix.

Live node/wrapper/DOM counts and retained Scene output pass, but the Windows
Debug CRT normal allocation baseline fails. `request-lifetime-objects-heap.log`
records a repeating per-input footprint, with the second cycle identical to
the first, at most +16 blocks / 480 bytes relative to the x baseline; after192
and after960 both end at +5 blocks / 144 bytes. No continuous growth is observed
in this run. Persistent ParseOptions nevertheless retains expression lists and
root/MathItem borrows after its document scope. These require an explicit
caller-owned request lifetime while preserving Source observations inside that
lifetime, rather than end-of-compile clearing or relaxed counters. A new full
math run and request ownership acceptance were pending at that diagnostic checkpoint.

The caller-owned request implementation follows lock `2f76e389`, retaining the
original ParseOptions/TeX owners and all accepted parser/package algorithms.
ParseRequest owns expression scratch and document borrows; TeXRequest binds the
main/text frames. Configuration and semantic session state remain persistent.
CompileScope restores the narrower MathItem/adaptor borrows on return/throw.
No new production file, manual retirement or end-of-compile clearing is added.

Stable parallel4 `parse-request-build-fixed2.log` exits0. Focused runs pass:
scope 1/11, TeX input jax 5/551, texhtml 9/182, textmacros 12/868,
hosted canonical75/150 1/1279 and default86 1/1021. The strict canonical replay
preserves 73 equal SVGs and two explicit Source event-API errors; its native
test exits1 as expected by that diagnostic wrapper.

`parse-request-ownership.log` exits0, 1 test / 11 assertions. Over 192 + 768
mixed requests every boundary returns actual MML/Common/DOM live populations
and Debug CRT normal/client blocks/bytes to the warmed baseline. The real
measurement exception, malformed TeX/merror, missing writer resource and
subsequent successful requests also return to baseline. Requests are unbound
after success/error; retained Scene serialization survives InputOwner teardown.
This supersedes the repeating +5-block/144-byte diagnostic footprint above.
The timing probe covers the complete helper execution and post-return Scene
cleanup; it does not isolate total MML destruction time. Persistent macro/label
semantics and application C ABI cancellation remain separate gates.

The first full run exposed a Physics fixture access before request binding and
a second creator probe retaining its parser/factory across document replacement.
The tests now install configuration creators before compilation and end local
borrows before replacing the frame; original assertions remain. Final parallel4
`parse-request-build-physics.log` exits0; Physics passes 8/5373. Final full
`parse-request-full-math.log` exits0: 847 tests / 215022 assertions / 170 suites,
no skips. Request ownership and caller migration are committed as `af9a8891`.
The lifetime test/helper and CMake registration are committed as `2a8b888a`.
This accepts the native request batch, not application integration.

## C. Source fidelity

Read component progress paragraphs below as historical evidence at their
recorded revisions. The latest hosted canonical75/150 result supersedes earlier
statements that that gate is pending; strict Lite still has the two explicit
Source event-API errors. The broad phrase "remaining specialized families" is
not an enumerated missing-case list. Reconcile existing registered Source
witnesses against the pinned profile before assigning new family work; stale
scope status fields alone do not establish an implementation gap. Do not edit
hash-locked fixtures merely to refresh progress comments.

The 2026-10-21 registration-only inventory found a named Source witness for
each of the pinned 38 TeX packages. Those witnesses differ in strength (MML,
package behavior, numerical probes or completed SVG); registration alone is
not execution evidence. Existing dedicated completed-SVG/retained-Scene
profiles cover tables, mglyph, maction, merror, XML, semantics and mfenced.
Other families also occur in the hosted canonical profile. One concrete
additional fidelity task is direct MathML munder/mover/munderover coverage:
`native_common_limits.cpp` and `common-limits-source-witness.cjs` currently
provide numerical/layout probes, while completed SVG is covered by canonical
TeX examples. Extend those existing family owners with an independently
captured direct-MathML Scene/SVG/after-request comparison; do not rewrite the
accepted algorithms or count missing dedicated tests as an implementation bug.

- Mathtools must preserve all six upstream file responsibilities and every
  implemented mapping. Compare matrices/cases/multlined/spreadlines, alignment
  commands, paired delimiters and expansion limits, relations/options, struts,
  laps/brackets/fractions and prescripts with actual Source MML. Check null/void
  dispatch results, pre/postscript holes and postfilter priority -6. Exercise
  named forms, renewal of a selected name, empty selection, errors/recovery,
  none/all/ams/cases/custom parents and both orders of Tagformat composition.
  Repeated input-jax construction must release forms and local creators without
  growing a shared registry. Closed typed option shapes replace arbitrary JS
  object/array mutation; report that boundary separately from rendered fidelity.
  Preserve GetArgCount's absent/undefined default separately from explicit empty
  brackets, the truthy string "0", and an explicitly supplied null argument.
  Retain all 42 historical formulas and verify actual extpfeil/Mathtools override
  order; obsolete reflection-dump comparisons are not mathematical acceptance.

- Colortbl must compare actual Source trees for cell/row/column precedence,
  clearing, sparse columns, preambles, overlap arguments and each array wrapper.
  Preserve unset versus defined-empty column values, color-argument errors before
  placement errors, and row-color writes before RowColorNotFirst. Exercise virtual
  createMml through ArrayItem, not only direct derived calls. Check that the real
  registered color config callback creates a missing model without enabling its
  commands, preserves an existing model, and keeps independent sessions isolated.

- Cases must compare actual Source MML, text-column scanning, ordinary-array
  fallback, tags/labels/references, tag modes/sides and configured formatters.
  The first matching end must pop one activation through an empty replay;
  Entry's explicit null must continue character dispatch to the base map.
  Verify explicit zero and UTF-16 subnumber wrapping, and preserve each error's
  consumed equation numbering in the next request rather than resetting it.

- Empheq must compare actual Source trees for all allowed environments and
  starred forms, side material and labels, all fixed/delimiter commands, and
  direct table transformations. Check creator dispatch, copied-node isolation,
  parent links and complete inferred-child transfer. Preserve false/empty side
  disabling and distinguish Source type failures from successful fixtures;
  neither arbitrary object mutation nor a successful SVG pipeline is implied.

- Setoptions must verify every Source option name against the finite native
  schema, distinguish absent fields from false/empty values, preserve the exact
  default permissions and error order, and exercise actual following formulas.
  Cover regex conversion, captured construction-time patterns, numeric limits,
  skipped/custom filters, partial writes, groups, persistence, macro priority,
  immutable aliases and independent input-jax state. A field-access check for a
  package not yet migrated does not establish that package's rendering fidelity.
  Report native type-domain rejections separately from Source-compatible cases;
  accepting arbitrary JS object/array/function mutation is not this native API.

- Tagformat must compare real TeX/MML output for none/all/ams, explicit and
  automatic tags, references, all five format callbacks, ref fallback, string
  and vector tag results, Unicode IDs/URLs, option updates and error recovery.
  Test custom parent behavior and formatting precedence independently. Repeated
  construction/destruction must release selected Tags and captured factories;
  unrelated instances and shared package registries must not accumulate entries.
  Source's generated class names are internal registration identities, replaced
  by a configuration-local creator; they are not the rendered-output oracle.

- Freshly build and observe the current native revision against independently
  regenerated, hash-locked Source results. Preserve corpus/profile/source pins.
- Compare all 75 common-layout cases, 150 complete SVG records byte-for-byte,
  paired Scene geometry and the four meaningful interaction-position cases.
  Preserve font tables, instance extensions, 40-module/80-request/24,646-glyph
  provider coverage and existing package-specific/malformed-input corpora.
- Add focused native tests at each changed mathematical algorithm boundary:
  TeX expansion/scopes, operator classes/spacing, inherited attributes,
  fractions, scripts/limits, radicals, stretch delimiters, tables, line breaks,
  colors/enclosures/actions, HTML/annotations, tags and errors.
- Keep Source-paired case data separate from a compact table-driven test runner.
  Commit the reproducible hash-locked Source witness, not thousands of repeated
  generated C++ assertion blocks. Preserve case/field diagnostics and exact
  comparisons; splitting a generated test into many source files is not a fix.
- C++ registration and option types need their own typed contracts. JS
  property descriptors, prototype replacement, Function.call mutation, sparse
  JS array holes and arbitrary JS coercion are withdrawn API requirements.
  Retire only those tests, with an explicit reason; retain their mathematical
  behavior coverage through typed tests and Source output comparisons.
- Missing package/node/primitive support is a failing/incomplete gate, never
  silent omission, reduced scope, hardcoded formula output or legacy fallback.

### Bussproofs component boundary (2026-10-20)

The proof-frame component compares pinned Source trees, proof markers, replay
order and TeX errors for empty/direct/multiple contents, ignored toMml flags,
ordinary/grouped prefixes, matching/mismatched end, stop and unfinished groups.
Verify the actual same frame transfers into the inner Stack, remains alive
after the outer Drop, and is destroyed after close or owning proof destruction;
test completion through the real outer begin/proof/end/stop replay. Repeat mixed
successful and failing frame scopes and confirm all tracked frames are released.
No full Bussproofs or SVG claim follows from these component tests: real Common
subtree widths, sequent/rule adjustments, semantic attributes and package filter
ordering still require end-to-end Source comparison after integration.

Whole-package acceptance runs the actual TeX configuration with the original
33 macro bindings, prooftree environment, configured node/item creators and
ordered filters. Compare complete MML trees and attributes against independently
captured Source results using real NewCM Common getBBox measurements; a fake
width service cannot establish rule/sequent spacing. Preserve all 31 historical
formulas and add label, direction, line-style, arity, empty-content, nested-proof,
sequent and captured-alias cases. Exercise definitions across expressions/reset,
missing output and measurement exceptions followed by successful requests.
Record Source runtime shape errors separately from typed native logic errors.
Verify all ten finite semantic fields preserve insertion, overwrite, deletion,
reinsertion, zero/false values, copy/merge and the original three-key transfer
order; unprefixed margins are excluded from semantics. Retire obsolete Context
reflection observers only after their mathematical cases have this replacement.
Passing this package boundary still does not establish SVG/Folia integration.

For Common subtree measurement, compare real Source getBBox outputs for
fixed-space, padded/styled, nested fraction/radical/table and token layouts
using the same profile fonts. Verify root/physical parents/attributes/layout
flags are unchanged apart from Source's intended setTeXclass state. A later
MML change must affect the next measurement, with no cached wrapper reuse.
Track actual custom wrappers/visitors through repeated successes and throws;
each call destroys its transient arena, and no BBox result borrows it. Exercise
supplied class registrations, metrics and MeasurementTarget callbacks as well
as a foreign-document rejection. Compile tests must observe the same output
borrow in both filter phases, null on a later unbound compile, correct nested
borrows, and recovery after pre/postfilter exceptions without retained service
or document ownership.
The per-call arena claim concerns Common wrappers/visitors. Source-generated
auxiliary MML stays document-owned: report its growth under repeated measures
on one document explicitly. At least 192 independent document requests with
radicals/bevelled fractions must release every tracked auxiliary MML node on
request destruction; a persistent session may not retain those documents.

### Physics package boundary (2026-10-20)

Preserve every historical Physics formula and compare full real TeX/MML trees
and attributes with pinned Source, including all eleven maps and callback/arg
bindings. Cover quantity stars/sizes/nesting and over items, eval/smash, vectors
and accents/fonts, operator expressions, quad text, differential and derivative
branches, bra/ket captured aliases, all matrix constructors and malformed input.
Verify both physics options independently and together, environment/creator
restoration after nested vectors and errors, next-request recovery, and native
AutoOpen lifetime/replay behavior. Explicitly check fence properties are removed
on inferred output and preserved by inferred=false. Record any Source runtime
errors separately from typed native errors; do not recreate JS object mutation.
Source fixtures must be independently generated, hash locked and byte checked;
native output never regenerates their expected trees. Follow focused tests with
one complete math run after the final substantive changes.

### Unicode and Entities boundary (2026-10-20)

Compare all three Unicode commands with actual Source, preserving historical
cases and adding font/HD parsing, variant weight/style, radix and boundary
values, NUL/surrogates/supplementary text, Char scan widths and following input,
captured aliases, errors and following-request recovery. Verify shared fonts
across two TeX instances and reset in one registry, plus destruction/isolation
of independent native registries against independent Source module loads.
Record native range_error separately from Source RangeError. Check the unicode
marker and configured token creator's exact request, and native cache release.

For Entities, compare base and every one of the 29 shard tables with pinned
Source, and exercise actual on-demand completed translations using Source's
real asyncLoad/retry protocol. Native resources are local and synchronous;
do not claim JS retry object/API equivalence. Test unknown names, one-load
behavior, disabled loading, explicit add/remove and reinstallation, case and
syntax boundaries, numeric conversion/errors, independent modules and exact
UTF-16 output. Fixtures must preserve embedded NUL and lone surrogates; emit
length-aware string literals and valid C++ escapes. Keep generated data ignored.

Accepted 2026-10-21: `unicode-entities-full.log` exit 0, 706 tests / 206757
assertions / 136 suites, no failures or skips; final build exit 0 in
`unicode-entities-build.log`. Entities focus: 9 tests / 4768 assertions.
Unicode focus (including existing matching tests): 12 tests / 414 assertions.
The independent `entities-source.cjs` bytecheck covers 406 base entries, 29
shards and 2128 raw records, 21 numeric cases, 19 completed translations, three
overlap reinstall cases, three full-input retry cases and two numeric/load
ordering cases. `unicode-source.cjs` covers all 11 historical cases within 51
inputs, six cache/reset/module-isolation observations, six same-jax recovery
steps and an actual configured token-creator request. Root reran both witnesses
and the full38 pinned profile; compiled Source hash remains unchanged. Logs are
under ignored `build/math-port/`. This accepts these owners, not SVG integration.

### Mhchem boundary (accepted 2026-10-21)

The original configuration pair owns Machine, relmo, nine commands, fourteen
characters and six ordered replacements. Its retained original maps preserve
ordinary typed mutations independently of later registry replacement. Existing
pure C++ mhchemparser algorithms remain their original dependency owners.
The MathChoice owner now replaces through its physical parent as Source does.

Independent self-contained Source witnesses bytecheck all 7960 dependency rows
(1679 parses, 6279 patterns, all fourteen FSMs and 62 regexes), 220 package inputs
including all 113 original tests, 63 inputs at each of six replacement stages,
bindings, creator requests, aliases, errors and recovery. Fourteen Source map
mutation observations include eight native typed scenarios and six observations
of withdrawn JS APIs; these six are not claimed as native supported APIs.
Fixture regeneration preserves unchanged bytes and modification time.

Root final bytechecks: `mhchem-parser-source-root-final.log` and
`mhchem-source-root-final.log`. Build: `mhchem-physical-parent-build.log`.
Full current target: `mhchem-full.log`, exit 0, 713 tests / 208874 assertions /
138 suites, no failures or skips. Dependency UT aggregates all rows into nine
assertions; row counts are not assertion counts. Logs are in `build/math-port/`.
This accepts these package/dependency owners, not SVG or Folia integration.

### Texhtml, Lite and delimiter finding (accepted component boundary)

The native original owners now pass their pinned Source comparisons. Lite has
41 witness records with 30 parser inputs; its 11 native tests execute 95
assertions across parsing/serialization, queries, mutation traces, metrics,
entities and RAII lifetimes. XmlDomAdaptor has five tests / 48 assertions for
immutable subtree isolation, HtmlNode behavior and final ownership release.
FindTeX compares 22 inputs / 59 ProtoItems in three tests / 1330 assertions.
Texhtml has nine tests / 182 assertions covering 22 direct cases, recovery and
all five original HTML-to-compile pipelines. Source direct input options are
captured from the initial input table, separately from later observed options.

Root bytechecks from C:/Windows: `lite-source-current-root.log`,
`find-tex-source-current-root.log`, `texhtml-source-current-root.log`, all exit 0.
The five original Source MML snapshots remain exact; texhtml Source JSON SHA is
`c13c8eb7ca5c9dea20e5a264a4b1437194949c01bd4f8c719dbf5a2959dfc4d5`.
Build `texhtml-final-build.log` and four `*-accepted-focus.log` pass. Final full
math `texhtml-full.log` exits 0: 741 tests / 210529 assertions / 149 suites,
no skips. Logs are in `build/math-port/`. This accepts these components;
Common-to-Scene emission, SVG equality and Folia integration remain open.

The following requirements define the accepted coverage and remain applicable:

Preserve all seven original TexHtml tests with their actual invocation paths:
the five render2mml cases first traverse HTMLDomStrings and serialize the
numbered nested markers, while the direct error/disabled cases use TeX input.
Additional raw TexParser examples do not substitute for those render cases.
Capture actual full physical MML trees, foreign HTML trees with ordered
attributes, raw and convenience comment values, serialization and textContent,
creator requests, map binding, default/changed allowTexHTML, numbered nested
markers, trim/empty/multiple roots, remaining input and errors/recovery.
Verify actual package pre/postfilter timing and clearing the adaptor borrow.

Lite witnesses must compare the original parser/repair/serializer, ordered
queries and tree mutations, attributes/classes/lazy Styles, entity loader
completed retries and numeric errors, UTF-16 split edge cases, clone identity
and parent behavior. Preserve known Source quirks, including clone text/comment
parent handling and parentless split's mutation before failure. Record invalid
JS-domain API errors separately from the native typed API; do not recreate
arbitrary property values or descriptor/class mutations. This is equivalence
with Source Lite, not browser HTML conformance.

Native lifecycle checks retain weak observers through nested parsing, errors,
detached/list-held nodes and independent adaptors, then verify final release.
Freeze must isolate XmlRef from subsequent mutations of the original Lite tree.
No mutable DOM, MML or adaptor owner may escape inside the immutable Scene.
Witness tools must validate fixed input hashes, reproduce from C:/Windows,
bytecheck by default and preserve unchanged fixture mtime on explicit writes.
Run focused package/dependency tests then one full math run after final changes.
Scene/SVG emission and Folia remain separate outstanding acceptance work.

## Common-to-Scene component evidence (2026-10-21)

The enclosure extension at `adef6867` passed 86 selected TeX/Common/Scene SVG
byte comparisons (1107 assertions), including every actual notation and four
boundary inputs. Twelve retired-request cases passed 24 assertions. All 56
positive/negative-height enclosure geometry, transform and paint outputs match
the independent pinned Source witness; raw bbox/extender checks also pass.
The full math run passed 776 tests / 212075 assertions / 150 suites without
skips in `build/math-port/enclosure-all-full-math.log`. These results supersede
the earlier foundation-only 11/20 comparison. Source148 retains the previous
115 records unchanged, and the separate direct-MathML table corpus has eight
accepted cases. The canonical 75 layout / two-mode 150 SVG gate, remaining
specialized families, full request ownership and Folia integration remain open.
The current checkpoint and next action belong only in HANDOFF.md.

The semantics/mfenced extension at `ef9ce49c` adds 11 independent MathML SVG
comparisons, including serialization after document/job destruction. Direct
emitter tests verify empty/null semantics handling and exact fenced wrapper
parent restoration on success and exception. The shared test builder replaces
the duplicated table tree reconstruction; the existing eight-case table fixture
remains byte-identical. Source generation and no-op writes reproduce both
corpora. The subsequent full math run passes 782 tests / 212129 assertions /
152 suites without skips (`semantics-fenced-full-math.log`). This accepts the
two traversal owners; foreign XML annotation payloads remain outside this batch.

The maction batch at `e389abf5` / `e07b3b16` / `0fe4fb0c` / `51ada53b` /
`970cff98` retains Common selection/layout, owns nested completed Scenes and
typed interaction descriptions, and replaces the old maction Surface/placement
bridge in its original SVG owner. Independent pinned Source uses real EventTarget
registration on Lite nodes without dispatch. The initial batch compared seven
fontCache=none cases and kept three local-cache tips explicitly pending. The
subsequent cache batch described below supersedes that temporary boundary.
Typed Source class styles, actual wrapper/root metrics, listener shape, parent
restoration on success/exception and Scene alias independence are checked.
MathML input compilation can override the requested display flag; the fixture
records the actual MathItem.display while preserving requested inputs and SVG.

The full math run passes 818 tests / 212525 assertions / 162 suites without skips
(`maction-full-math.log`). Canonical75 retains 73 byte-equal normal Source SVGs;
both strict-Lite action cases now return native Scenes while Source fails event
registration. This is recorded as source_error_native_success and does not pass
the 75/150 gate. Complete request ownership and application integration remain
open; static SVG parity does not claim browser event dispatch support.

The recursive local-cache batch at `e72c7576` / `dd48c4d9` / `fa409846` /
`897a0f50` records actual Builder resource order independently of drawing order
and restores the original FontCache responsibility as a native temporary plan.
It retains no JS objects or callbacks and allocates no global cache IDs in the
writer. All 14 independent Source MathML cases match exact SVG before/after
request retirement, including nested and segmented tips, following outer glyph
definitions in nested defs, automatic/fixed/empty prefixes and empty defs.
The earlier ten SVG byte strings are unchanged against Git `175ecabf`; Source
prefixes are observed from actual FontCache.useLocalID calls. Scene order checks
pass 5/26, FontCachePlan 7/39, writer local 3/12 and Source14 1/128.

Full `cache-full-math.log` passes 831 tests / 212634 assertions / 164 suites,
without skips. `cache-results.json` still records 73 canonical byte-equal SVGs
and two strict-Lite source_error_native_success observations, not a passed75/150
gate. Mglyph images, full profile coverage, request ownership, integration and
the remaining old object layer are still unfinished.

The mglyph batch keeps CommonMglyph's existing dimensions, baseline and legacy
character wrapper in its request-owned factory. The original SVG mglyph owner
emits an owned Image Scene value; the writer only projects its dimensions,
transform, alt and href. The inactive MglyphPlacement pair and registration are
removed. Independent pinned MathML Source captures 13 cases: eight images,
three legacy ASCII/supplementary indices with actual explicit-font measurement
requests/results, and two invalid-input merror verification observations. The
last two are not image successes. The fixture SHA-256 is
`ef8fe550e9d54839050ffa896556f7d8adc555b02f184ed827a476d05d31e1ab`;
Source write/default/no-op and Root independent default checks pass.

Scene image checks pass 3/17, writer 3/5, owner/catalog 4/26, and Source13
1/222. All 13 complete SVG strings match before and after request retirement;
the image owner uses only the first standard content root. The final parallel4
build (`mglyph-build-final.log`) and full math run (`mglyph-full-math.log`)
pass: 842 tests / 212904 assertions / 168 suites, without skips. Canonical75
(`mglyph-results.json`) remains 73 byte-equal SVGs and two strict-Lite
source_error_native_success observations. Full profile coverage, 75/150 gate,
request ownership, integration and final old-layer retirement remain open.

The additional table-profile witnesses at `037e543f` / `5923227e` preserve the
existing table algorithms and earlier corpora. Seven actual MathML inputs run
in none/local modes: contrasting left/right labels and width ownership, nested
percentage width, ragged alignment with a cell override, horizontal stretching,
and text/operator column breaks. Source predicates confirm the branches are
exercised; the operator break retains the original per-character identifiers.
The Source fixture SHA-256 is
`b23150d14cb8cdeb08e429428c87b6820f16bffb14eb87613165dad7209c12f8`.
Source default/write/no-op and Root independent default pass.

Native Source14 passes 1 test / 669 assertions: final root and every table's
BBox, TableData, shape, flags and percentage width match; all complete SVGs are
byte-equal before/after request retirement. Table-related regression checks
pass 42/4701, default86 passes 1/1021, and the parallel4 build passes. Full
`table-profile-full-math.log` passes 843 tests / 213573 assertions / 169 suites
without skips. No production code changed. `table-profile-results.json` retains
the strict-Lite canonical73 equality and two explicit Source event-registration
errors; it does not establish the full75/150 gate or application integration.

The canonical two-mode batch shares the unchanged original18 + profile57 input
table (`8ce3d53b`) and the existing native TeX-to-Scene test pipeline
(`060bd58d`). Independent pinned Source runs none/local in separate 512 MB
Node processes, preserving the full38 package order and persistent TeX session.
Real passive EventTarget registration on actual Lite receivers supports the
toggle/tooltip captures without dispatch or changing their DOM serialization.
Actual root outer BBox, display, local cache prefixes and complete SVG are
captured; the fixture SHA-256 is
`5c970e41dc36f7717f1dbf31c576a6c875f445068284fc8c2cd7042e9a17985c`.
Source write/default/no-op and Root independent checks pass (`7f83584a`).

Two exposed assumptions are fixed at their original owners: absent mspace
leading retains the .2 default rather than scaling it again (`5743e3fc`), and
font instance cache IDs are captured without assigning constructor static IDs
after extension registration (`71e4abe5`). Empty SVG suffixes are valid; outline
resource provenance remains face/originating-variant and inherited resources
stay shared. The development generator streams its complete Source witness,
preserving integer-key order and eager lookup side effects. Generation succeeds
at 512 MB, retains the 40-module/80-event/24646-profile locks, and a second run
leaves all 88 output modification times unchanged.

The native hosted test (`af8c5ede`) passes 1/1279: 75 layout pairs, 150 completed
Scene metric comparisons and 150 complete byte-equal SVGs after request teardown.
Default86 passes 1/1021, focused leading 1/20 and fonts 1/73. Final parallel4
build `canonical-modes-build-fixed2.log` exits0; full
`canonical-modes-full-math.log` exits0 with 845 tests / 214872 assertions /
170 suites, no skips. Strict Lite `canonical-modes-results.json` separately
retains 73 equal SVGs and two Source event-API errors; its original fixture hash
is unchanged. Hosted parity does not establish browser event dispatch, the
192-request lifetime gate, complete profile coverage or Folia integration.

## D. Application integration

### Native application-facing library checkpoint (2026-10-21)

The direct MathML limits witness adds twelve inputs in none/local modes:
inline movable integral, display sum limits, explicit accents and stretched
accents for munder/mover/munderover. Source records actual compiled nodes,
wrapper BBoxes, prefixes and complete SVG; fixture SHA-256 is
`bb52168574af0b70a7f63a6dcff0c5bff05f9c5ca6922cdb10f76527abafc636`.
Root Source default/write-no-op passes, old numeric fixture stays byte-identical.
Source/fixture commit `d688445e`, existing limits UT `73ae4eef`; focused 1/97,
numeric 6/34 and canonical 1/1279 pass. Full `limits-scene-full-math.log` passes
848 tests / 215119 assertions / 171 suites, no skips. No math algorithm changed.

The original Renderer owner now composes typed TeXRequest/MathItem/Common layout
and immutable Scene, with no Context session or manual retirement. Its prior
dirty callback/projection cleanup served only the withdrawn object graph;
Root reviewed and retained that patch plus source snapshot under ignored
`build/math-port/renderer-owner-candidate/` before deliberate owner replacement.
The obsolete identity diagnostics and its native-session getter test are
withdrawn, replaced by actual typed measurement cancellation/lifetime evidence.

Stable parallel4 `renderer-owner-build-fixed.log` passes, including the real
FoliaNativeMath DLL. `renderer-owner-exports.log` confirms exactly the nine C
exports, without C++/STL exports. Public C++ renders all original 75 none-cache
cases through UTF-8 and UTF-16: 1/1053 with complete SVG and Scene metrics.
The fixture's actual options are directly representable (tag-side default,
em16/ex8, widths1280/100/80/120); neither Source data nor hidden options change.
Result/Unicode/merror recovery and ownership after Renderer teardown pass 1/7.

Real opaque C calls match all 75 Source SVG records (1/460); arguments/null
results pass 1/26, owning-thread and pre-cancel reuse 1/11, deadline and ensuing
SessionUnusable 1/8. C++ pre-cancel passes 1/10, in-flight deadline 1/2, actual
Lite measurement cancellation/recovery 1/2, nested thread-local scope 1/4,
wrong-thread rejection 1/2 and normal result contract 1/4. OOM status mapping is
reviewed, not induced by exhausting memory. Final `renderer-owner-full-math.log`
passes 860 tests / 216708 assertions / 174 suites without skips. This proves
the native application-facing boundary. The worker integration evidence follows;
obsolete on-disk graph retirement remains pending.

### Default WinUI SVG worker checkpoint (2026-10-21)

Commit `ba9ee591` replaces the original MathJaxRenderer worker backend with
the opaque native C ABI; queue/cache/generation/priority/dependency behavior
and SVG parser are retained. QuickJS runtime calls, link/build target and JS
math/font deployment items are removed. Mermaid and its Rust normalizer are
unchanged. setup.ps1 exits0 (`worker-setup.log`), supported Debug build exits0
(`worker-app-build.log`, session24161, parallel4). The built and deployed
FoliaNativeMath.dll share SHA-256
`0FB98CA1ADD2BB9D1BEA9315A03F31917C6FA40CDA73F173D0B990B5762CFFFC`.

The actual MSBuild MathJaxRenderer, MathJaxSvgParser, queue, std and pch objects
link with the native import library and console failure runtime in a bounded
worker probe. `worker-probe.log` exits0: mathbb, display fraction, raw UTF-8,
noundefined output, malformed fraction mapped to Formula with fragments,
subsequent recovery, identical cached shared result, completion dependency,
Clear generation drop and disable/join/re-enable all pass. The stop join takes
4454ms in this Debug run, within the ten-second test bound. Unknown commands
are accepted by the configured noundefined package; the probe's initial wrong
Formula expectation was corrected to use genuinely unclosed fraction input.
No production parsing/error classification change was made for that test fix.

`cmd /c build_platform_test.bat` exits0 (`worker-platform-tests.log`, session78599):
80 tests / 3429 assertions / 17 suites, including the existing deterministic
priority queue tests. `cmd /c build_test.bat` exits0 (`worker-core-tests.log`,
session95496): 455 tests / 40840 assertions / 50 suites. Neither run skips tests.
This establishes the default worker-to-native-to-existing-SVG-parser path;
it does not claim a launched WinUI visual inspection, a Release build or browser
event dispatch support.

Reproduce with `powershell -NoProfile -ExecutionPolicy Bypass -File
tools/math-port/winui-math-worker-probe.ps1` after setup. The runner uses
build_app.ps1 and actual app objects, never a duplicate worker implementation.
Root ran this tracked runner under Windows PowerShell: session57717 exits0,
`worker-reproducible-probe.log`; probe PASS, stop join4575ms, total8.91s.
The runner explicitly retains its process handle before waiting so PowerShell
5.1 reports the actual child exit code. The initial runner's null ExitCode was
a runner bookkeeping issue; the underlying worker probe also printed PASS.

- Validate opaque C ABI UTF-16 input, result ownership, thread contract,
  cancellation, deadlines and errors against the accepted native library.
- Exercise the default real Folia worker through queue -> C ABI -> native math
  -> SVG parser. Include mathbb, fractions, malformed TeX, repeated cache keys,
  generation changes and shutdown. Preserve worker scheduling/cache behavior.
- Use the existing SVG path. No extra D2D pixel gate is required. No QuickJS
  math fallback or native runtime-object shim is allowed after switching.
- Restore with `setup.ps1`, build the supported WinUI configuration, and run
  relevant deterministic platform tests. Keep Mermaid's dependencies separate.

### Retired object-protocol tests (2026-10-21)

The inactive `context_string_trim.cpp`, `context_string_match.cpp` and
`context_string_split.cpp` tests are removed after individual review. Their
24 cases exercise Context String/RegExp receiver coercion, live properties,
Symbol dispatch, species construction and JS result-array descriptors. None
imports a mathematical owner or tests TeX/MML/Common/Scene results or typed
request lifetime. They were absent from the explicit FoliaMathTests source
list; no current generator or fixture consumes them. Historical ignored draft
patches are records, not build inputs. This removes no active test coverage and
does not establish retirement of other mixed mathematical/object-protocol tests.
The protected dirty files remain untouched.

The mathematical assertion formerly in inactive `tex_combine_relations.cpp`
is preserved in the existing active `tex_jax.cpp` owner. It compiles `a><b`
through the actual base configuration and caller-owned TeXRequest, then checks
one inferred row with exactly `mi(a), mo(><), mi(b)` in order. No node is
manufactured or merged by the test. `retirement-combine-build.log` exits0
(parallel4); focused case exits0 with 1 test / 9 assertions, existing
`*TeX input jax*` exits0 with 5 tests / 551 assertions. Unrelated cases are
filtered/skipped by these focused runs. The obsolete Context/document harness
file is removed after this replacement passes; no production algorithm changed.

### SVG object-layer retirement coverage (2026-10-21)

Before retiring the old SVG graph, preserve the mathematical assertions at
their existing typed test owners. `parse_util.cpp` adds four real fixInitialMO
cases (null/space skipping, first eligible node, embellished operator and
REL TeXAtom), checked against pinned ParseUtil.ts. `native_common_maction.cpp`
adds a real two-line selected row and verifies delegated break count/line BBoxes.
`native_common_wrapper.cpp` preserves numeric/keyword/px/scale length conversion.
`tex_jax.cpp` additionally checks all eight default filter priorities.

Parallel4 `retire-svg-coverage-build-fixed2.log` exits0. The focused runner
`retire-svg-coverage-results.json` records fixInitialMO 4/26, Maction lines
1/15, wrapper length 1/12 and TeX Jax 5/559, all exit0. Initial compile failure
was two Boost.UT literal additions in test code, corrected to native unsigned
arithmetic. The initial Maction test used a dimensioned mspace that Source
explicitly disallows as a newline; the corrected fixture uses a real newline
without explicit height/depth. No production algorithm changed.

Six inactive files now retire eleven reflected-metadata/record cases:
package_filter_source_metadata, tex_default_filter_source_metadata,
tex_stack_item_symbol_keys, textmacros_source_fields,
jax_filter_record_owner_scope_g262 and math_document_request_record_lifetime.
Their JS callable name/arity, dynamic descriptors, Symbol operations, projected
record identities and registry retirement protocols are withdrawn contracts.
The retained default priorities are checked directly above; real request
allocation/Scene lifetime remains in request_lifetime.cpp, and actual package
behavior stays in the active package suites. No Source fixtures/oracles or
protected dirty files are removed by this batch.

The real public behavior from bussproofs_renderer_session is preserved at the
existing public Renderer test owner, mathjax_full_profile_package_bootstrap.cpp:
separate Renderers produce the same complete proof SVG and Scene geometry;
proof/formula/proof calls retain distinct valid outputs; renderScene/render
agree on typed glyph/rule/path geometry and BBoxes. Public build session27959
exits0 (`retire-svg-public-build.log`); the subsequent actual focused run passes
1 test / 23 assertions. An earlier zero-match invocation against the old binary
before linking finished is invalid evidence and was replaced by that run.

The inactive bussproofs_renderer_session, common_factory_context_methods,
tex_format_error_context_identity and tex_stack_item_raw_source_fields files
now retire after per-test review. Retained proof sequences/Source trees/errors
stay in bussproofs.cpp and bussproofs_items.cpp; configured TexError formatting
stays in tex_jax.cpp and native_tex_error_source/utf8. The old fixInitialMO
mathematical assertion is covered by the four typed tests above. Dynamic
Source factory replacement, callbacks, property arrays and weak projection
leases are withdrawn, with no replacement object-semantic mechanism introduced.

Five inactive Scene projection test files also retire: maction_passive_host_g213,
maction_tooltip_scene_projection_g219, scene_attribute_parity_g145,
scene_parent_projection and scene_request_owner_retirement. Typed action kinds,
nested tooltip snapshots, exact Source SVG, restoration after errors and request
ownership remain in native_svg_maction_source, native_svg_maction,
native_scene_actions, native_svg_actions and request_lifetime. Multi-line Maction
delegation is checked by the new real selected-row test above. Old mutable
Scene-to-Context arrays/parent links and dynamically replaced SVG helpers are
withdrawn. In particular cloneSceneNode in the old scene/glyph_paint.cpp is
inactive and uses the discarded mutable NodeRef model; its text/comment parent
rule is not a missing immutable Scene requirement. The active Common
finishInlineFrames performs inline split/placement, with complete canonical
Source SVG coverage including inline-linebreak-narrow.

Legacy tool review additionally preserves the 75-case Scene-only/SVG numerical
geometry comparison in the existing public profile test: two independent but
identically ordered Renderer sessions feed renderScene and render. All 75
Scene metrics match the pinned Source, and glyph origins/advances, transforms,
rule bounds and path segments agree between routes. The original 75 UTF-8
and 75 UTF-16 complete Source SVG comparisons remain. Focused profile passes
1/1354 (`retire-svg-profile-geometry.log`); the explicit XML metrics-family case
passes 1/6 (`retire-svg-xml-family.log`). Nested HTMLDomStrings placeholders and
callback ownership were already asserted by active texhtml.cpp; empty-family
Times fallback was already compared to complete Source SVG. JS false family
values are outside the typed string field's domain, rather than a missing case.
`retire-svg-tools-coverage-build-fixed.log` exits0; no production code changed.

The first full run was stopped by its runner's 180-second limit while normal
Source cases were progressing, not accepted as a passing run. Its output and
terminal ETIMEDOUT/SIGTERM result are retained as retire-svg-full-timeout180.*.
The existing accepted full-run harness uses 600 seconds. The subsequent full
run with that bound and cleared diagnostic environment exits0 (session43545):
869 tests / 217108 assertions / 175 suites, zero skips. The result and complete
log are retire-svg-full-results.json and retire-svg-full-math.log.

Legacy native-only font extension observers/comparers retire with their Context
Jax/Map dependency. The dynamic-map observer checks no glyph mathematics. The
instance observer's supported font data is retained in the canonical typed
NewCM repository, active native_raii_fonts/BBM/BBoldx/Dsfont tests and the already
accepted complete Source font-data comparison. Independent font-data, dynamic
Map and instance-extension Source oracles/records remain. No font tables,
provider inputs, generated glyphs, licenses or Source locks are removed here;
retirement does not claim a newly rerun full font-data comparison.

The old common-layout native dump/observer and its native-result runner/comparer
retire as well. They required mutable wrapper/Property/Context identities and
Source callback/paint traces. The independent common-layout-original-oracle,
profile inputs/locks and Source records are retained. This retirement does not
claim that all old reflective per-wrapper record fields match the new C++ model.
The retained mathematical gates are the typed family BBox/placement Source
witnesses, 75 canonical input pairs, 150 full SVGs, Scene geometry across both
public routes, and request ownership. Experimental tracked C++ copies that
still imported the same old graph have also been reviewed and retired in
separate batches; directory names were never used as evidence of inactivity.

The nine old SVG module pairs now retire: native_document, render_session,
scene_context_nodes, output_jax, output_jax_bindings, menclose_surface,
wrapper_classes, wrapper_constructors and wrapper_source_methods. Before removal,
the configured File API sources of every target referenced none of these 18
files, the 247-name active module map contained none of their modules, and a
scan of tracked C++ module imports found no remaining external production,
test or tool importer after the reviewed consumer batches. The portable C ABI,
original Common family algorithms/catalog, immutable Scene, SVG writer/XML
serializer and Source oracles remain. This is retirement of an unused service
graph, not removal of the active mathematical owners or all other inactive files.

After removal, the parallel4 FoliaMathTests/FoliaNativeMath build exits0 with
no work required, confirming the accepted executable is unchanged. The supported
WinUI Debug build and actual worker probe are then rerun through the tracked
PowerShell driver (session8116, retire-svg-worker-verify.log), exit0. Mathbb,
fractions, Unicode, noundefined output, Formula recovery, cache/dependencies,
Clear and disable/re-enable pass; stop join4423ms, probe total8.72s. No Release
or GUI visual inspection is claimed. Removed draft paths no longer have ignore
exceptions that could accidentally reintroduce them as current verification code.

### CommonMrow styled inferred fast path

`common-mrow-style-source.cjs` runs the pinned TeX compiler and real Source SVG
wrappers, then observes ordinary own CSS attributes on the compiled inferred MML
row. The unbroken row keeps dx zero; the broken control retains its styled
line-stack translation of 1.125em. This is a programmatic MML attribute state,
not a claim that the base TeX mmlToken command can author an inferred mrow.
The oracle records the Source commit, profile and five TS/compiled-module hashes;
Root's run exits0 (`common-mrow-style-source.jsonl` and `.stderr` in build/math-port).

CommonMrow now owns the corresponding virtual sceneDx query without retaining
render-state flags. Parallel4 builds pass. The new actual-factory/immutable-Scene
test passes 1 test / 10 assertions, the existing explicit-break test 1 / 14,
and the existing selected family Source-SVG comparison 1 / 1021
(`retire-scene-focused-results.json`). The test reuses math's seeded inferred
child, and its broken control includes an operand so it actually creates a line
stack rather than the distinct embellished-operator path. The full math and
worker checks after old Scene removal passed as recorded below.

### Retained CommonWrapper broken-border coverage

The two obsolete Scene candidate translation units used the withdrawn mutable
NodeRef/Transform representation. Their still-supported ownership, transforms,
HTML, viewport and notation assertions have active typed Scene/Source counterparts.
Their CSS dashed/dotted border checks are retained in the existing Common-to-Scene
test: actual TeX `\\bbox[border:2px dashed/dotted red]{x+y}` reaches CommonWrapper's
border emitter, then checks all four raw endpoints, widths, dash lengths, caps,
red stroke, complete Source SVG and serialization after request teardown.

The independent Source generator appended exactly two records; all prior 148
records, including the canonical first75, remain byte-identical. The extended
selected 88-case test passes 1 test / 1047 assertions (`border-coverage-native.log`
and `border-coverage-native-results.json`); parallel4 build passes. Source helper
`common-border-dash-source.cjs`, its fixture, text-tip oracle and comparer remain.
No production border algorithm was changed.

### Old Scene retirement and current integration gate

The 50 obsolete Scene source files retired in five importer-first batches
(d9222851, 3d9a966d, 8ed96a4e, 9618536b, 57a153f3). Before each removal,
Root checked exact file hashes, all-target configured membership and remaining
tracked imports/direct consumers. The before-removal reports remain under
ignored build/math-port/scene-*-before-removal.json and scene-batch-graph-review.json.
The original Common family implementations and the eight active Scene sources
remain. Two obsolete mutable-Scene test tools retired after preserving their
supported mathematical assertions in existing typed tests, including the two
border cases above (cad58dcd). Independent Source artifacts remain.

After removal, the parallel4 FoliaMathTests/FoliaNativeMath build exits0 with
no work required. The bounded full mathematical run exits0: **870 tests,
217144 assertions, 176 suites, zero skips** (session96710,
retire-scene-full-math.log and retire-scene-full-results.json). Diagnostic
Scene environment variables were cleared for this run.

The supported WinUI Debug build and real deployed worker probe also exit0
(session64450, retire-scene-worker-verify.log). Mathbb, fractions, Unicode,
noundefined output, Formula recovery, shared cache/dependencies, Clear and
disable/re-enable pass; stop join4599ms, probe total9.07s, empty stderr.
No Release or GUI visual inspection is claimed. The canonical inventory records
678 src/math files / 674 code sources, with 560 configured folia_math sources,
247 active interfaces matching the module map, and eight active Scene sources;
the latest File API index is index-2026-10-21T08-13-34-0100.json.

The remaining unused SvgMath pair and TimerHost pair retire separately. Root
read their complete implementations: the former's percentage-width rule is
already in CommonMath::setChildPWidths and the standard catalog uses CommonMath;
the latter schedules only Context ObjectRef callbacks, which are outside the
accepted completed-Scene contract. The typed maction tests retain selected
geometry, text/math tooltip Scenes and delay values, not a simulated event loop.
All four sources have zero all-target memberships and zero surviving tracked
imports/API callers (scene-svgFinal-before-removal.json). The timer draft is
ignored historical material, not a tracked consumer. The parallel4 native
math/DLL build exits0 after removal; no active code or test body changes.

Three pure legacy protocol consumers retire after Root's complete body review:
tests/math/common_factory_live_fields.cpp,
tests/math/mml_factory_context_methods_bad_kind_order.cpp and
tools/math-port/mml-node-context-methods-native-tests.cpp. The first two assert
Context Map/descriptors, creator receiver/getter observation and call-error
ordering; they contain no layout or mathematical output result. The last
asserts method/prototype identity, reflected parent mutation independent of
children and post-runtime errors. Its token kind/text subjects do not add an
algorithm beyond the typed family/tree tests; native_mml_families retains
concrete family isKind and exact token-text copies, and native_mml_row_tokens
retains actual mtext contents/spacing. No configured test or Source record is
removed. The all-target/import check confirms zero configured memberships and
no external module consumers for these three files
(scene-factoryProtocol-before-removal.json). Their providers remain pending
other old-consumer and protected-diff accounting. Independent factory/node
Source observers, locks and records remain; this is not reflective-trace parity.

The Common factory binding chain retires after its last protocol-only getBBox
probe: common_bindings.{ixx,cpp}, factory_context_fields.ixx,
wrapper_class_bindings.ixx, svg/wrapper_factory_bindings.{ixx,cpp}, and
drafts/common-getbbox-lifetime-native-candidate.cpp. Root cross-checked Luna's
method/consumer map against declarations, Context callback implementations,
typed factory catalog/measurement and tracked direct API searches. Dynamic
constructors, live property/Map hooks, prototype walking and callback leases
have no remaining caller outside this batch; all-target membership is empty
(scene-commonFactory-before-removal.json). Original factory, CommonOutputJax
getBBox, linebreak, style and family algorithms stay with their active owners.
The discarded probe asserts only an expired Context getBBox callback, with no
geometry output. Its obsolete ignore exception also retires. Independent Source
factory/layout witnesses remain; broader tree/Map bindings are outside this batch.

The final parallel4 math/DLL build exits0 (session69747,
factory-retirement-build.log/results.json). CMake regenerates for the two
removed unconfigured test paths. Before/after SHA-256 values of FoliaMathTests.exe
and FoliaNativeMath.dll are identical, and all eleven protected paths remain
byte-identical. This verification checks retirement/build membership; it does
not claim a newly repeated full mathematical or worker run.

### Retain substantive leaf assertions at typed owners

Root reviewed the old reference_leaf case map and retained its additional
supported mathematics in the existing native_mml_row_tokens.cpp and
typed_mml_containers.cpp. Real document-owned Mo rows now assert embellished
core selection and the two-operator branch; delimiter rows assert previous
class transfer. Mspace tests retain explicit attribute presence, negative
Unicode-whitespace-prefixed widths and suppressed newlines; mtext retains
explicit style/background presence. Math tests distinguish own attributes,
explicit inherited slots and attached-child displaystyle for mode/display
and explicit true/false precedence. No production algorithm changes.

The independent ignored math-display-source-probe.cjs executes real Source
MmlFactory nodes and passes all six observations, with empty stderr. It checks
the existing p9 math.ts/runtime lock and records exact MmlFactory/Attributes
dependency hashes; it does not imply these dependencies are entries in p9.
Source getInherited reads defaults through the inherited object's prototype;
typed inherited() is the explicit slot and effectiveInherited() adds that
fallback. Raw Source own-slot observations justify the tests' layer checks.

The first candidate build failed on three explicit string_view-to-owned-key
conversions in the test; Root fixed those conversions and required headers.
The subsequent parallel4 math/DLL build exits0
(mml-leaf-test-build-fixed.log). All six bounded focused runs pass, each
executing exactly one test: **78 assertions total**, recorded in
mml-leaf-focused-results.json and mml-leaf-*.log. This is new focused test
acceptance, not a repeated full math/worker run or replay of old Context traces.

After that focused acceptance, reference_leaf.cpp and its obsolete standalone
native driver leaf-mutation-test.cjs retire. The leaf Source oracle and p9 lock
remain; its generated historical C++ observation now goes to ignored
build/math-port/leaf-source-reference.cpp, so running it cannot recreate the
retired tracked Context test. Only the output destination changes; node --check
passes, and the broad Source generator is not rerun. The old test/driver have
no configured membership (scene-leafReference-before-removal.json).
The final parallel4 build exits0 (session85509,
mml-leaf-retirement-build.log/results.json), with unchanged test/DLL hashes
relative to the focused-test build and all eleven protected paths byte-identical.

### Retire the mutable font observers

Root read the complete font_data_source.cpp body and the actual configured
font repository contract. Three Luna packets read the ten old standalone
font tools, their outputs, direct consumers and companion CJS scripts; Root
cross-checked their findings against the typed NewCM provider/data path.
Final glyph/variant/cache/remap/metric/delimiter data remains covered by the
accepted complete font-data gate and current native_raii_fonts/package tests.
Mutable FontDataClass receivers, charOptions tuple identity/Missing/null slots,
ListPatch array deltas and reflected fields are retired representation contracts,
not missing methods to add back to the immutable FontRepository interface.

The first batch removes font_data_source.cpp and three old JSON observers
(font-data-native-candidate, font-methods-native-candidate,
font-tuple-presence-native-candidate), plus their two native-format comparators.
The current typed font-data-dump.cpp / font-data-compare-native.cjs gate stays,
as do independent Source oracles, locks, data and licenses. All six paths have
zero configured membership (font-fontDataConsumers-before-removal.json).
No active implementation or configured test changes in this consumer batch.

The second batch retires the four field/raw/lifecycle/replacement C++ probes
and their three DynamicFile native-format comparators. Their effective assertions
are replaceable Context Map/descriptor/receiver operations and callback/promise
lifecycle; the success probe injects synthetic PUA glyphs through a test host,
not a real NewCM shard. Current typed glyph data and independent Source
lifecycle/field oracles remain. The seven paths have zero target membership
(font-fontLifecycleConsumers-before-removal.json); no production/active test edits.

The third consumer batch removes the old option-array tests, two NewCM runtime
provider probes and their native-only runner/two comparators (six paths,
font-fontProviderConsumers-before-removal.json). Root read the actual active
newcm_font_module_provider interface/implementation: makeNewcmFontData eagerly
constructs all pinned shards into an immutable bundle; newcmProviderCoverage
measures direct glyph/remap/resource counts. The 40 modules/80 events are Source
provenance metadata, not a claim of native runtime JS/CJS module evaluation.
The old runtime loader class, promise/setup/cache identity and failure/retry
records are retired Context contracts. Keep the independent Source provider
oracle, locks, generated final data and active typed data comparison. No new
runtime loader API or repeated full font comparison is introduced.

With their consumers accounted, the NewCM/Common/SVG font_data_surface pairs
retire together (six sources, font-fontBindings-before-removal.json). They
install Context constructors/fields/methods and project extension records;
the active generated typed data, font lookup/resource algorithms and Common
layout owners remain. Root's tracked direct-API search finds no outside caller,
and the current all-target/module-map check finds no configured source/importer.

The support batch removes the unused svg_extensions, font_data_bindings,
font_data_options and bbox_bindings pairs (eight sources,
font-fontSupport-before-removal.json). The old extension builders only served
the retired mutable class surface; their final glyph/variant/resource content
already lives in the current generated typed data. BBox/LineBBox arithmetic
stays in util/bbox and common/line_bbox. Source generators, observations, locks,
licenses and the active typed include are retained; the 18-file byte baseline
is font-retained-before.json. Direction is kept for its real shared type owner.

### Direction owner and post-retirement verification

StretchDirection now lives in the existing Direction.ts counterpart,
output/common/direction.ixx. FontData re-exports it and retains DelimiterRecipe;
CMake registers that one original interface. Enum values and all mathematical
algorithms are unchanged; no compatibility aliases or second type are added.
The parallel4 FoliaMathTests/FoliaNativeMath build exits0 (session42226,
font-direction-build.log). Nine bounded focused tests execute exactly once
each and pass **26340 assertions**, including pinned NewCM values, Mo/table
stretching, all generated fraction/radical pairs, complete Source SVG and
LineBBox operations (font-direction-focused-results.json / font-direction-*.log).

The supported WinUI Debug build and real deployed worker probe exit0
(session56295, font-direction-worker.log): mathbb, fractions/cache/dependencies,
Unicode, noundefined, Formula recovery, Clear and disable/re-enable pass;
stop join4372ms, elapsed8.93s, empty stderr. No new full math, full font-data
comparison, Release or GUI visual run is claimed. All 18 retained font/data/
oracle/license files and eleven protected paths are byte-identical.
The new File API index index-2026-10-21T09-05-51-0865.json records 654 src/math
files / 650 code sources, 561 configured folia_math sources and 248 interfaces
matching the module map, with zero unresolved active imports and 168 remaining
unconfigured cpp/ixx sources. The original direction interface is now active.

### Context record protocol test retirement (2026-10-21)

Root read all nine remaining non-media dirty diffs and preserved their exact
working bytes plus a binary patch in context-dirty-backup-20261021; all nine
copies match scene-batch-before.json, and git apply --reverse --check passes.
The other two protected media paths remain untouched.

The unused context_object_record_retirement.cpp now retires separately after
Root and Luna reviewed its complete body, including all 309 uncommitted added
lines. Its fifteen tests assert only the withdrawn Context record table,
handle/alias errors, diagnostics and reentrant manual cleanup protocols; none
asserts mathematical layout, MML, DOM or SVG behavior. All seventeen targets in
the existing Debug File API codemodel omit it, and no tracked native runner
references it. No Context provider or active owner is removed by this batch.
Evidence: context-record-disposition.md and context-record-before-removal.json.
This is static retirement review, with no new build or runtime pass claimed.

### Native behavior retained before Lite consumer cleanup (2026-10-21)

Three existing test owners now retain the genuine behavior found in obsolete
Context consumers: native_raii_mml_base checks each repaired fraction child is
an mrow; xml_dom_adaptor checks XMLNode serialization through its retained real
Lite/XML adaptor after the caller releases that adaptor; native_renderer_bridge
directly exercises the C++ Renderer result/Scene lifetime, persistent macros and
tags/references, matrix cells, malformed-input recovery and pre-cancel/Unicode
RangeError recovery. Retained Scenes serialize to the complete same SVG after
Renderer destruction using an independent immutable font resource catalog.
The original C ABI tests remain intact; no production implementation changed.

The first owner test incorrectly assumed resolved mathematical glyph keys were
ASCII; its four matrix assertions failed normally. It now compares each cell
with the resolved glyph from an individual render. The initial failure log is
preserved as lite-coverage-public-owner.log. The fixed parallel4 build exits0
(lite-coverage-build-fixed.log); eleven bounded filters execute **15 tests /
1733 assertions**, all passing (lite-coverage-fixed-focused-results.json),
including existing Lite/TeXHTML/XML, complete pinned Source SVG and C ABI checks.
All ten remaining protected paths match their original byte baseline;
lite-coverage-check.json records inputs and output hashes. No new full math,
full font comparison, worker, Release or GUI run is claimed.

### Lite object facade and consumer retirement (2026-10-21)

The four obsolete Lite/XML/Renderer/TeXHTML tool consumers and the protected
G259 request-retirement suite now retire before their providers. The original
LiteModule pair then retires, followed by node_store and reflected
source_text_content (including its native-only Array.reduce test consumer).
These twelve paths were removed in six reviewable commits through f35c4f98.
The per-batch lite-*-before-removal.json records exact hashes, no surviving
tracked imports, zero current target membership and the unchanged module map.
The independent Source oracles, locks, fixtures and actual typed Lite parser,
tree/adaptor, XML snapshot and HTMLDomStrings owners remain.

After removal, the parallel4 build exits0 (session66222,
lite-retirement-build.log). All 478 active math source files remain byte-equal;
all seventeen target source lists and the 248-interface module map are unchanged.
The test inputs and both binaries match the preceding 15-test/1733-assertion
gate exactly, so that gate is retained without a duplicate test run. Seven
remaining protected paths match their original bytes. Inventory at f35c4f98
records 648 src/math files / 644 code sources, 561 configured math sources,
162 unconfigured cpp/ixx and zero unresolved active imports. Evidence:
lite-retirement-input-check.json and lite-retirement-final-check.json.

Root's final mixed-assertion review also preserves the zero-child fraction
case: inheritance repairs both missing slots to mrows. The existing public
Renderer lifetime test now retains a real math tooltip, verifying its complete
SVG after Renderer destruction as well as the outer Scene glyphs. The bounded
follow-up build and both changed tests pass **2 tests / 40 assertions**
(mml-consumer-coverage-build.log / mml-consumer-coverage-focused-results.json).
This complements the earlier gate; it is not a new full math or worker run.

### MML request projection test consumer retirement (2026-10-21)

The protected mml_private_renderer_allocation_domain_g227.cpp now retires as a
separate consumer batch (5bd8b49c). Its complete Luna review separates 38 old
capture/identity/descriptor/retirement tests from the embedded real fraction
repair, text-copy and public Renderer/Scene assertions. Root checked those mixed
assertions against the existing typed copy tests and the preceding strengthened
fraction/Renderer tests. Exact working bytes and the binary diff remain in the
original nine-file backup; mml-request-before-removal.json records the hash and
zero membership across seventeen targets. No MML provider or algorithm retires
in this batch.

The post-removal parallel4 build exits0 (session97930,
mml-request-retirement-build.log). Its five verified test/binary files are
byte-equal to the two-test/40-assertion follow-up inputs; all 478 active math
source bytes and seventeen target source lists remain unchanged. The six
remaining protected paths match their original bytes. The refreshed canonical
manifest at 5bd8b49c still records 648 sources / 644 code, 162 unconfigured
cpp/ixx, 561 configured sources, 248 exact interfaces and zero unresolved active
imports. Evidence: mml-request-retirement-check.json / mml-request-final-check.json.
No additional runtime, full math/font, worker, Release or GUI run is claimed.

### MML factory consumer copy coverage (2026-10-21)

The allocator, factory-field and node-field consumers were reviewed by test,
with the long factory-field consumer split at a complete test boundary. The
values/shared-module/token-walk consumers and the methods draft were reviewed
separately. Context descriptor/getter/prototype/identity/iterator traces are
withdrawn protocols; actual MML attributes, token text, row/core, tree operations,
copy and Scene lifetime remain with their existing typed owners.

One real gap was retained in the existing mml_native_factory.cpp: virtual copy
unwraps a sole inferred row, copies a token with a real TextNode child and its
own attributes, leaves independent attributes, and leaves source links/content
unchanged. Root corrected the candidate's initial direct text-on-mi setup to
the proper TextNode-child representation before promotion. No production
implementation or module responsibility changed.

Parallel4 build mml-copy-consumer-build.log exits0. Eleven bounded focused tests
pass **11 tests / 1989 assertions**, including the new copy case (25 assertions),
existing constructor/negative arity/inferred-row conversion, virtual versus
exact clone, parent links, row core, all 43 Source default tables, token walk
and full pinned Source SVG (mml-copy-consumer-focused-results.json).
mml-copy-consumer-check.json records verified test/binary hashes. The six
protected paths and all 478 active math source bytes remain unchanged.
No new full math/font comparison, worker, Release or GUI run is claimed.

Root's final factory-consumer review identified one further concrete gap:
childPosition must skip an inferred row when reporting the slot in the logical
parent. The existing mml_typed_table.cpp now checks the original cell/token
slot-zero cases, a second cell at slot one, and two tokens whose physical
indices differ while both logical slots remain zero. Parent queries and the
actual inferred-row shape are checked directly; tree shape alone was not
treated as proof of childPosition. The follow-up parallel4 build exits0
(session75046, mml-position-consumer-build.log) and its bounded focused test
passes **1 test / 12 assertions** (mml-position-consumer-focused-results.json).
No production algorithm changed.

### MML Context consumer cleanup verification (2026-10-21)

Six reviewed consumers retire in five small batches through c5b4853e: allocator
ownership, node fields, the methods draft, factory fields, shared Context module
and projected token walk. The two real behavior gaps above remain in their
existing typed test owners. No production implementation retires in this batch.

The final parallel4 build exits0 (session60121,
mml-consumer-retirement-build.log). All 478 active source bytes, six protected
paths, seventeen target source lists and 248-interface module map are unchanged.
The two modified test files and both binaries match the successful position
gate; the earlier eleven-test gate and subsequent position test remain the
runtime evidence, without an unnecessary duplicate run. Refreshed inventory
still records 648 sources / 644 code, 162 unconfigured cpp/ixx, 561 configured
sources and zero unresolved active imports. Evidence:
mml-consumer-retirement-input-check.json / mml-consumer-retirement-final-check.json.
The refreshed narrow graph retains the remaining fields/values/methods owner
edges and 61 class_binding test consumers; inactive membership alone does not
authorize provider removal. No new full math/font, worker, Release or GUI gate
is claimed.

### Class consumer review: retained semantics and accent-role correction (2026-10-21)

The 61 class_binding test consumers are split into eight responsibility groups,
with 2,901 static test names recorded in class-consumer-review-snapshot.json.
Luna reports are static reviews, not runtime acceptance or deletion approval.
Root returned broad coverage claims for exact assertion mapping; the old
consumers remain tracked while uncovered semantics are resolved.

Seven focused behavior tests now live in four existing test owners: the full
8-by-8 TeX spacing matrix and both script suppression paths; stable error-tree
reverification; paired post/prescript level and prime alternation; MathChoice
display/clamped-level/empty-branch selection; operator form precedence; and
accent role/width rules. The first build failed only in the test printer's
UTF-16 vector comparison; explicit boolean assertions corrected that diagnostic
instantiation without changing the behavior under test.

The accent-role test first reproduced one failed assertion out of twelve:
munderover chose the over key merely because an over core existed. Pinned
mo.ts:299-303 compares the current operator identity to each role's core.
The original mo_classes.cpp now preserves those identity comparisons in place;
no other node family or layout owner changed. The fix and its three operator
tests are committed in 93544b62; other tests in 4b7c7bcf, 26921683 and 311766cb.

The parallel4 fix build exits0 (class-accent-fix-build.log), followed by
**8 tests / 1299 assertions**, including full pinned Source SVG. Evidence:
class-accent-reproduce-results.json, class-accent-fixed-results.json and
class-coverage-check.json. Of 478 active source files, 477 are byte-equal and
the sole change is the reviewed mo_classes.cpp fix; all six protected paths
remain unchanged. No full math/font, worker, Release, GUI or new walk-depth
boundary run is claimed. Inheritance's initial explicit-only mathsize theory
was rejected after reading Source Attributes.isSet: own and inherited both
count, while default-only does not. No production inheritance change was made.

An independent locked MathJax 4.1.3 CJS probe then directly invoked the Source
getter/inheritance methods for all twelve accent-role and five width/role
observations: **17 assertions pass**, exit0, empty stderr
(class-accent-source-observations-fixed.json). Module hashes and the existing
compiled-package lock were checked. Its first metadata pass named nonexistent
standalone mover/munder modules; these classes actually share munderover.js.
Root corrected that dependency list before running the successful observation;
the initial dependency failure remains recorded, not reported as a Source
behavior failure.

### Inheritance and NodeUtil value coverage (2026-10-21)

Three existing owners now retain seven focused cases in 8f486bb4, 9c643891 and
85d29a15: own/inherited/default-only mathsize sources and source immutability;
NodeUtil attribute/property value copy, independent destination mutation,
requested operator form, postfix fallback and own/inherited false precedence;
child slot ownership and cross-parent detach; detached logical position and
two nested inferred rows mapping to the outer parent's slot one.

The first five cases pass 5 tests / 46 assertions. The original slot candidate
then indexed former slot two after native detach had compacted its old parent
to length two. CRT failure stayed on stderr; broader evaluation stopped. Root
corrected this test to the existing MmlDocument ownership contract without
changing production code. The bounded slot follow-up passes 1/12, and the
extended position case passes 1/8: seven distinct cases, 66 assertions across
the recorded passes. Final parallel4 build exits0; all 478 active source bytes
and six protected paths remain unchanged. Evidence:
class-helper-coverage-check.json. No additional broad gate is claimed.

### Maction inheritance receiver validation (2026-10-21)

Root's Source review rejected the initial classification of malformed
actiontype as disposable exception behavior. Source maction.ts calls
toLowerCase before dispatching child inheritance; typed null/bool/number inputs
are reachable through native AttributeSet and must be rejected there. This is
separate from verifyAttributes clearing selection for non-toggle values.

Test-only reproduction fails 21 of 33 assertions. The original
maction_behavior.cpp now rejects non-string input with std::invalid_argument
and reuses the configured Unicode lowercase primitive; no Context/property
projection or new module is added. This maps the native finite type boundary,
without promising JS TypeError names/messages. The tests cover unchanged child
state before failure, continued inheritance after changing the same node to
toggle, uppercase TOOLTIP with isolated tooltip style/prime, and default versus
explicit toggle. Source's existing branch and family responsibilities remain.

The parallel4 fix build exits0, then three cases pass 3/53 and full pinned
Source SVG passes 1/1047. An independent locked MathJax 4.1.3 CJS probe passes
all six corresponding Source scenarios, including its three TypeErrors and
same-node continuation, with empty stderr. Evidence:
maction-inheritance-check.json / maction-inheritance-source-observations.json.
Of 478 active source files, 477 remain byte-equal and the only new change is
maction_behavior.cpp; six protected paths remain unchanged. Commit be421fe0.
No full math/font, worker, Release, GUI or new walk-resource gate is claimed.

### Position consumer and generator retirement (2026-10-21)

The positions packet's four old C++ suites now retire after the native value,
slot and logical-position assertions above are retained. Root first resolved
their tool consumers: the Parent/childPosition and NodeUtil-order oracles keep
the original Source execution, pins and exact profile check while dropping old
C++ fixture emission. The inheritance regression tool drops only the two
retired fixture entries and adjusts its summaries; its unrelated fixtures stay
and that legacy native runner was not executed or claimed passing.

The three Source-only checks pass 10,800 main parent cases, 2,160 rebind cases
and 18 NodeUtil-order cases, with exact frozen profile equality. Five shared
Source profiles and their independent loaders/locks remain byte-equal. Six
fixture-only generators/runners retire in 7288e2e8; the two oracle edits and
inheritance fixture-list edit are in 09d0fd01. The four reviewed test consumers
retire separately in 01e584cb; no math provider or algorithm retires here.

The final parallel4 build exits0 (session63368,
positions-consumer-retirement-build.log). All 478 active source bytes, six
protected paths, seventeen target source lists, 248-interface module map and
six verified test/binary files remain unchanged from the preceding successful
inputs. Therefore the helper and maction/Source SVG gates above are retained
without a duplicate runtime run. Refreshed inventory: 648 sources, 162
unconfigured cpp/ixx, 561 configured sources and zero unresolved active imports.
Evidence: positions-retirement-before.json,
mml-consumer-class_positions-before-removal.json and
positions-consumer-retirement-final-check.json. The refreshed narrow graph
counts 57 remaining class_binding test consumers and preserves the complete
Fields/ContextBindings/ParserRuntime chain; inactive providers are not inferred
removable. No new full math/font, worker, Release or GUI gate is claimed.

### MML value and tree verification coverage (2026-10-21)

The mfenced delimiter type boundary now retains all three fields crossed with
null, boolean and numeric inputs: rejection precedes auxiliary-node creation,
and the original children, physical parents and arena size remain unchanged.
All 43 registry kinds also have independent initial flag/previous-getter
expectations, with unique, complete registry coverage. A parallel4 build and
three focused cases pass 477 assertions, including the existing fenced Scene
case. Both independent locked Source probes pass. Commits d6360942/0a352237;
evidence: fenced-flags-coverage-check.json.

The followup parallel4 build and seven focused cases pass 141 assertions:
menclose/mpadded class transfer, nested Math short errors at the actual inferred
parent, both fixMmultiscripts branches, HTML verification, and direct/inferred
replaceChild. Three independent Source probes pass. In particular, Source HTML
dispatches mError to the empty-node no-op; the earlier static expectations of
a TypeError or replacement error node were rejected, without a production fix.
Math and merror's negative-arity inferred rows are explicitly retained in the
tests. The multiscript flag suppresses diagnostics without repairing children.
Evidence: class-followup-coverage-check.json and its three Source observation
files. All 478 active production files and six protected paths remain byte-equal.
No new full math/font, worker, Release, GUI or D2D pixel gate is claimed.

### Three further legacy MML consumers retired (2026-10-21)

Constructor descriptor identity tests retire separately in 83cedbf5: all six
cases concern descriptor lifetime, rebinding, initialization timing or saved
creator protocol, without mathematical results. The independent constructor
identity Source oracle and frozen evidence remain unchanged.

The mfenced boundary and string-error consumers retire after the native
delimiter, typed-family and pinned Scene coverage above. Their two oracles keep
the original locked Source execution and exact frozen JSON checks, now without
C++ fixture emission. Both checks pass, 11 plus 9 observations, in a0c2890b.
The legacy gate drops only those two fixture/count entries; it was not run.
The separate callback-mutation/class-length consumer and its evidence remain.

The removal build passes with parallel4 (session1051). All 478 active source
files, six protected paths, seventeen target source lists, nine verified
test/binary inputs, and ten retained profile/loader/consumer files stay
byte-equal. The preceding 3/477 and 7/141 runtime gates are retained without a
duplicate run. Class-binding test consumers decrease from 57 to 54; no math
provider or algorithm is removed. Evidence:
mfenced-identity-retirement-before.json / mfenced-identity-retirement-final-check.json.
At that checkpoint, the class-state and registry oracles compared only their
C++ fixtures and were pending the preservation step completed below. The original class-state Source
check passes (29 raw values, 1,682 spacing rows and 2,523 level rows; the raw
inputs omit numeric classes 4/5/6, so this is not a full 8×8 enumeration).
The registry's original whole-fixture check fails on its manually adapted C++
Key/creator-slot wrapper. A bounded Source-only diagnostic then proves that
all 387 serialized Source values and 175 generated cases remain byte-identical
to the frozen expectation tail (137,776 bytes); no golden or old wrapper was
rewritten. Evidence: class-state-original-frozen-check.log and
registry-original-check-diagnosis.json. No new broad runtime gate is claimed.

### Class-state, registry and verification consumer retirement (2026-10-21)

The class-state oracle now checks the finite mathematical domain directly:
the independent literal is compared to pinned TS TEXSPACE/TEXSPACELENGTH,
then actual locked MmlFactory mo nodes pass all 194 normal/script/NONE results.
The old fixture's 52 representative finite results agree; the active native
8x8 table and previously passed spacing test body remain unchanged. Arbitrary
JS object coercion, getter traces and boxed/prototype protocols are withdrawn.
No new native runtime or golden data is built for those protocols. Commit cd959200.

Registry migration preserves every original Source expectation. The bridge
checks the original tool/fixture hashes, re-executes the original emitter,
requires the 137,776-byte expectation tail to match, and allows only the
known C++ wrapper Key/creator-slot adaptations in its prefix. It captures 176
top-level Source values into a lossless, independent JSON profile. The new
check-only oracle passes 43 keys, 172 construction cases, 1,849 type relations
and 35 protocol observations against that profile, without native input or
C++ emission. The protocol observations remain Source provenance, not native
API requirements. Commit f13b3e4f.

The Math verification and repair-option oracles retain their original Source
inputs and frozen profiles byte-for-byte; both Source-only checks pass, 72 and
18 cases. Their legacy gate lists drop only the matching consumers/counts.
Commit 1b01ff26. The two fixture-only class/registry runners retire in d40acdef;
the remaining base gate retains a required primary fixture and its other suite.
No legacy native runner was executed or claimed valid.

The four C++ consumers retire in two batches: class-state/registry in 8ec3261c,
then Math verification/repair options in 2b547b6d. The parallel4 removal build
passes (session63913). All 478 active sources, six protected paths, seventeen
target source lists, nine verified input/binary files, nine new tool/profile
files and eleven other retained files remain byte-equal to the verified inputs.
The preceding native 3/477 and 7/141 gates are retained without duplicate runs;
class_binding test consumers decrease 54 to 50. No math provider or algorithm
is deleted. Evidence: class-source-oracles-promoted.json,
class-source-only-check.json and class-source-retirement-final-check.json.
No new full math/font, worker, Release, GUI or walk-resource gate is claimed.

### BaseNode script classification (2026-10-21)

The existing typed_mml_scripts.cpp now checks actual msubsup nodes with mo,
mi, mn and TeXAtom bases against four locked Source observations. The test
preserves the factory's inferred TeXAtom row, parent links, returned node,
wrapper/base class transfer, independent sub/sup classification and unchanged
incoming previous node. This closes the AbstractMmlBaseNode coverage gap;
the existing LayoutNode test is not substituted for it. No production module
or algorithm changed. The parallel4 build passes, and the single focused
native test passes 55 assertions. Evidence: script-base-class-source-observations.json,
script-base-promoted.json, script-base-build.log and
script-base-native-covered-results.json. No broad runtime gate was repeated.

### Constructor/container consumer retirement (2026-10-21)

The constructor's original 88-case Source profile passes unchanged. The
container oracle first passes its complete original fixture/profile check,
then passes the same 6,480 frozen rows after removing only C++ emission.
Both independent profiles remain byte-equal. Actual BaseNode behavior is
protected by the preceding four-branch 1/55 native test; factory kinds,
defaults, attributes and LayoutNode coverage retain their previously accepted
tests. Sparse/prototype arrays and custom callback/getter choreography are
Source provenance, not native API requirements.

The four fixture-only tools retire in 39ab4396; the shared inheritance runner
drops only the constructor's 91 assertions/two tests (remaining 1,449/13),
and was not run. The two unconfigured C++ consumers retire separately.
Removal build57997 passes with parallel4. All 478 active sources, six protected
paths, nine distinct verified input/binary paths, six retained files and
seventeen target source lists remain byte-equal. Class-binding cpp/ixx test
consumers decrease from 50 to 48.
Evidence: constructor-container-before.json,
constructor-container-tools-promoted.json and constructor-container-final-check.json.
No math provider/algorithm or Source profile is removed; no broad runtime
gate was repeated.

### Remaining parent-position protocol consumer (2026-10-21)

The separate rebind consumer retires after its original 2,160-row Source-only
profile check passes unchanged. Logical Parent/childPosition behavior remains
in the accepted typed table tests, including detached nodes and nested inferred
rows; their bytes still match the earlier accepted snapshot. Dynamic getters,
sparse/prototype arrays and callback rebinding are Source provenance only.
Build42085 passes with parallel4. All 478 active sources, six protected paths,
nine verified input/binary paths, six retained files and seventeen target lists
remain byte-equal. Class-binding cpp/ixx test consumers decrease 48 to 47.
Evidence: position-rebind-before.json, position-rebind-removed.json,
position-rebind-source-check.log and position-rebind-final-check.json.
No provider, mathematical algorithm or frozen Source profile changed; prior
runtime gates are retained without repeating them.

### HTML observer/consumer retirement (2026-10-21)

The HTML Source-only oracle replaces the existing owner in 6d0459f7; no parallel
oracle or C++ emitter is introduced. Its one-time ignored bridge first checks
the original oracle and complete 52,398-byte frozen C++ fixture, then proves
all 75 ordered scenario inputs and observations equal the clean profile.
All 87 legacy p()/Property calls match direct deep expectations in order:
29 distinct top-level expectations recursively contain 52 distinct values.
The earlier candidate incorrectly treated 52 as a top-level count; that check
failed before any profile/canonical write and was corrected before promotion.
Nineteen Source pins remain checked. The compact independent JSON profile and
canonical check pass 75 cases/434 assertions. The old p23m observer profile
also passed its original 434/75/570-record check and remains byte-equal as
historical evidence; its emitter-anchor tool retires with two fixture-only
tools. The shared legacy gate drops only HTML's 434/75 and its dedicated edit,
leaving 5,309 assertions/162 tests; it was not run.

The legacy HTML C++ consumer retires after the previously accepted native
HTML token/serialization, Text/Comment wrapping, clone and verify coverage.
HtmlNode inherits the empty-node mError no-op; the accepted test preserves
that behavior for explicit/null options under token and row parents. Removal
build74847 passes with parallel4. All 478 active sources, six protected paths,
nine verified input/binary paths, seven retained paths and seventeen target
source lists remain byte-equal. Class-binding cpp/ixx consumers decrease
47 to 46. Evidence: html-source-bridge-v3.log, html-source-promoted.json,
html-retirement-before.json and html-retirement-final-check.json. No production
algorithm or repeated broad runtime gate is involved.

### Mfenced captured-length protocol consumer (2026-10-21)

The original four-row class-length fixture/profile check passes, then the
same frozen profile passes after its existing oracle becomes Source-only.
These cases register a custom callback which grows, shrinks, rebinds or nulls
the containing JS childNodes array during classification. The accepted finite
production families do not perform that operation. The protocol fixture is
therefore withdrawn without changing the typed Mfenced algorithm or adding
mutable JS-array behavior. Existing fake-operator ownership and pinned Source
Scene coverage remain unchanged. The legacy fenced gate drops only this 4/1
fixture and its dedicated loop-length edit, leaving 988 assertions/226 tests;
it was not run.

Removal build28779 passes with parallel4. All 478 active sources, six protected
paths, nine verified input/binary paths, seven retained paths and seventeen
target source lists remain byte-equal. Class-binding cpp/ixx test consumers
decrease 46 to 45. Evidence: mfenced-length-before.json,
mfenced-length-original-source.log, mfenced-length-source-only.log,
mfenced-length-promoted.json and mfenced-length-final-check.json. No production
algorithm changed and no broad runtime gate was repeated.

### Walk/replacement resource boundary disposition (2026-10-21)

The two still-retained legacy depth tests exercise real recursive token walk
and negative-arity replacement, but their 512/513 RangeError condition comes
from the old P7 host adaptation, not the MathJax algorithms. Root accepts
withdrawing that host budget and shared std::any identity protocol from the
native consumer contract. Existing typed preorder/Text/HTML skip, direct and
inferred replacement parent/return tests and request-level RAII failure
recovery remain the accepted behavior. This disposition does not introduce
a general depth policy or claim bounded behavior for arbitrary deep trees.
Their complete original fixture/profile checks pass, followed by exact checks
of all 780 token-walk and 1,504 replacement rows after the original oracles
become read-only Source-only checkers. The complete Source input/algorithm
prefixes remain byte-equal; profiles remain unchanged. The two fixture-only
native gates and two old C++ consumers retire. The previously rejected ignored
gate which substitutes an active owner is not promoted.

Removal build57382 passes with parallel4. All 478 active sources, six protected
paths, nine verified input/binary paths, eight retained paths and seventeen
target source lists remain byte-equal. Class-binding cpp/ixx test consumers
decrease 45 to 43. Evidence: walk-resource-decision/decision.md,
walk-protocols-before.json, walk-protocols-promoted.json and
walk-protocols-final-check.json. No new runtime gate or production recursion
change is claimed.

### Accent protocol consumers (2026-10-21)

Both original accent fixture/profile checks pass (raw accent72, math accent33).
The bounded bridge additionally matches every C++ aggregate input/expected
field to its existing frozen Source JSON row with both fixture/profile hashes
locked. The original Source input/algorithm prefixes remain byte-equal after
the two oracles become read-only Source-only checks; the same 72/33 rows pass
again, with unchanged profiles. Native role/explicit-attribute and math-accent
width/base behavior retain their previously accepted tests and Source SVG
comparison. The shared legacy gate drops only the two fixtures and 105/2
counts, leaving 32,723 assertions/160 tests; it was not run.

Removal build97637 passes with parallel4. All 478 active sources, six protected
paths, nine verified input/binary paths, seven retained paths and seventeen
target source lists remain byte-equal. Class-binding cpp/ixx test consumers
decrease 43 to 41. Evidence: accent-protocols-before.json,
accent-expectations-bridge.log, accent-protocols-promoted.json and
accent-protocols-final-check.json. No production algorithm or new native
runtime/broad gate is involved.

### Simple MML independent Source profile (2026-10-21)

Root separated the existing five-family observer from C++ emission. The
one-time ignored observer reproduces all 339,824 bytes of the frozen fixture,
then the clean owner matches all 106 ordered scenarios, 368 direct equals,
349 deep expectations, 31 configuration observations, 102 calls and three
complete matrices (1,605 rows). Every one of 8,625 legacy p/list operands
matches in order; their recursive values are exactly the original 296-value
graph. The 732 static C++ expect calls include matrix loop bodies; 357 are
static deep calls, while 349 are ordinary Source deep observations. The
original reported assertion count is 7,667. These counts have distinct scopes.

The independent tagged JSON preserves undefined/null, NaN/infinities/-0,
UTF-16 strings, ordered object keys and array lengths/keys. It passes exact
checking through the replacement existing simple-mml-oracle.cjs owner.
No native values generate expectations; no C++ emitter/slot table survives in
the canonical owner. The native defaults, menclose/mpadded, table alignment
and mglyph owners retain their previously accepted coverage. A separate Luna
static review found no specific omission; Root owns the executed proof.

The fixture-only simple runner retires. Shared action gates drop only the
simple fixture/counts and its five method edits (main remains 1,364/1,346;
red has no count change). The compile-only group retains mfenced as primary
and maction as additional fixture. None of these legacy native gates ran.
Evidence: simple-source-migration/bridge-proof.json,
simple-source-migration/independent-review.md and simple-source-promoted.json.
The C++ consumer then retires separately. Removal build63833 passes with
parallel4; all 478 active sources, six protected paths, nine verified
input/binary paths, eight retained paths and seventeen target lists remain
byte-equal. Class-binding cpp/ixx test consumers decrease 41 to 40. Evidence:
simple-consumer-before.json and simple-consumer-final-check.json. No repeated
native/broad runtime gate is claimed.

### Script observer independent Source profile (2026-10-21)

The original P10 oracle and instrumented observer each pass the complete
185,969-byte legacy fixture check. The bridge matches all 17 Source scenarios,
their lossless input witnesses and all 1,683 expected values in order to
continuous C++ expectation ordinals 1..1,683. The clean replacement owner
passes the independent profile at the original script-oracle.cjs path, with
no C++ emitter/actual expressions, slot table or bridge-writing mode.

The first candidate confused scopes: the fixture has 1,686 static expect
sites, while the original reported count is 1,689. Its final native-only
two-kind loop has three static sites executed six times; Source contributes
1,683, not 1,680. The bridge failed before writing data, and Root corrected
the scope/count checks and the fixture-name pattern before promotion. The
native-only host budget has no Source expectations and retires under the
already accepted budget disposition. Existing string, script MML and Common
layout tests remain accepted without duplicate runtime runs.

Two fixture-only/historical compile tools retire. The operator/script main
and red groups remove only script's 1,689/18, leaving 31,034/142 and 31,005/139.
A stale reference_leaf.cpp entry was discovered in the consumer group:
that fixture already retired in 6f7cc0e7. Only that missing fixture and its
669/27 counts are removed, leaving 4,640/135. These legacy native tools were
not run. Evidence: script-source-bridge-v2.log, script-source-promoted.json
and script-consumer-before.json. The script C++ consumer retires separately:
build92516 passes with parallel4. script-consumer-final-check.json confirms
478 active sources, six protected paths, nine verified input/binary paths,
seven retained paths and seventeen target lists unchanged; class-binding
cpp/ixx test consumers decrease 40 to 39, interfaces remain 248 and unresolved
imports remain zero. No production algorithm changed or broad runtime gate
was repeated.

### Multiscript independent Source profile (2026-10-21)

The original oracle and observer reproduce the full 319,433-byte fixture.
All 226 Source cases, 1,304 ordered expectations and 480 raw inheritance rows
match the clean check-only owner. Its 825 static expect sites expand to 1,304
at runtime; 1,170 recursive materialization records are a separate count.
The bridge checks all 2,631 legacy p/list operands against their own case's
lossless input/expected ledger, and every matrix row's fields and expectation
in order. Native multiscript and Common script coverage remains unchanged.

Root corrected candidate result-envelope/matrix decoding issues, used lossless
encoding to distinguish NaN from null, and replaced a timed-out recursive
lookup with a case-local index. The final bridge completes within its original
60-second bound; no native program ran. Evidence: multiscript-source-bridge-v4.log
and multiscript-source-promoted.json. The existing multiscript-oracle.cjs is
now Source-only and exact-checks the compact independent profile. Its legacy
C++ consumer and dedicated tools await a separate removal batch.

### Table independent Source profile (2026-10-21)

The full legacy table fixture matches both its original oracle and observer.
The Source-only owner preserves all 135 cases and 4,700 runtime expectations:
240 raw inheritance rows account for 3,120 expectations, 160 display/default/
cramped rows for 800, and the remaining observations for 780. The bridge
separately records 790 static expect sites, 428 recursive value records and
4,146 ordered p/list calls; every operand occurs in its own case's full input
and expectation ledger. All matrix fields and ordered observations match.
Root added the exact verify-options and outer-inherited argument records
after the completeness check identified their absence; no expectation changed.

The existing table-oracle.cjs now checks the compact independent profile,
with no C++ emitter or profile-writing mode. Evidence: table-source-bridge-v3.log
and table-source-promoted.json. No production algorithm or accepted native
test changed; the old C++ consumer and tools await their separate removal.

### Table/multiscript consumer removal (2026-10-21)

The two legacy C++ consumers retire after the independent profiles above.
Their two dedicated runners and the completed parent-fixture selector retire;
the existing 88-case parent-verification include remains byte-identical.
The shared table/multiscript group retains its other fixtures with a total
of 13,587 assertions/11 tests, using table-repair as primary. The old red
group retains only its pending include at 2/1; compile-only observers remove
the two retired inputs. These legacy runners were not executed.

Removal build5483 passes with parallel4. The final check preserves all 478
active sources, six protected paths, nine verified input/binary paths, fifteen
retained paths and seventeen target lists. Interfaces remain 248 with zero
unresolved imports; class-binding cpp/ixx test consumers decrease 39 to 37.
Evidence: table-multiscript-consumers-before.json, -removed.json and
-final-check.json. Existing accepted native behavior remains byte-identical;
no new runtime acceptance or production cleanup is claimed by this batch.

### BaseItem Source owner representation (2026-10-21)

The original BaseItem oracle still matches all 85 frozen JSON rows. Its C++
comparison fails on precisely two historical type adaptations from 951385dc:
the typed defaultKind accessor and factory creator-slot arguments. Applying
only those two uniquely matched adaptations to its regenerated C++ reproduces
the entire locked fixture. The proof records the original assertion failure;
it does not claim direct old-fixture byte equality.

The actual pinned Source replay, the clean candidate and all original ordered
label/expected pairs agree exactly. The existing p42b JSON keeps every one of
those pairs; only unused cpp replay fields are removed. The existing
base-item-oracle.cjs now performs the real Source replay and checks that same
profile, with no emitter, hash-only substitute or second owner file. The 85-case
canonical check passes. Evidence: base-item-original-mismatch.json,
base-item-source-migration/executed-proof.json and base-item-source-promoted.json.
Existing native stack/frame/error/annotation owners remain unchanged; the
legacy C++ consumer and its dedicated runner await separate removal.

The legacy BaseItem C++ consumer and its fixture-only runner then retire in a
separate batch. Build89489 passes with parallel4. The final check preserves
478 active sources, six protected paths, nine verified input/binary paths,
twenty-four retained paths and seventeen target lists. Interfaces remain 248
with zero unresolved imports; class-binding cpp/ixx test consumers decrease
37 to 36. Evidence: base-item-consumer-before.json, -removed.json and
-final-check.json. Native base_items, stack, parser and source-annotation
coverage remains byte-identical; no repeated runtime gate is claimed.

### Base verification Source owner (2026-10-21)

The original mml-verification-oracle.cjs passes the full 435,209-byte fixture
and p32 JSON check. The separate flags owner reconstructs and checks all 576
supplemental inputs. The replacement existing owner preserves the original
Source input construction and run function, exact-checking all 3,176 main rows
plus all 576 supplemental expected values. Both profile hashes remain unchanged.
The canonical owner and flags consumer pass after promotion; exports retain
the original run entry. p35 belongs to a different math-verification consumer
and was not used as replacement evidence.

The legacy fixture's four additional assertions concern callback/options
identity and the old host traversal budget; they are separate from the 3,752
Source expectations. They do not establish a MathJax depth rule and are not
extracted into another old-protocol test. Existing native verification/error
and RAII recovery coverage remains unchanged; no depth policy is added.
Evidence: verification-source-executed-proof.json and
verification-source-promoted.json. No new native runtime gate ran; the legacy
C++ consumer and dedicated runner await their separate removal batch.

The verification consumer and dedicated runner then retire separately.
Build6566 passes with parallel4. verification-consumer-removal-final-check.json
preserves 478 active sources, six formerly protected paths, nine verified
input/binary paths, thirty-one retained paths and seventeen target lists.
Interfaces remain 248, unresolved imports zero, and class-binding cpp/ixx
test consumers decrease 36 to 35. No new native runtime gate ran.

### Copy observer retirement (2026-10-23)

The completed `copy-source-executed-proof.json` retains the full 299-row sparse
Source corpus, its 108 selected copy observations, and the six independent
boundary observations. All twelve recorded file hashes matched before removal;
the completed Source/mapping execution was not repeated. The two C++ observers
and their freeze mappers retire, while all Source owners/profiles, metadata's
32-case observer, CMake and the existing native copy tests remain byte-identical.
The historical inheritance configuration decreases only 114 assertions/two
suites, to 1335/11. Dedicated legacy tools retain only metadata32/1 and its seven
previous mutation definitions. These object-protocol tools are pending migration
and their static configuration checks are not current native runtime gates.

`copy-retirement-build.log` builds FoliaMathTests successfully with parallel4.
The existing inferred-copy and virtual-copy-versus-clone cases each execute
25 assertions: two executed cases/50 assertions pass. Other cases were filtered;
this is not a complete mathematical test run. Logs are
`copy-retirement-native-inferred.log` and `copy-retirement-native-virtual.log`.
`copy-consumer-retirement.json` records preserved hashes and the narrowed direct
class_binding importer list: 35 to 33 cpp/ixx files, with four inc files unchanged.
No production implementation or application integration changed in this batch.

### Copy metadata observer retirement (2026-10-23)

The existing `mml-copy-metadata-oracle.cjs --check` executes all 32 original
inputs with their original pins and checks the entire profile unchanged:
SHA256 `b9be470fecd1e2432639f036aebb41d9b91f986949890ec78baf0915dce8276b`.
`freeze-mml-copy-metadata.cjs --check` verifies the entire old C++ consumer before
retirement. All sixteen observation fields are accounted for; dynamic JS key
ordering, nested identity/alias mutation and nullable attribute-object state
remain Source evidence rather than requirements for a native object facade.

The existing factory test owner adds one case across eight Source families and
both keepIds states. It checks root-only ID retention, recursive child ID removal,
own string/numeric/bool attributes, independent attribute/property mutation,
new-node default mathsize instead of runtime inheritance, and unchanged source
tree/parents. Native NodeProperties use existing typed fields and value ownership.
The test uses the initialized factory entry, executes 472 assertions and passes.
An earlier test draft used raw arena allocation for a negative-arity node and
raised a caught logic_error; only the test setup was corrected, not production.

The old metadata consumer, mapper and dedicated red/mutation tools then retire.
The legacy shared configuration decreases exactly 32/1 to 1303/10; no copy fixture
references remain in tools. Original metadata/sparse/boundary Source owners and
profiles remain. Direct class_binding importers remain 33 cpp/ixx plus four inc
because metadata was an indirect consumer. `copy-metadata-review/retirement-proof.json`
records the pins, preserved hashes and limits. `final-build.log` succeeds at
parallel4; `all-math-tests.log` passes all 911 tests/218824 assertions. These logs
are under `build/math-port/copy-metadata-review/`, not tracked build products.
No production, installer or application integration changes are included.

### Additional row prototype observer retirement (2026-10-23)

`mml-return-probe-oracle.cjs --prototype --check` preserves both original inputs
and their complete expected observations, canonical LF hash
`92435496a5cf3b04687f1fdb00c5d58f00e7d9d1c13826b970bb3e4f6d9da050`.
The old mapper's complete-file check first exposes historical optional-to-Property
observer drift. A separate bounded check accounts for the entire C++ file with
six explicit helper-format/type changes, including both complete case rows;
it does not repair a retired object layer or claim old native execution.
Evidence is `build/math-port/return-prototype-review/mapping-proof.json`.

The dedicated two-row C++ observer and mapper branch retire. The ordinary
155-row profile, C++ file and emitted mapper content remain byte-identical.
The mapper explicitly rejects its retired --prototype mode and directs callers
to the retained Source oracle. The legacy shared configuration decreases only
2/1 to 1301/9; the returns configuration becomes187/2 and removes the two
row-prototype mutations whose observer has retired. Remaining legacy tools are
pending migration, not current native gates.

Prototype property lookup/own-versus-inherited slots are Source-only behavior.
Normal MML row core/coreMO projection, cache invalidation and clone ownership
remain at native_mml_row_tokens.cpp and mml_exact_clone_row.cpp. Their files and
the already-passing native binary are unchanged from the preceding full run
(911 tests/218824 assertions). No extra mathematical algorithm or protocol test
is introduced. retirement-proof.json records these preserved hashes and direct
class_binding consumers33 to32 cpp/ixx, with four inc unchanged. The ordinary
155-row corpus still includes other protocol observations awaiting review.

### Remaining returns and predicates observers (2026-10-23)

Five old observers retain all 32/155/75/111/7 original Source observations.
Independent Source checks pass for returns32, returns155, predicates111 and
rebind7; predicates75 is within the full299 corpus whose existing completed
proof and owner/profile hashes remain unchanged. The completed proof is reused.
Full C++ mapping passes with explicit historical Property/accessor/formatter
drift only; no input or expected row changes. Native mathematical behavior stays
with the unchanged row/token, exact-clone, arithmetic and container owners.
Five C++ observers, four mappers and four dedicated old runners retire. Shared
legacy configuration decreases exactly380/5, to921/4; direct class_binding
consumers32 to28, four inc unchanged. All profiles, native owners and the passing
native binary remain byte-identical. Evidence is
`build/math-port/finish-old-tests/returns/{source-proof,mapping-validation,retirement-proof}.json`.
This is not a claim that the remaining object-protocol runner executes natively.

### Class observer closure and real two-child operator forms (2026-10-23)

Five class observers retain all3/432/14/9/28 Source observations. Their original
oracles now only execute Source and check the unchanged complete profiles;
C++ template generation, fixture paths and unused emitter variables are removed.
All five Source checks pass. Protected method return representation, callback
alias/rebind/throw timing and prototype slots remain offline Source evidence.
Actual mathematical row/cell/class behavior stays at existing native owners.
The existing Mo owner adds a real two-child row: first operator form order is
prefix/infix/postfix, last is postfix/infix/prefix. Parallel4 build succeeds and
the single focused case passes2 assertions. The initial test compile exposed
Boost.UT's unsupported UTF16-container diagnostic printer; the comparison uses
the repository's existing boolean form, with production and expectations unchanged.

Five C++ observers and their dedicated base-class gate retire. Shared legacy
configurations remove only the corresponding fixture/suite: cell14, row28 and
forms9. The earlier candidate's row32 count was corrected against its28 rows and
the actual gate summary. Direct class_binding consumers28 to23, four inc remain.
Proofs and logs are under `build/math-port/finish-old-tests/classes/`; no full
native run is claimed for this batch, pending the final combined regression run.

### Inheritance observer closure (2026-10-23)

Eight old C++ observers, five freeze mappers and five dedicated runners retire.
The boundary's 249 cases, 350 expected values and seven pins are retained as
structured gold decoded from the frozen fixture. Existing profiles remain
byte-identical. The completed boundary249, inherit-from9344 and row6 Source
proofs are reused after exact loader/profile relocation; the six independent
inheritance modes pass699/128/16/8/16/8 rows. Consumer54 remains within the
previously checked sparse299 corpus. All 10528 original rows are accounted for;
Source protocol observations remain offline and native inheritance remains at
the existing typed family owners. No production code or native binary changes.
The shared table gate removes only row-inherit6; direct class_binding test
consumers decrease23 to16, four inc unchanged. Evidence is under
`build/math-port/finish-old-tests/inheritance/`.

### Full-domain native value regression coverage (2026-10-23)

The old-test inventory includes useful typed tests that were never configured,
as well as obsolete object adapters. Real values are migrated before retirement.
The existing arithmetic owner adds24 display/level/prime combinations (136
assertions); Mo preserves the phantom-copy spacing regression (5). Five existing
owners retain8 factory/table/script/ParseOptions/regex/textcomp cases, including
the current compiled factory implementation, nodeFactory lifetime and live
text parser's current input-jax owner.

The existing numeric owner replays every6412 shortest/fixed pair, all608 typed
length conversions,512 percent/em and1024 pixel strings,30 unit/table values,
defaults and145 dense reductions:15293 assertions pass. The48 JS intrinsic
return rows remain offline Source evidence. Numeric/storage's original typed
modules are configured only in the test target; application code does not change.
Dedicated js_case/storage owners retain the original full Unicode domain,
1460 mappings,1162 sigma ranges and474 examples (477 assertions), and two real
ordered-storage cases (19). The original13 Unicode regex cases migrate into
the existing RegExp owner with the same35 assertions.

Input Number1013/parseInt944 values pass bit-exactly (NaN payload excluded).
Casefold replays1141 groups/2307 members including complete BMP match positions
and state,18434 assertions. Operator data checks all1252 ordered entries,
944 properties,1252 lookups,27 named definitions,50 ranges/eight spacing rows
and228 range queries. Thirty complete BMP masks match every original shape.
BBox constructor/mutation values pass13 assertions. Build8 and all21 focused
patterns pass; logs are `finish-old-tests/native-{combined-build-8,focused-*}`.
Final combined full-suite validation is still pending.

Candidate mistakes were corrected against the original frozen literals: mask
anchors contained a generator placeholder; input UTF16 literals were double
escaped; the operator candidate flattened forms alphabetically instead of the
original prefix/postfix/infix graph order. No expected mathematical value or
production implementation was changed to resolve these mapping errors.

### Remaining MML family/node/operator fixtures (2026-10-23)

Seventeen C++ observers and five root includes retire after native acceptance.
Six original family owners become Source-only and check complete structured
gold statically decoded from the old C++:7/22/73/934/410/225 cases. Error3204 and
action-error8 retain their original profiles and pass. The original MathChoice
profile's409 Source records and all historical metadata remain byte-identical;
ordinary require replaces the old emitter injection.

MmlNode retains42 Source cases,1051 helper observations and26 additional direct
values omitted by the original wrapper. Its old43/1084 metadata remains intact,
and the historical delayed XML trace difference is recorded without changing
either value. Table-repair64 passes. Parent verification's88 cases/288 values
map exactly to a frozen projection of the accepted full multiscript profile.
The four Unimplemented-only includes retire in favor of real typed family checks.

Five operator owners preserve14/50/14/32/12 Source groups; the old order fixture's
51st group is the now-passing five-assertion native spacing regression. Existing
token-walk780 and verification3752/math-verification72 profiles stay intact.
No object-protocol emulation enters the native test target. The per-file Source
and native responsibility record is `evidence/old-mml-test-retirement.json`.

### Current Debug and self-contained Release MSI (2026-10-21)

Dependency setup passes. The supported Debug application build and actual
worker probe pass (session20610, folia-latest-worker.log). The deployed Debug
math DLL now matches core SHA256 CFDA6103DC0C55754EAC52AB5BDB617F26B3AAF08B8756D9F6DB43096A51A373.
The stale deployment recorded in the preceding handoff is superseded.

The self-contained x64 Release application and MSI build pass (final
session17523, folia-latest-release-msi-v2.log), with zero build warnings/errors.
Folia-0.2.1-x64.msi contains 127 program files and 21 asset files, excluding
the obsolete mathjax-quickjs.js, font JavaScript and worker probe executable.
Third-party notices remain. WiX decompilation/extraction succeeds, and the
actual extracted Folia.exe and FoliaNativeMath.dll match their Release output
hashes; the deployed DLL also matches core-release. The MSI is 53,329,574 bytes,
SHA256 6D044C437C52FC211866D9747738944AF06FE929D8EA252EA5D62DAEC3335541.

The Release probe uses -SkipBuild -SelfContained to preserve MSI deployment
settings and match its static CRT linkage. The original probe's dynamic CRT
link attempt failed before execution; the corrected driver matches the actual
Folia.vcxproj settings. The actual Release worker then passes mathbb, fraction
cache/dependencies, Unicode, noundefined, malformed-formula recovery, Clear and
disable/re-enable (folia-latest-release-worker-v2.log; stop join501ms, total1.19s,
empty stderr). No application rebuild follows this check. Regenerating the
payload after creating the probe produces identical XML, proving it stays out
of subsequent packages. Evidence: folia-latest-release-deployment.json.

All 478 active math source files, six previously protected files, nine verified
inputs/binaries and retained Source profiles remain byte-identical. The user
then explicitly requested committing every tracked change: fe5eba6e preserves
the four pre-existing legacy projection edits, which are outside the active
source list. Their commit is preservation for handoff, not native runtime
acceptance or a new ownership design. The two media paths had no content diff.
No MSI installation, GUI visual test, GitHub Release or tag publication is
claimed. Generated artifacts, local logs and ignored candidates are not Git
deliverables; a new checkout must recreate them with the tracked scripts.

## Evidence and failure handling

Root owns builds, executions, target edits, registered source promotion and Git.
Freeze inputs during a build/evaluation. Record revision, source hashes, actual
exit status, commands, counts and limitations. Stop a broader evaluation on an
abnormal exit and diagnose with a bounded focused run; no desktop error dialogs.
Compile success, static review or a Source-only witness is never native runtime
acceptance. Commit small coherent batches, preserving unrelated work.
