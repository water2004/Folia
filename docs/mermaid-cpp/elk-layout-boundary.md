# ELK layered layout boundary

The pinned Mermaid commit `1fb7869866dd619168d840291c9c54b4f129758b`
uses `elkjs` 0.9.3. Its npm package contains the readable GWT-generated
`lib/elk-worker.js`, including the original Eclipse ELK layered processors.
The package is EPL-2.0; the derived native cycle-selection files retain that
license separately from Mermaid's MIT adapter. See `ELK-LICENSE` and the
`elkCycle` source-lock entry. No JavaScript engine is part of the native library.

## Consumers and default options

`packages/mermaid/src/diagrams/class/classRenderer-v3-unified.ts` and
`packages/mermaid/src/diagrams/state/stateRenderer-v3-unified.ts` both obtain
the configured layout through `getRegisteredLayoutAlgorithm(layout)`, then
call the common renderer with measured nodes, edges, labels and groups.
The global default layout is ELK; State also declares ELK in its own defaults.
Class's already implemented explicit Dagre route does not satisfy this default.

`packages/mermaid/src/rendering-util/layout-algorithms/elk/render.ts` defines
the actual adapter contract. Its default preset is:

| Setting | Value |
| --- | --- |
| Algorithm for `layout: elk` | `elk.layered` |
| Layering | `NETWORK_SIMPLEX` |
| Root and container node placement | `BRANDES_KOEPF` |
| Brandes-Koepf alignment | `BALANCED` |
| Cycle breaking | `DEPTH_FIRST` |
| Root hierarchy handling | `INCLUDE_CHILDREN` |
| Root spacing base | 40 |
| Port surrounding margin | 12 on every side |

The named `legacy` preset instead uses `GREEDY` cycle breaking and `NONE`
alignment. `modelOrder` and `depthFirst` select network-simplex root placement.
These names must not be conflated with the default preset. Explicit relevant
`config.elk` values override presets; the original adapter also sets model-order,
edge-merging and edge-routing options. The graph's actual direction replaces
the root's initial `DOWN`. Container padding, title constraints and measured
label boxes remain part of the adapter, not a reason to modify the algorithm.

The whole ELK algorithm catalogue, experimental option-sweep hooks and
unrelated layout algorithms are not prerequisites for the initial Class/State
layered package. Required source configuration remains a separate acceptance
gate and must not silently select Dagre.

## Native processor delivered here

`folia.mermaid.elk_cycle` ports the feedback-edge selection in the original
`DepthFirstCycleBreaker` (`$dfs_3` / `$process_57` in the locked worker).
Its input retains node order, each node's port order and each port's outgoing
edge order. It visits nodes with no incoming edges first, then remaining nodes
in model order; self-loops do not participate. Feedback edges are collected
without mutating adjacency during traversal, preserving their reversal order.

`tools/mermaid-cpp/elk-cycle-oracle.mjs` loads the entire unchanged original
worker, constructs its real `LGraph`, `LNode`, `LPort` and `LEdge` objects, and
executes the original processor. A wrapper records calls to its original edge
reversal method. The seven cases cover a class chain/cycle, a state-style entry
into a cycle, port ordering, parallel return edges, disconnected components and
self-loops, plus the empty graph. The native test compares exact edge indices
and order with these observations. This is processor evidence, not measured
layout or SVG evidence.

The native selection function deliberately returns a reversal plan. The later
layered graph adapter must apply ELK's original reversal semantics: swap ports,
adapt collector ports, swap head/tail edge-label placement, toggle the reversed
property and reverse bend points. The original pipeline restores reversed edges
after routing. This slice is not connected to `render_entry` and does not make
ELK a supported public render path.

## Next algorithm boundary

Continue with the actual layered graph and network-simplex layering package,
starting at `NetworkSimplexLayerer` (`$process_68` / `$initialize_4`) and its
`NGraph`, `NNode`, `NEdge`, `NetworkSimplex` helpers in the same locked worker.
Preserve component traversal/order, the minimum edge shortness weight of one,
unit edge delta, the thoroughness-derived iteration limit and previous-component
layer balancing. Use the same real-worker harness to observe layer assignment
for the Class/State representative graphs after cycle reversal. Reusing Dagre's
network-simplex implementation merely because its name matches is insufficient.

