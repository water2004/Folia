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

The Packet component ports the official grammar, metadata value conversion,
contiguous field validation, splitting into rows, and SVG geometry. It supports
the Packet dimension, bit-order and sizing options. Its SVG currently uses the
default Packet styles. The raw parser and renderer are deliberately separate
from MermaidAPI's frontmatter, directives, entity handling and sanitization.

Full YAML/configuration processing, diagram detection, shared themes, the other
diagram families and the Folia replacement remain separate work. No application
switch or fallback has been added. The old Rust dependency remains the active
application dependency until the native pipeline is accepted. The current
library is a foundation for the requested replacement, not that replacement.

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
pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
node tools/mermaid-cpp/packet-oracle.mjs --check
node tools/mermaid-cpp/packet-svg-oracle.mjs --check
git submodule update --init third_party/ut
cmake -G Ninja -S tools/mermaid-cpp -B build/mermaid-cpp -DCMAKE_BUILD_TYPE=Debug
cmake --build build/mermaid-cpp --parallel 4 --target FoliaMermaidTests
ctest --test-dir build/mermaid-cpp --output-on-failure
```

Use an x64 MSVC developer environment and CMake 4.3.5, matching the CI job and
the repository's `import std;` gate. The standalone build avoids unrelated
MathJax generation, WinUI restoration and Cargo. Native tests also exercise
large adversarial documents and ownership after the input has been destroyed.
Source observations alone are not evidence that the C++ implementation passes.

The Packet oracles execute the official published parser and Mermaid renderer
with JSDOM. The exact package versions and transitive integrity hashes are
locked under `tools/mermaid-cpp/oracle`; these packages are test tools, not
native runtime dependencies. Raw-parser, layout, renderer and full MermaidAPI
observations are kept distinct so that frontend preprocessing does not become
an incorrect expectation for a lower-level component.

The original source's current functions are the oracle, including behavior not
covered by its own legacy-regex examples. In particular, detection comment
stripping consumes to LF even when a comment contains CR, LS or PS. Changing
that to the JavaScript `.` regex interpretation changes the released function.
