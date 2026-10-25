# Third-party notices

## Eclipse Layout Kernel / elkjs 0.9.3

`src/elk_cycle.cpp`, `src/elk_layering.cpp`, `src/elk_long_edges.cpp` and their
module interfaces port the original ELK DepthFirstCycleBreaker, flat
NetworkSimplexLayerer and LongEdgeSplitter.
These files are licensed under
EPL-2.0, not the Mermaid MIT license. The full license is in `ELK-LICENSE`.
Original source: https://github.com/kieler/elkjs, package `elkjs` 0.9.3,
`lib/elk-worker.js`. Exact package/source hashes are recorded under `elkCycle`
in `docs/mermaid-cpp/source-lock.json`. This algorithm slice is not yet a
complete ELK layout engine or the public Class/State render path.

Quadrant parsing, fixed-canvas geometry and SVG output follow the pinned
Mermaid quadrant-chart sources under the MIT license reproduced in `LICENSE`.
Radar parsing, polar geometry and its closed Catmull-Rom curves follow the
pinned Mermaid Radar sources under the same license; source hashes are locked.

Flowchart display-line adaptation follows Mermaid 12.2.0's labelHelper,
createText, createLabel, nonMarkdownToHTML and decodeEntities. Their original
source hashes are recorded in docs/mermaid-cpp/source-lock.json; the Mermaid
MIT license is reproduced in LICENSE. Native line measurement and wrapping use
DirectWrite, without an HTML or JavaScript runtime.

Mermaid-derived code is covered by the MIT license in `LICENSE`.
Flowchart's classic node geometry follows the pinned Mermaid rendering-elements
shapes and line/polygon intersection functions.

## D3 Shape 3.2.0 and D3 Path 3.1.0

Pie geometry specializes the unpadded, square-cornered paths used by Mermaid
from d3-shape (`pie.js`, `arc.js`, `math.js`, `path.js`) and d3-path (`path.js`).
Flowchart's spline writer follows d3-shape `src/curve/basis.js`.
Source hashes and versions are recorded in `docs/mermaid-cpp/source-lock.json`.
The complete ISC notice is also preserved in `src/pie_geometry.cpp`.

Copyright 2010-2022 Mike Bostock (d3-shape)

Copyright 2015-2022 Mike Bostock (d3-path)

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
USE OR PERFORMANCE OF THIS SOFTWARE.

## Langium 4.4.0

The Packet quoted-string conversion follows Langium's `ValueConverter`
(`packages/langium/src/parser/value-converter.ts`). The JavaScript package is
used only by the independent test oracle; it is not a runtime dependency of
the native library.

Copyright 2021 TypeFox GmbH

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Dagre graph layout

The native rank pipeline follows dagre-d3-es 7.0.14's Dagre and Graphlib
algorithms, including DFS cycle removal, nesting constraints, feasible-tree
construction, network simplex, edge normalization, compound crossing reduction
and Brandes-Kopf coordinate assignment. Its MIT notices are reproduced in
`DAGRE-LICENSE`; the original source hashes are in
`docs/mermaid-cpp/source-lock.json`. Dagre's JavaScript package is test-only.

## Khroma color primitives

The native theme color operations and generated named-color table are derived
from Khroma 2.1.0 by Fabio Spampinato, under the MIT license reproduced in
`KHROMA-LICENSE`. Source and distribution hashes are recorded in
`docs/mermaid-cpp/source-lock.json`. Khroma is an oracle-only Node dependency;
the shipped native library contains no JavaScript runtime.

## es-toolkit compatibility merge

The native cleanAndMerge operation follows es-toolkit 1.45.1's compatibility
merge, pinned to the dependency embedded in the official Mermaid 12.2.0 bundle.
The MIT notice, including the OpenJS/Lodash compatibility attribution, is
reproduced in `ES-TOOLKIT-LICENSE`. The package and transitive helper sources are
hash-locked in `docs/mermaid-cpp/source-lock.json` and used only by the oracle.
