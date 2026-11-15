# Mermaid math reference checks

These tools run upstream JavaScript only as an independent development oracle.
Folia's renderer remains C++ and uses its existing native MathJax library. No
browser, Node runtime, new font copy, or cache is added to the application.

## The two upstream versions have different roles

Folia's graph baseline remains Mermaid **12.2.0**, commit
`1fb7869866dd619168d840291c9c54b4f129758b`. That release uses **KaTeX**, normally
through browser MathML. Its `common.ts` math detector recognizes non-greedy
`$$...$$` pairs without crossing a real newline; the expression is typeset in
display mode. Shared HTML labels use centered flex rows. The sequence renderer
has its own math path.

The official **Mermaid 13.0.0-alpha.3** math plugin actually uses **MathJax 4.1.3**.
`capture-official.mjs` runs that unmodified plugin and complete diagram renderer,
including mixed text, Markdown, matrix, multiple formula, edge, group, sequence,
class, state, mindmap, color, error, and literal-delimiter cases. It uses the
official `output.cssVariables: false` export option so librsvg receives literal
paint values. SVG and PNG files are both retained.

The v13 graph renderer and its default TeX font differ from Folia's v12 graph
renderer and existing NewCM math font. Consequently, v13 diagram coordinates are
**not** an assertion that a Folia graph must have identical coordinates. Its
plugin output is evidence of official MathJax integration semantics and visual
rendering. The exact native formula comparison instead runs official MathJax
4.1.3 with the **same committed NewCM profile** as Folia.

## Install the locked development dependencies

From the repository root:

```sh
npm ci --prefix tools/mathjax-bundle --ignore-scripts
pnpm --dir tools/mermaid-cpp/oracle install --frozen-lockfile --ignore-scripts
npm ci --prefix tools/mermaid-cpp/math-oracle --ignore-scripts
```

`source-lock.json` pins the committed profile, dependency locks, the executable
MathJax/font module trees, and the official plugin and label sources. The new
oracle verifies these before rendering. Its profile uses the same package
registrations and dynamic font loader as `tools/mathjax-bundle/entry.cjs`, then
selects the fragment host's display overflow policy (`display: true`,
`displayOverflow: 'overflow'`, inline breaking disabled). Ordinary document math
keeps its original profile and cache behavior.

The historical `docs/math-port/scope.json` records a profile SHA that does not
match the committed `entry.cjs`, including the version present when that scope
was first committed. That historical gate is not changed, skipped, or described
as passing here. This feature has a separate reviewed lock for the actual
committed profile (`0e7e985517dbf8055a33595e6638746901442d9bec652792903dca52b7409893`).

## Exact native SVG and bitmap comparison

Build the production `FoliaNativeMath` target with the repository's C++23 module
toolchain. Then run:

```sh
node tools/mermaid-cpp/math-oracle/capture-mathjax.mjs
python tools/mermaid-cpp/math-oracle/capture-native.py \
  --library build/mermaid-math-runtime/libFoliaNativeMath.so
node tools/mermaid-cpp/math-oracle/compare-fragments.mjs \
  build/mermaid-math/mathjax-newcm/report.json \
  build/mermaid-math/native/report.json
node tools/mermaid-cpp/math-oracle/capture-official.mjs
```

On Windows, pass the built `FoliaNativeMath.dll`. A Linux toolchain installed
outside the system search path may need its LLVM library directory in
`LD_LIBRARY_PATH` when Python loads the shared library.

`capture-native.py` calls the real production C ABI with UTF-16 input. Its report
records the corpus and shared-library SHA-256 and refuses a binary that changes
during capture. It does not implement a renderer. The upstream capture observes
MathJax's actual completed root box, since its printed SVG `viewBox` is rounded
and cannot recover full-precision metrics.

`compare-fragments.mjs` checks:

- CSS-pixel width, height, and baseline, with `1e-9` tolerance for floating-point
  arithmetic; graph geometry tolerances are not changed.
- Ordered SVG geometry, glyph paths, transforms, and paint. Attribute order,
  numeric serialization, accessibility/TeX annotations, scoped resource IDs,
  root embedding metadata, and empty non-painting groups are normalized. No
  nonempty drawing element, glyph, or paint is removed.
- Exact RGBA pixels using the same locked sharp/librsvg rasterizer and the same
  2× SVG host canvas. There is no image registration, filtering, or fuzzy pixel
  threshold. Even the empty expression is rendered in a real SVG host canvas.

The 22-case corpus covers fractions, sums, integrals, matrices, aligned equations,
cases, `\nabla`, delimiters, text, explicit colors, cancel marks, blackboard
letters, chemistry, 12/16/24-pixel fonts, overflow, empty input, invalid input,
and class attributes. The first completed native verification matched all
22 metric records, all 22 drawing trees, and every pixel of all 22 bitmaps.
Artifacts and machine-readable results are written under
`build/mermaid-math/{mathjax-newcm,native,comparison,official-v13}`.

## Stock v12 browser observations

In an environment that permits launching a browser:

```sh
FOLIA_BROWSER=/path/to/chromium \
  node tools/mermaid-cpp/math-oracle/capture-v12-browser.mjs
```

This captures stock v12 KaTeX SVG, actual DOM boxes/styles, and browser
screenshots. It does not inject a MathJax shim or claim v12 has a built-in
MathJax provider. In the implementation environment Chromium could not create
its process socket (`EPERM`), and permission escalation was rejected. These
browser observations therefore remain a separate reproducible check, not a
completed browser or Windows visual acceptance claim.

## Relevant v12 label routes

| Family | Math label route |
| --- | --- |
| Flowchart, Agentflow | Shared HTML shape, edge, and cluster labels |
| Sequence | Explicit `hasKatex` actor/message/note/control-label path |
| Class | `shapeUtil.addText`; math forces HTML for class compartments |
| State | Shared HTML shapes, notes, clusters, and edge labels |
| Block | `renderHelpers` → shared shapes and edges |
| ER | `erBox` titles/attributes and shared edge labels |
| Requirement | `requirementBox` text and shared edge labels |
| Mindmap | `svgDraw` → `createText`, controlled by `htmlLabels` |
| Use Case | Shared use-case shapes, actors, tables, boundaries, and edge labels |
| Swimlane | Flowchart labels and the swimlane cluster's HTML label path |
| Kanban | Renderer forces `htmlLabels: false` |
| Architecture | Each `createText` call explicitly uses SVG text |
| C4 nodes | `c4LabelHelper` and node adapter explicitly use SVG text |

The implementation preserves the relevant HTML-label switches. A generic text
measurement callback must never interpret math unless the corresponding paint
path also emits the typed math label.
