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
the non-YAML `\\'` escape inside double quotes. Schema resolution separately
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

The next frontend boundaries are directive argument interpretation, config merging and appearance
precedence (diagram-scoped settings can override globals), and default/dark
theme resolution. Each needs its own official runtime oracle before wiring a
shared entry point into the family renderers.

The Packet component ports the official grammar, metadata value conversion,
contiguous field validation, splitting into rows, and SVG geometry. It supports
the Packet dimension, bit-order and sizing options. Its SVG currently uses the
default Packet styles. The raw parser and renderer are deliberately separate
from MermaidAPI's frontmatter, directives, entity handling and sanitization.

Full configuration processing, diagram detection, shared themes, the other
diagram families and the Folia replacement remain separate work. No application
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
Folia application integration and shared theme/config resolution remain pending.

The platform API requires one installed font-family name, not a CSS fallback
list. Missing families, invalid UTF-16, invalid sizes and DirectWrite failures
return errors; there is no character-count estimate. Glyph fallback is supplied
by the OS. SVG output accepts literal named/hex theme colors, not CSS paint
servers or arbitrary CSS expressions. Text and attributes are escaped as XML.
The raw family renderer accepts display text; commonDb/MermaidAPI sanitization
must be applied by a future shared frontend, not silently duplicated here.

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
pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
node tools/mermaid-cpp/packet-oracle.mjs --check
node tools/mermaid-cpp/packet-svg-oracle.mjs --check
node tools/mermaid-cpp/pie-oracle.mjs --check
node tools/mermaid-cpp/pie-geometry-oracle.mjs --check
node tools/mermaid-cpp/pie-layout-oracle.mjs --check
node tools/mermaid-cpp/pie-svg-oracle.mjs --check
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

The preprocessing oracle isolates and executes the original pinned
`cleanupText` and `removeDirectives` function bodies, stripping only TypeScript
syntax and the export keyword and supplying the original `directiveRegex`.
It checks 1,221 cases independently and in execution order, including malformed
tags/directives, all line-terminator kinds, Unicode and lone surrogates. Native
tests consume the same UTF-16 fixture and separately check large adversarial
inputs and owned results. It does not claim YAML/config or sanitizer coverage.

The frontmatter oracle executes the original extractor with the exact locked
js-yaml schema on 421 cases. Typed snapshots preserve negative zero, non-finite
numbers and UTF-16 text. It checks metadata coercion, block/flow YAML, aliases,
empty explicit tags, duplicate keys, Unicode/escape boundaries and large-number
rounding. Native tests also check resource budgets, cycles, independent alias
ownership and a large numeric scalar. The 12 Unicode-boundary cases assert
explicit native rejection while retaining upstream's permissive output. This
does not establish directive/config-precedence/theme or application integration.

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