Node ordering, Brandes-Koepf placement, orthogonal routing, compound-graph
processors, measured label geometry and the common-renderer adapter remain
necessary after layering. Their absence is an implementation gap, not a missing
source/package blocker; the original worker is available and hash locked.

## Other retained families: source availability

This bounded source inventory establishes where the other independent original
algorithms can be obtained. It does not claim their native implementation or
extend their required feature lists.

| Consumer | Locally available original implementation | License |
| --- | --- | --- |
| Mindmap | `cytoscape-cose-bilkent` 4.1.0, `cose-base` 1.0.3, `layout-base` 1.0.2 | MIT |
| Architecture | `cytoscape-fcose` 2.2.0, `cose-base` 2.2.0, `layout-base` 2.0.1 | MIT |

These installed packages include their `src` files under the oracle's pnpm
directory. Mindmap's pinned `mindmapRenderer.ts` selects `cose-bilkent` as its
fallback; `rendering-util/layout-algorithms/cose-bilkent/cytoscape-setup.ts`
passes measured node dimensions and runs the original layout with `quality:
'proof'`, no animation and no style engine. Architecture's
`architectureRenderer.ts` calls fCoSE directly, also with `quality: 'proof'`,
and supplies seeded randomization, group-dependent edge lengths/elasticity and
alignment/relative-placement constraints. The different CoSE dependency
generations and these constraints must remain distinct.

Before establishing their layout oracles, resolve the known dependency-version
difference: the installed oracle has Cytoscape 3.34.4, while the pinned Mermaid
repository's pnpm lock resolves Cytoscape 3.34.0. This observation does not
invalidate unrelated accepted renderers. The next package should pin the
consumer's original adapter and necessary force-layout sources together, then
observe one representative measured diagram through each original pipeline.

### ZenUML package evidence

The pinned Mermaid lock resolves `@zenuml/core` 3.49.2. The exact public npm
archive is `https://registry.npmjs.org/@zenuml/core/-/core-3.49.2.tgz`.
Its registry SHA-512 integrity was verified on download:

```text
sha512-qeQMapuE0XETML2mRw2MIIrMCjcZBhRDSwW10//q8XQ+VyExzkAn39VCBwxIbbPdTBt6gJnaBBixF9A4hsIxcA==
```

The package license is MIT, copyright 2022 Peng Xiao. The distribution's
`dist/zenuml.esm.mjs.map` supplies all 457 `sourcesContent` entries, including
the generated `sequenceLexer.js` / `sequenceParser.js`, parser adapters,
`Coordinates`, `VerticalCoordinates`, `buildGeometry` and `renderToSvg`.
That source map has SHA-256:

```text
c090e83b377121186edaa097c6e402562f72a9a694319c7a57cf3d06b4eba061
```

Local static evidence is in ignored `build/mermaid-cpp/elk-boundary/`:
`zenuml-package-metadata.json`, `zenuml-core-3.49.2.tgz`, and
`zenuml-package/package/{LICENSE,package.json,dist/zenuml.esm.mjs.map}`.
No downloaded ZenUML code was executed for this inventory.

The registry reports git head `213c239587ffa1f3dbb54d8b64468c3038ced55d`
at `mermaid-js/zenuml-core`; that commit's `package.json` identifies itself as
4.0.0, whereas the verified archive identifies itself as 3.49.2. Use the exact
archive and embedded source content as the initial reference rather than
assuming the reported repository head is source-identical.

Mermaid's `packages/mermaid-zenuml/src/zenumlRenderer.ts` removes the `zenuml`
prefix and calls `renderToSvg(code)`, then adopts its dimensions, viewBox and
inner SVG. The package's implementation runs parse, its own coordinate engines,
geometry construction and SVG composition; `WidthProviderOnCanvas` supplies
text measurements. The next native boundary is therefore the exact grammar /
generated parser and coordinate engine with a real native measurement adapter.
First lock the required parser/runtime and positioning sources from this
distribution and capture a representative call/return/fragment oracle. Porting
the entire surrounding React/DOM product is not required by this call path.
