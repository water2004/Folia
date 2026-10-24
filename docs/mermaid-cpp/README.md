# Official Mermaid to C++ migration

The target is Folia's Mermaid dependency, using the official
[`mermaid-js/mermaid`](https://github.com/mermaid-js/mermaid) implementation as
the source of behavior. This is not a translation of `mermaid-rs-renderer` or
a continuation of its unfinished Rust Direct2D backend.

The reference is Mermaid **12.2.0**, commit
`1fb7869866dd619168d840291c9c54b4f129758b`. `source-lock.json` records the source
files used by each implemented component. Source-derived code retains the
upstream MIT notice in `third_party/mermaid-cpp/LICENSE`.

## Dependency boundary

Folia submits Mermaid text and a light/dark choice, then consumes SVG, intrinsic
dimensions or an error. Its caller does not restrict diagram families. The
existing coverage to account for is flowchart, class, state, sequence, ER, pie,
mindmap, journey, timeline, Gantt, requirement, git graph, C4, Sankey, quadrant,
ZenUML, block, packet, kanban, architecture, radar, treemap and XY chart.
ZenUML is an external official integration, not an intrinsic Mermaid parser.
This inventory is not a claim that all of those families are implemented here.

The port includes the parsing, configuration, themes, layout algorithms, text
measurement and SVG generation actually needed for that dependency. Mermaid's
website, documentation application, browser startup/DOM scanning, live editor,
release tooling and unrelated new diagram families are outside this boundary.
Browser event callbacks need a Folia host mapping rather than a JS interpreter.

The official implementation mixes final geometry with SVG/HTML measurement.
Family-specific algorithms must remain recognizable. Extract the required
measurement services and drawing data without substituting a generic graph
layout or character-count text widths. Any algorithm dependency (Dagre, D3,
Cytoscape, date calculations) needs its own source and behavior verification.

## Current state

`folia.mermaid.flowchart::prepare_flowchart` connects shared preparation and
configuration to owned Flowchart layout input. The current classic-syntax
stage supports rectangular/rounded/circle/stadium/subroutine/cylinder/diamond/
hexagon/slanted/flag nodes, plain and quoted Unicode labels, labelled edges,
normal/thick/dotted/invisible and double-ended links, chains, grouped endpoints,
subgraphs, directions, classDef/class/style/linkStyle and accessibility metadata.
Node declaration order, repeated-label replacement, parallel edge ids, compound
parents and compiled class styles follow the official FlowDB/getData boundary.
Spacing, padding, curves, inherited direction and titles consume shared config.
Initialize the supplied session with `theme: default` (light) or `theme: dark`,
as Folia's render entry does. An untouched upstream Flowchart session selects
the unported redux-color theme and returns the existing explicit theme error.

`folia.mermaid.dagre_rank::rank_flowchart` advances this input through Dagre's
first layout stage: DFS back-edge reversal, compound nesting constraints,
default network simplex, edge-label rank reservation and rank normalization.
It returns owned node ranks, group rank intervals and per-edge reversal/self/
label-rank data in the original input order. Edge label boxes are supplied by
the caller; the oracle supplies explicit boxes, not font measurements. Parallel
edges are aggregated only inside the rank optimizer. The original graph stays
unchanged. The default Flowchart node/edge/depth limits also apply to this API.
The original dagre-d3-es 7.0.14 rank pipeline is hash-locked and observed directly.

`layout_flowchart` continues on the same private ranked graph through long-edge
normalization, dummy parenting, compound borders, crossing reduction, BK
coordinates, self-loop placement, direction transforms and restored edge paths.
It accepts explicit node/edge boxes and returns owned node boxes and edge paths
in input order. The full original Dagre oracle covers the same representative
diagrams, including compound groups and all four directions.

The measured Flowchart SVG entry supports explicit `layout: dagre` and
`look: classic`, default/dark themes, square/rounded/diamond/circle and
stadium/subroutine/cylinder, hexagon, slanted parallelogram, trapezoid,
inverse trapezoid, asymmetric notch and double-circle nodes,
quoted/entity plain labels, explicit line breaks and measured automatic wrapping,
basis/linear paths, point/circle/cross/plain arrow ends, and
normal/thick/dotted/invisible links, and compact merged self loops. Self loops
use the original three-segment Dagre graph for placement, then the original
dummy-node side selection, compact path and label-offset rules for one logical
SVG edge. Real DirectWrite widths, line heights and
baselines drive rendering; the plain HTML-label spacing mode retains Mermaid's
CSS line height and minimum node width while emitting native SVG text. There is
no browser runtime, HTML execution or character-count measurement in production.
The display adapter keeps original parser labels, decodes numeric and common
named entities, and converts explicit breaks into display lines. DirectWrite
supplies the actual line breaks and metrics. Nodes use `wrappingWidth`, edge
labels use createText's 200px default and plain group titles remain unbounded.
Native SVG uses measured text spans, including preserved spaces at wrap points.
The same-host browser check compares actual official/native node boxes,
coordinates, text containment, colors and sampled edge geometry. Native font
rasterization can differ slightly from Chromium; geometric tolerances are 1.5
SVG units for boxes/coordinates and 3 for sampled edge paths/lengths.
Stadium nodes retain the original sampled semicircles and measured aspect ratio;
subroutines retain their eight-unit side frames. Cylinders retain the original
elliptical caps, label offset and curved edge intersection adjustment. The shape
comparison checks painted outlines and label centers in connected and grouped
default/dark diagrams, including a contained cylinder self loop.
Classic polygons reuse the original polygon intersections and retain their
shape-specific padding; the asymmetric node shifts its label past the notch.
Double circles measure the inner ring from the label diagonal and retain the
five-unit outer gap. Connected and grouped checks include minNodeWidth: 0.

`folia.mermaid.flowchart_graph::prepare_flowchart_graph` now prepares the
Mermaid graph around that Dagre engine. It anchors edges to externally connected
clusters, extracts independent subgraphs with their original direction settings,
and expands self loops into the original three layout segments. The owned result
retains the source graph, source node/edge indexes, cluster endpoint markers and
nested graph trees for subsequent measurement and painting. The source oracle
executes the pinned original prepareLayoutForDagre/mermaid-graphlib functions on
representative nested, independent, externally connected and self-loop inputs.
This preparation API does not itself measure, lay out or paint the nested tree.

The measured cluster adapter recursively measures independently directed groups,
reinserts their parent cluster, and passes the resulting bounds to the parent
Dagre layout. It retains the original parent rank-spacing increment, cluster
label width/diff, title margins and node-position translations. Non-extracted
compound groups share the parent layout; edges with group endpoint markers use
the original boundary-path cut and label repositioning. Group backgrounds and
plain titles use default/dark theme colors and real DirectWrite measurements.
The browser comparison observes absolute nested coordinates, group boxes and
title positions in addition to the existing nodes and sampled paths. Classic
edge markers retain the original start/end reference points, dimensions,
outlines and theme paints, including bidirectional and group-connected links.
Only point arrows shorten the classic path; circle/cross use marker offsets.
Plain frontmatter diagram titles use real DirectWrite metrics at the original
18px size and theme text color. Titles are centered over the graph before the
viewport expands, use `titleTopMargin` for their baseline and remain unwrapped.
The comparison covers ordinary/wide titles and custom title margins on groups.

Common class/direct styles now paint node and cluster fills, outlines, text
colors, stroke widths and dash patterns. Class declarations precede direct
styles. Link styles also paint edge labels and clone colored markers using the
original rules: point markers change both fill and stroke, circle/cross change
stroke only, and marker color takes the first stroke declaration while the
path uses the final declaration. Dark cluster titles use the separate title
color from the theme. The source declarations remain owned parser data.

Node and edge labels support an installed font family, positive pixel font
size, normal/bold or numeric 100-900 weight, and normal/italic style. DirectWrite
wraps and measures with those parameters; SVG retains the same resolved family,
size, weight and style. This covers whole-label typography in connected and
grouped diagrams. Group-title typography and mixed HTML/Markdown runs remain
separate adapters. The comparison checks computed font properties, display
lines, label positions, node boxes and edge geometry in default/dark themes.

Complete Flowchart rendering remains pending. The SVG entry explicitly rejects
style properties beyond the supported paints/typography, group-title font
overrides, styled HTML/Markdown labels and HTML diagram titles,
and non-classic looks; it does not silently drop these features.
The complete existing family requirements remain unchanged. The rank/layout
API requires prepared graphs without nonempty group endpoints; callers must
measure and lay out extracted subgraphs separately before the enclosing graph.
Styled HTML/Markdown label adaptation,
shape metadata, explicit edge ids and interpolation commands are not implemented
by this stage; unsupported input returns an error. Browser callbacks are outside
the Folia dependency path. No partial graph escapes failures. Default limits
are 10,000 nodes/groups, 500 edges (also bounded by site maxEdges), 64 nested
subgraphs and the shared frontend source budget. Error offsets refer to prepared
UTF-16 text. The representative oracle compares the original parser/DB's layout
nodes and edges, metadata, styles and configuration with the native result.

`folia.platform.mermaid_renderer::render_mermaid` is an opt-in Packet/Pie/Flowchart
source-to-SVG entry. It composes the shared frontend, family configuration,
default/dark theme values, parsing, layout, font selection, real DirectWrite
measurement and SVG output. `folia.mermaid.render_entry` exposes the owned
intermediate data and the platform-independent renderer for other hosts.
Packet uses its own default style in both themes unless themeVariables.packet
overrides it, matching upstream. Pie consumes the resolved palette and sizes.
An undefined palette entry (pie12 in the official dark theme) leaves SVG
fill/stroke attributes absent, matching D3's inherited-paint behavior.
Pie showData comes from the grammar, not a similarly named configuration key.

Family style sizes accept numeric values or px units; paints accept literal
named/hex/RGB/HSL colors through the existing theme color parser. Fonts are
selected in configured list order from installed Windows families. Generic
sans-serif/serif/monospace/system-ui map to Arial/Times New Roman/Courier New/
Segoe UI. The selected family and sizes are shared by measurement and output.
Missing fonts, malformed input and unavailable families return explicit errors.

Body titles override frontmatter titles; plain accessibility title/description
are retained in SVG. Source entities are protected before parsing and decoded
for final measurement/output, preserving the DB's original deduplication keys.
Decimal escapes and quot/amp/apos/lt/gt/nbsp/copy/reg are supported. Other named
entities require decimal spelling. HTML metadata is explicitly rejected: this
entry does not emulate browser DOM sanitization or execute HTML, arbitrary CSS,
URLs, event bindings or host callbacks. Labels remain escaped SVG text.

The application still uses its Rust dependency. Remaining required families
and the application switch are the next functional work; this opt-in entry is
not evidence of a completed dependency replacement or WinUI/DPI acceptance.

`folia.mermaid.frontend::prepare_diagram` composes the original shared frontend
sequence for a configured session: normalize text, extract frontmatter, process
init/wrap directives, clean-and-merge configuration, remove comments, reset a
private session copy, detect diagram scope, apply configuration, then detect
again with the effective configuration. It returns owned raw/cleaned/comment-
bearing source, frontmatter line offset, title, extracted configuration, diagram
type and effective configuration. Folia's caller can reuse a site session without
one request's directives or errors changing the next request.
The extracted configuration is captured before sanitization; unlike the original
aliased return object it remains unchanged when the session applies directives.
The effective configuration is the separately verified renderer input.

The original composed oracle covers representative family detection, Packet/Pie
inputs, default/dark appearance, secure keys, directive/frontmatter precedence,
legacy displayMode, Unicode, line endings and incomplete editing input. The init
and wrap scans share a request-local cursor, preserving the original behavior
after malformed JSON while keeping independent requests isolated. Existing YAML,
configuration and theme restrictions still apply. Source is capped before copying;
delegated operations retain their existing budgets. Preparation does not validate
family grammar, encode parser entities, run loaders, or produce SVG. Packet/Pie
rendering is performed by the separate native entry described above.

`folia.mermaid.diagram_registry` ports `detectType.ts` and the detector portion
of `addDiagrams`: 37 official detector modules plus the `error` and `---`
sentinels, in original order. Registration is once per owned registry; the
large-feature switch controls ELK, mindmap and architecture. Overwriting a key
retains its position but replaces its detector and loader route. Canonical
uint32 keys enumerate first, and dispatch snapshots callback records just as
`Object.entries` does. All builtin regex predicates use linear UTF-16 scans,
including ASCII word boundaries/case folding and the unanchored C4 alternatives.

Detection removes an initial frontmatter fence, directives, then detection
comments before returning the first match. It does not validate a diagram's
grammar. Explicit `flowchart-elk` changes the returned owned config's `layout`
to `elk`; disabling large features instead lets the flowchart-v2 predicate
match. Input text and configuration are preserved. Unknown-diagram errors retain
the exact cleaned UTF-16 text, including lone surrogates.

Host detectors may be registered with an optional opaque loader route. This
models lookup/presence without executing JS dynamic imports or claiming native
renderers for those ids. Actual loader execution, `diagramAPI` styles/utils,
family parser/renderer execution and application integration remain separate boundaries.
The host owns callback captures and must bound their work/side effects. Registry
copies have independent registration vectors with shared callback handles.

The official oracle executes original modules and orchestration with only the
renderer-registration sink observed: 8,084 detection cases and 39 registry
steps cover preprocessing, Unicode, keyword boundaries, large-feature selection,
numeric-key ordering, overwrite/removal, config writes, errors, and registration
during a dispatch snapshot. Native tests also cover ownership, isolated
registries and budgets. Defaults bound source to 4 Mi UTF-16 units, registrations
to 1,024 and total id/route text to 64 Ki units; configuration uses `ConfigLimits`
before copying and after callbacks. Native restrictions are mapping configuration
inputs and no Object.prototype ids. Builtin registration is transactional on
budget failure; detection failures expose no partial configuration. External
callback side effects cannot be rolled back. These explicit native restrictions
are separate from the original successful-dispatch observations.

The first portable module implements the official front-matter fence scanner,
diagram-detection comment stripping, and preprocessing comment cleanup. It uses
UTF-16 strings so positions and even unpaired code units retain JavaScript
string semantics. Returned matches own their strings. The two comment functions
have deliberately different behavior and must not be collapsed into one.

The same module now ports `cleanupText` and `removeDirectives` as independent
UTF-16 operations. Normalization maps CR/CRLF to LF before the official lexical
HTML-attribute quote rewrite. Directive removal matches the released regex,
including permissive unclosed matches and its word-argument alternative. These
are not HTML sanitization or a complete preprocessing API. Configuration must
be extracted before directive removal; the family renderers do not call these
operations or silently discard configuration. The editor's source is untouched.

The native frontmatter boundary now parses YAML configuration trees and extracts
title/displayMode/config using the original truthiness and string conversions.
`parse_config_yaml` exposes owned null/bool/binary64/UTF-16/list/map values;
`extract_front_matter` dedents only the captured fence indent and preserves the
remaining diagram text. Config values, including unknown keys and truthy scalar
config roots, are preserved for the next layer to validate. No settings are
applied or silently discarded at this boundary.

YAML grammar uses yaml-cpp 0.9.0 at commit
`56e3bb550c91fd7005566f19c079cb7a503223cf`, fetched into the ignored build tree.
A small checked-in parser patch fixes UTF-8 encoding of `\\N`/`\\_` and rejects
the non-YAML `\\'` escape inside double quotes. Tag scanning rejects punctuation
attached to a non-verbatim tag (`!!str}`), while preserving whitespace-separated
empty nodes (`!!str }`), empty non-specific null nodes, and verbatim tags.
Empty explicit sequence/map nodes are emitted as collections, distinct from
incorrectly tagged empty quoted strings. Flow separators remain valid at column
zero after a line break; a comma still cannot begin a block value. Schema resolution separately
follows the pinned upstream js-yaml **4.3.0** `JSON_SCHEMA`, which is more
permissive than strict JSON (for example, hex integers and `.inf` are supported).
Both dependencies are MIT licensed; the resolver's notice is retained in
`third_party/mermaid-cpp/JS-YAML-LICENSE` and yaml-cpp retains its fetched license.

The native boundary explicitly rejects cyclic aliases, malformed UTF-16 input,
and surrogate or out-of-range Unicode escapes, including adjacent `\\uD83D`
and `\\uDE00` escapes. A literal supplementary character or `\\U0001F600` works.
This is stricter than js-yaml; the oracle records these as explicit divergences
alongside the actual official results. It is not unrestricted YAML parity.
Default budgets are 1 Mi UTF-16 source units, depth 64, 100,000 materialized
values and 4 Mi expanded string/key units. Anchor copies and alias expansions
consume those budgets; depth has a hard ceiling of 64. Duplicate/coerced keys,
unknown tags and multiple YAML documents fail explicitly. Merge keys have no
special meaning in this schema. Aliases produce independent owned values.

The directive boundary now ports isolated `detectDirective` calls with the
all/init/wrap filters used by the common frontend. It preserves the original
quote replacement, bare-word string arguments, strict JSON arguments, duplicate
key overwrite, numeric overflow/underflow and UTF-16 surrogate code units.
The singleton/array result is normalized to an owned vector. No match retains
the transformed source as its type; malformed selected JSON returns one absent
type/null argument entry and discards previous matches. Filtering happens before
argument parsing. The shared lexical scanner also drives `remove_directives`.

Independent native calls start from offset zero. The composed frontend uses an
explicit request-local cursor for the init/wrap sequence, without leaking the
upstream global regexp state across requests. Default safety limits are 1 Mi UTF-16 source units, depth 64
(also a hard ceiling), 100,000 values, 4 Mi decoded key/string/type units and
10,000 scanned directives, including filtered matches. Budgets are cumulative
within a call, including overwritten JSON properties, and fail explicitly.
These raw values are not automatically sanitized or applied to a renderer.

The configuration primitives now expose two separate, bounded operations:
`sanitize_directive_config` implements the default-key whitelist, dictionary
color/icon patterns, CSS brace checks and theme-variable filtering;
`sanitize_secure_config` removes protected site keys and the original recursive
tag/data-URL patterns. These reproduce the pinned policies, not a general CSS
or HTML sanitizer. Both return owned results without mutating inputs.

`assign_config_with_depth` preserves the original depth-two merge semantics:
object/scalar type conflicts do not clobber, null source fields are skipped
above the depth boundary, arrays union primitives with SameValueZero, and
objects at depth zero use shallow assignment. Separate owned objects remain
distinct array entries. Inputs cannot encode JS object aliases, array holes or
custom array properties; operations that would create the latter two fail
explicitly. Shallow assignment that would invoke JS's inherited `__proto__`
setter is also rejected. The fixture retains the official result alongside
each restriction.

`resolve_config_appearance` selects `theme`, `look` and `layout` from the
directive sum, saved directives in reverse order, initialize delta and pinned
defaults. Diagram-specific values precede root values within each layer, invalid
theme/look names fall through, and existing section fields stay synchronized.
It includes the official diagram aliases and theme registry, including the
literal `"null"` sentinel. It operates on already-sanitized, merged layers;
it does not implement the full config.ts session lifecycle or derive colors.

All configuration operations preflight the owned inputs before copying. Default
limits are depth 64 (also a hard ceiling), 100,000 values, 4 Mi UTF-16 key/string
units and 1,000,000 work steps; array-union comparisons consume work steps.
Inputs collectively and output separately obey the same size bounds. Manually
constructed values with inactive string/list/map payloads are rejected before
copying, so hidden storage cannot evade these bounds.
Failures never publish a partial result. Config schema keys and appearance
defaults are generated from the original schema/AJV pipeline and defaultConfig,
including keys contributed by the original theme implementation. JavaScript and
AJV are oracle tools only; they are not native runtime dependencies.

The shared render entry connects prepared configuration and derived theme
variables to Packet/Pie; additional family adapters remain to be implemented.
The isolated default/dark derivation API is described below. Theme name
precedence and variable derivation do not establish native rendering parity.
The entry oracle checks the configuration actually consumed by both families.

The Packet component ports the official grammar, metadata value conversion,
contiguous field validation, splitting into rows, and SVG geometry. It supports
the Packet dimension, bit-order and sizing options, with default or explicitly
resolved Packet styles. The raw parser and renderer are deliberately separate
from MermaidAPI's frontmatter, directives, entity handling and sanitization.

The other diagram families and the Folia replacement remain separate work. No application
switch or fallback has been added. The old Rust dependency remains the active
application dependency until the native pipeline is accepted. The current
library is a foundation for the requested replacement, not that replacement.

The Pie core now includes its raw grammar, database validation and first-label
deduplication, the D3 sector/arc specialization, filtering, percentage labels,
five legend positions and title-driven viewBox expansion. Layout requires
explicit measured legend/title widths. `folia.mermaid.pie_svg` emits the family
SVG using an explicit resolved theme, including palette cycling, donut paths,
highlight/hover styles, title expansion and all five legend positions.
`folia.platform.mermaid_pie::render_measured_pie` joins DirectWrite measurement
to that renderer using the same font family and sizes for measurement and SVG.
The source entry supplies shared configuration/themes; Folia application integration remains pending.

The low-level measurement API requires one installed font-family name, not a CSS fallback
list. Missing families, invalid UTF-16, invalid sizes and DirectWrite failures
return errors; there is no character-count estimate. Glyph fallback is supplied
by the OS. SVG output accepts literal named/hex/RGB/HSL theme colors, not CSS paint
servers or arbitrary CSS expressions. Text and attributes are escaped as XML.
The raw family renderer accepts display text; commonDb/MermaidAPI sanitization
must be supplied by the shared frontend's family adapter, not silently duplicated here.

Packet has explicit native safety boundaries: non-finite bit counts and more
than 10,000 rows produce an error. Upstream can silently truncate oversized
fields at that row count. Invalid dimensions, invalid UTF-16, and characters
forbidden in XML 1.0 also produce an error at the relevant stage. The renderer
rebuilds layout from the document and configuration instead of trusting a
mutable cached row array.

## Verification

The source-text oracle directly imports the unmodified official TypeScript
functions in Node 24. It verifies source hashes before producing or checking
862 independent observations, including ECMAScript whitespace, UTF-16 offsets,
surrogates, CR/LF/LS/PS, malformed fences, directives and generated combinations.
The TSV is test data: seven fields encode a case name, input, both comment
outputs, match length/presence, indent and body. String fields contain four hex
digits per UTF-16 code unit; `-` denotes an empty string or an absent match.

```powershell
git -c core.autocrlf=false clone --branch mermaid@12.2.0 --depth 1 https://github.com/mermaid-js/mermaid.git .reference/mermaid-js
node tools/mermaid-cpp/source-text-oracle.mjs --check
node tools/mermaid-cpp/preprocess-text-oracle.mjs --check
node tools/mermaid-cpp/frontmatter-oracle.mjs --check
node tools/mermaid-cpp/directive-oracle.mjs --check
pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
node tools/mermaid-cpp/packet-oracle.mjs --check
node tools/mermaid-cpp/config-oracle.mjs --check
node tools/mermaid-cpp/frontend-oracle.mjs --check
node tools/mermaid-cpp/packet-svg-oracle.mjs --check
node tools/mermaid-cpp/pie-oracle.mjs --check
node tools/mermaid-cpp/pie-geometry-oracle.mjs --check
node tools/mermaid-cpp/pie-layout-oracle.mjs --check
node tools/mermaid-cpp/pie-svg-oracle.mjs --check
node tools/mermaid-cpp/theme-oracle.mjs --check
node tools/mermaid-cpp/generate-themes.mjs --check
git submodule update --init third_party/ut
cmake -G Ninja -S tools/mermaid-cpp -B build/mermaid-cpp -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mermaid-cpp --parallel 4 --target FoliaMermaidTests FoliaMermaidTextProbe FoliaMermaidPlatformTests
$env:FOLIA_MERMAID_SVG_OUTPUT = Join-Path $pwd 'build/mermaid-cpp/pie-svg'
ctest --test-dir build/mermaid-cpp --output-on-failure
node tools/mermaid-cpp/pie-svg-oracle.mjs --compare build/mermaid-cpp/pie-svg
node tools/mermaid-cpp/browser-text-probe.mjs
build/mermaid-cpp/FoliaMermaidTextProbe.exe build/mermaid-cpp/browser-text.tsv build/mermaid-cpp/native-text.tsv
node tools/mermaid-cpp/compare-text-probe.mjs
```

Use an x64 MSVC developer environment and CMake 4.3.5, matching the CI job and
the repository's `import std;` gate. The standalone build avoids unrelated
MathJax generation, WinUI restoration and Cargo. Native tests also exercise
large adversarial documents and ownership after the input has been destroyed.
Source observations alone are not evidence that the C++ implementation passes.

The directive oracle executes the pinned original `detectDirective` body and
regular expressions on 3,279 observations (1,093 inputs, three filters),
including malformed JSON, lexical alternatives, quotes, Unicode, comments,
line endings, duplicate keys, non-finite numbers and error recovery boundaries.
Native tests separately cover ownership, aggregate budgets, depth ceilings and
adversarial source sizes. The separate configuration oracle checks 2,940
sanitizer, merge and appearance observations, including explicit dense-tree
restrictions, plus the generated 720-key registry and appearance defaults.
Successful raw detection is not permission to apply a setting.

`detect_init_config` ports the original detectInit directive-to-diagram mapping.
One directive retains its raw arguments; multiple directives sanitize and merge
with assignWithDepth first. A caller-owned detector is invoked once on unchanged
source whenever the resulting arguments are truthy, even without a `config`
property. Errors propagate; only `flowchart-v2` maps to `flowchart`. This callback
boundary does not implement the diagram registry or config.ts session lifecycle.
Prototype setter effects fail explicitly; primitive setter arguments are ignored
and ordinary own `__proto__` data remains data. Results are owned and optional
(absent models JavaScript undefined), with directive and config resource bounds.

`clean_and_merge_config` ports `merge({}, defaults, data)` independently of
assignWithDepth. Arrays merge by index, primitives replace nested values, root
primitives are ignored and recursive `__proto__` writes are skipped. The oracle
pins es-toolkit 1.45.1, matching the official full Mermaid bundle; the newer
transitive core-package dependency has different array-null behavior. Internal
undefined values, holes and array properties survive intermediate merges; a
final result outside the dense ConfigValue tree is rejected rather than silently
normalized. Array-like conversions and array length coercions follow the original
plain-data behavior. Owned inputs exclude functions, symbols, custom prototypes,
cycles and shared identities. Input/output and cumulative intermediate nodes/text
are bounded; no JavaScript runtime or shared mutable config is introduced.
The original bodies provide 1,746 init mapping observations and 1,210 clean merge
observations (the latter cross-checked against the official full bundle), plus
native ownership, input preservation and resource-limit tests.

`derive_theme_variables` resolves the pinned default and dark themes from an
optional owned override mapping. It preserves the constructor/update/override
order (including default's extra constructor update), JavaScript short-circuit
and nullish choices, and final shallow object overrides. Unknown data keys are
retained; undefined generated fields are omitted as in JSON serialization.
The assignment functions are generated as ordinary C++ from the hash-locked
official AST, checked in and reproducibly checked in CI. This is neither a
static theme table nor a JavaScript runtime. Khroma 2.1.0 arithmetic, parsing,
color-name data, and decimal formatting are ported independently; the official
runtime supplies 1,218 theme observations and 2,412 color observations. The
theme oracle cross-checks the original source against the official bundle and
observes actual color-function arguments to classify non-string restrictions.

The native theme boundary accepts mapping overrides and literal string colors.
It explicitly rejects mutable Khroma channel objects, prototype/method-key
replacement, color strings over 4,096 UTF-16 units, and THEME_COLOR_LIMIT values
outside integer 0..12. Fixtures retain the corresponding official outcomes.
Input and final output use ConfigLimits, and cumulative intermediate assignment
trees have the same size limits. All operations return owned values or errors,
without modifying the input. CSS variables/currentColor require browser context
and are not resolved here. The render entry consumes these derived values;
application theme integration remains separate work.

The configuration session is an explicit `ConfigSession` instance with owned
site/current/initialize data and saved directives. It ports the original
`config.ts` command order: `setConfig` is temporary, directive replay uses the
current site's secure list, reset clears directives and scope, and a changed
theme derives variables from the original initialize overrides. Default/dark
are the supported derived themes. Other known theme names fail when derivation
is requested, including a diagram's scoped default; unknown names and the
`null` sentinel retain the original no-derivation behavior. `updateSiteConfig`
does not independently derive a theme. Deprecated-diagram warnings are exposed
as a once-per-session flag, without a process-global logger.

The session oracle executes the complete original module in fresh VM scopes.
Its 101 scenarios / 640 steps compare full site/current/user JSON-data state and
warning counts. Lossless recursive deltas and interned strings reduce fixture
size without hashes or selected-field sampling. Generated native defaults are
the original defaults' data projection: undefined properties and family font
callbacks are omitted. The session never invokes those callbacks; diagram
family integration remains separate. A generated registry of the 25 callback
slots rejects container overrides there, since omitting a callable would
otherwise change the original type-sensitive merge. Primitive overrides still
follow the original behavior. Source getters' mutation aliases and
partially applied failures are deliberately replaced with independent owned
values and transactional errors.
Getter copies retain the original `assignWithDepth` behavior (including skipped
null fields and primitive array union), rather than exposing raw stored trees.

Session command inputs are mappings. Arrays contain primitives; nested input
containers stop at depth two (root zero), before `assignWithDepth` could retain
shared deeper objects. Theme names are primitive values, secure lists contain
strings, and `__proto__` input fields are rejected. Existing theme/sanitizer/merge
restrictions still apply. Limits bound aggregate retained state, command input
and cumulative intermediate tree inspections; each delegated primitive also
has its own work limit. Saved directives have a hard ceiling of 256. Failed
commands preserve all state and warning flags. No singleton, diagram detector,
renderer wiring, application dependency switch or general JS object model is
introduced by this boundary.

The preprocessing oracle isolates and executes the original pinned
`cleanupText` and `removeDirectives` function bodies, stripping only TypeScript
syntax and the export keyword and supplying the original `directiveRegex`.
It checks 1,221 cases independently and in execution order, including malformed
tags/directives, all line-terminator kinds, Unicode and lone surrogates. Native
tests consume the same UTF-16 fixture and separately check large adversarial
inputs and owned results. It does not claim YAML/config or sanitizer coverage.

The frontmatter oracle executes the original extractor with the exact locked
js-yaml schema on 515 cases. Typed snapshots preserve negative zero, non-finite
numbers and UTF-16 text. It checks metadata coercion, block/flow YAML, aliases,
empty explicit tags, duplicate keys, Unicode/escape boundaries and large-number
rounding. The 94 boundary regression cases include valid empty tags with
space/tab/LF/CRLF separators, comma/nested/anchor/verbatim forms, and paired
invalid attached punctuation, plus ordinary flow and block comma boundaries.
Native tests also check resource budgets, cycles, independent alias
ownership and a large numeric scalar. The 12 Unicode-boundary cases assert
explicit native rejection while retaining upstream's permissive output. This
does not establish directive/config-precedence/theme or application integration.

The PR workflow defines four Windows jobs: standalone and root-project builds in
Debug and Release. Root jobs also build and run `FoliaTests` and
`FoliaWindowsEditorTests`, exercising the CMake module dependency wiring and
core test exclusions. Both modes retain the source oracles, native Mermaid and
platform suites, SVG comparisons and same-host text measurements. This does not
build the WinUI shell or establish packaged application/DPI acceptance.

The Packet oracles execute the official published parser and Mermaid renderer
with JSDOM. The exact package versions and transitive integrity hashes are
locked under `tools/mermaid-cpp/oracle`; these packages are test tools, not
native runtime dependencies. Raw-parser, layout, renderer and full MermaidAPI
observations are kept distinct so that frontend preprocessing does not become
an incorrect expectation for a lower-level component.

Pie verification separates 291 parser/database observations, 86 real D3
geometry cases (618 sectors), and 40 real Mermaid renderer layouts with
explicitly injected measurement inputs. The last set tests layout arithmetic;
it does not validate fonts or native/browser text-measurement equivalence.
Paths are compared exactly; trigonometric centroids allow a 1e-12 tolerance
because V8 and the platform CRT can differ in their final binary64 bits.

Pie SVG checks run the actual pinned official family renderer and stylesheet on
80 inputs. They compare 78 finite native SVG DOMs and reject the two official
empty-left outputs with non-finite coordinates. Checks cover full family style
declarations, palette order after filtering, all legend placements, showData,
donut geometry, highlight/hover, Unicode and XML escaping. Titles are supplied
at the same display-text boundary on both sides; this is not a test of the
unported commonDb sanitization frontend.

Real font validation runs Edge and DirectWrite on the same Windows machine,
using Arial, Segoe UI and Times New Roman at 17/25 SVG units. The 96 cases
include long titles, bidi, combining marks, CJK, emoji and SVG whitespace.
Native measurement uses the union of glyph ink bounds and the advance interval.
It is not pixel-identical to Chromium's hinted glyph bounds: local maximum
advance error was 0.03076171875 units and bounds error 2.14892578125 units.
The regression ceilings are 0.0625 and 2.5 respectively **for this corpus**;
they are not universal error guarantees for arbitrary fonts/text/sizes. No
string-specific correction is applied. CI preserves raw observations, the
comparison summary, generated SVGs and test executables as artifacts.

The browser uses an isolated ignored profile. Set `FOLIA_BROWSER` to another
Chromium executable to probe it. Browser code and oracle packages are test
dependencies only; the native runtime does not execute JavaScript. These checks
do not establish WinUI integration, arbitrary-font parity or DPI acceptance.

The original source's current functions are the oracle, including behavior not
covered by its own legacy-regex examples. In particular, detection comment
stripping consumes to LF even when a comment contains CR, LS or PS. Changing
that to the JavaScript `.` regex interpretation changes the released function.
