# Third-Party Notices (`mermaid-cpp`)

`src/mermaid` ports behaviors and layout algorithms from the pinned upstream Mermaid 12.2.0 release (`1fb7869866dd619168d840291c9c54b4f129758b`) and its algorithm dependencies into native C++23 modules. Exact upstream versions and source file hashes are recorded in [`tools/mermaid-cpp/source-lock.json`](../../tools/mermaid-cpp/source-lock.json).

---

## 1. Mermaid (`mermaid-js/mermaid` 12.2.0)

Parser, configuration, theme derivation, diagram database models, layout adapters, and SVG painters derived from `mermaid-js/mermaid` are covered by the MIT License in [`LICENSE`](LICENSE) (Copyright (c) 2014–2022 Knut Sveidqvist).
The subgraph feedback edge orientation in `flowchart_elk.cpp` is derived from Mermaid's `subgraphFeedbackEdges.ts`. Directional scope selection in `flowchart_elk_scopes.inc` follows the same pinned ELK adapter's hierarchy policy.

---

## 2. Eclipse Layout Kernel (`elkjs` 0.9.3)

The native ELK layered and compound layout modules (`src/mermaid/layout/elk/elk_*.{ixx,cpp}`) are derived from `elkjs` 0.9.3 (`lib/elk-worker.js`, https://github.com/kieler/elkjs). These files are covered by the **Eclipse Public License 2.0 (EPL-2.0)**; see [`ELK-LICENSE`](ELK-LICENSE) for the full license text.
The shared connected-component traversal and row packing (`elk_components.*`),
self-loop processing (`elk_self_loops.*`), greedy switching
(`elk_greedy_switch.*`), shared crossing checks (`elk_order_*.inc`), and the
routing dependency cycle helper (`elk_routing_cycles.inc`) derive from that same worker
and are covered by EPL-2.0. Flowchart and State adapters reuse the shared component
implementation.

---

## 3. D3 Modules (`d3-shape`, `d3-path`, `d3-scale`, `d3-array`, `d3-interpolate`, `d3-hierarchy`, `d3-format`, `d3-time`, `d3-axis`, `d3-time-format`)

- Pie sector and arc geometry (`diagrams/pie/pie_geometry.{ixx,cpp}`) and Flowchart basis curves follow `d3-shape` 3.2.0 and `d3-path` 3.1.0.
- XY chart scales and ticks (`diagrams/xy/xy.{ixx,cpp}`) follow `d3-scale` 4.0.2, `d3-array` 3.2.4, and `d3-interpolate` 3.0.1.
- Treemap squarify layout and value formatting (`treemap.{ixx,cpp}`, `treemap_format.cpp`) follow `d3-hierarchy` 3.1.2 and `d3-format` 3.1.0.
- Gantt time intervals, ticks, and axis formatting (`diagrams/gantt/gantt_svg.{ixx,cpp}`) follow `d3-time` 3.1.0, `d3-axis` 3.0.0, and `d3-time-format` 4.1.0.

```text
Copyright 2010-2021 Mike Bostock (d3-scale, d3-interpolate, d3-hierarchy, d3-format, d3-axis, d3-time-format)
Copyright 2010-2022 Mike Bostock (d3-shape, d3-time)
Copyright 2010-2023 Mike Bostock (d3-array)
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
```

---

## 4. `d3-sankey` 0.12.3

Sankey node alignment, iterative relaxation/collision layout, and link positioning (`diagrams/sankey/sankey.{ixx,cpp}`) follow `d3-sankey` 0.12.3 under the BSD-3-Clause License; see [`D3-SANKEY-LICENSE`](D3-SANKEY-LICENSE).

---

## 5. Langium 4.4.0

Quoted-string value conversion in the Packet/Pie lexer follows Langium's `ValueConverter` (`packages/langium/src/parser/value-converter.ts`) under the MIT License:

```text
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
```

---

## 6. Dagre (`dagre-d3-es` 7.0.14)

The native Dagre ranking, ordering, compound nesting, and Brandes-Koepf coordinate assignment pipeline (`layout/dagre/dagre_rank.{ixx,cpp}`, `layout/dagre/dagre_order.inc`, `layout/dagre/dagre_position.inc`) follows `dagre-d3-es` 7.0.14 under the MIT License; see [`DAGRE-LICENSE`](DAGRE-LICENSE).

---

## 7. Khroma 2.1.0

Theme color manipulation primitives and the named-color table (`themes/theme_color.{ixx,cpp}`) are derived from Khroma 2.1.0 by Fabio Spampinato under the MIT License; see [`KHROMA-LICENSE`](KHROMA-LICENSE).

---

## 8. `es-toolkit` 1.45.1

Configuration `cleanAndMerge` compatibility merge semantics (`config/config_clean_merge.{ixx,cpp}`) follow `es-toolkit` 1.45.1 under the MIT License; see [`ES-TOOLKIT-LICENSE`](ES-TOOLKIT-LICENSE).

---

## 9. `js-yaml` 4.1.0

Frontmatter YAML schema resolution and scalar coercion rules (`frontend/frontmatter.{ixx,cpp}`, `config/config_yaml.cpp`) follow `js-yaml` 4.1.0 under the MIT License; see [`JS-YAML-LICENSE`](JS-YAML-LICENSE).

---

## 10. Day.js 1.11.21

Gantt civil-date parsing, ISO week calculation, and duration arithmetic (`diagrams/gantt/gantt.{ixx,cpp}`) follow Day.js 1.11.21 and its `customParseFormat`, `advancedFormat`, `isoWeek`, and `duration` plugins under the MIT License; see [`DAYJS-LICENSE`](DAYJS-LICENSE).

---

## 11. Cytoscape, CoSE-Bilkent, and fCoSE

- Mindmap tree force-directed layout (`layout/cose/cose_tree.{ixx,cpp}`) follows `cytoscape-cose-bilkent` 4.1.0, `cose-base` 1.0.3, `layout-base` 1.0.2, and `cytoscape` 3.34.0 under the MIT License; see [`COSE-LICENSE`](COSE-LICENSE) and [`CYTOSCAPE-LICENSE`](CYTOSCAPE-LICENSE).
- Architecture constrained force-directed layout (`diagrams/architecture/architecture_constraints.{ixx,cpp}`, `diagrams/architecture/architecture.{ixx,cpp}`) follows `cytoscape-fcose` 2.2.0, `cose-base` 2.2.0, and `layout-base` 2.0.1 under the MIT License; see [`FCOSE-LICENSE`](FCOSE-LICENSE).

---

## 12. RoughJS 4.6.6

Hand-drawn path helpers follow RoughJS 4.6.6. The path and hatch primitives
(`rough_hachure.{ixx,cpp}`, `rough_polygon.{ixx,cpp}`) also follow the bundled `hachure-fill` 0.5.2,
`path-data-parser` 0.1.0, `points-on-curve` 0.2.0, and `points-on-path` 0.2.1.
All are covered by the MIT License in [`ROUGHJS-LICENSE`](ROUGHJS-LICENSE).

---

## 13. ZenUML (`@zenuml/core` 3.49.2)

ZenUML sequence parsing, horizontal constraint solving, recursive block positioning, and SVG rendering (`zenuml.{ixx,cpp}`, `zenuml_svg.{ixx,cpp}`, `zenuml_assets.inc`, `zen_fragment_icons.inc`, `zenuml_comments.inc`) follow `@zenuml/core` 3.49.2 under the MIT License (Copyright (c) 2022 Peng Xiao); see [`ZENUML-LICENSE`](ZENUML-LICENSE).

---

## 14. `@upsetjs/venn.js` 2.0.0 and `fmin` 0.0.4

Venn circle layout and intersection geometry (`venn_layout.{ixx,cpp}`) follow
`@upsetjs/venn.js` 2.0.0 under the MIT License. The optimizer follows the
patched `fmin` 0.0.4 bundled in that release, under the BSD 3-Clause License.
See [`VENN-JS-LICENSE`](VENN-JS-LICENSE) and [`FMIN-LICENSE`](FMIN-LICENSE).

---

## 15. Marked 4.3.0 (embedded in ZenUML)

The code-span normalization in `zenuml_comments.inc` follows the Marked 4.3.0
parser embedded in the pinned ZenUML bundle. Its exact source-map contents are
locked separately from Mermaid's newer Markdown parser. Marked is covered by
the MIT License; see [`MARKED-LICENSE`](MARKED-LICENSE).
