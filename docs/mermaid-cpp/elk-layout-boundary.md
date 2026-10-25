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
