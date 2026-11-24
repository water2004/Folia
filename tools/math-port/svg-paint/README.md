# Self-contained native Math SVG paint checks

This development oracle compares the production C++ Math SVG with pinned
MathJax 4.1.3 / NewCM **including the upstream stylesheet**. It requires no
Windows SDK or browser. JavaScript captures official MathJax expectations and
real raster samples. Python owns input completeness, native comparison,
negative controls, diagnostics and pass/fail status. Saved observations replay
without Node or installed MathJax packages.

Comparing two bare SVG strings misses paint that both renderers assume a host
stylesheet supplies. In particular, the old native array rules inherited the
root's zero stroke width, and an unpainted frame rectangle used its inherited
fill. The original MathJax SVG also depends on CSS for those elements. This
oracle keeps that distinction visible instead of treating bare Source SVG as a
complete picture.

## Run the production C ABI matrix

From the repository root, restore the existing locked development dependencies:

```sh
npm ci --prefix tools/mathjax-bundle --ignore-scripts
npm ci --prefix tools/mermaid-cpp/math-oracle --ignore-scripts
```

Build the `FoliaNativeMath` target with the repository's C++23 module toolchain.
Then pass the resulting shared library explicitly:

```sh
python3 tools/math-port/svg-paint/capture-native.py \
  --library build/math-svg-paint/libFoliaNativeMath.so \
  --output build/math-svg-paint/native
node tools/math-port/svg-paint/capture-source.mjs \
  --output build/math-svg-paint/source
python3 tools/math-port/svg-paint/check.py \
  --reference build/math-svg-paint/source/report.json \
  --native build/math-svg-paint/native/report.json \
  --output build/math-svg-paint/comparison
```

On Windows use the built DLL. A separately installed Linux LLVM / libc++
toolchain may require its normal `LD_LIBRARY_PATH` before Python loads the
library. Install the Python comparison dependencies from
`tools/mermaid-cpp/regression/requirements.txt`. Every command returns a failure
status when capture or verification fails; individual render failures remain in
the JSON report. The optional Sharp subprocess only renders the exact requested
SVG and returns its original RGBA/PNG bytes, size and backend version. It never
reads a native executable or decides test acceptance.

The resulting `raster-observations.json` and `samples/` bind every render request
and byte stream to the complete input reports. Replay the same checks offline:

```sh
python3 tools/math-port/svg-paint/check.py \
  --reference build/math-svg-paint/source/report.json \
  --native build/math-svg-paint/native/report.json \
  --observations build/math-svg-paint/comparison/raster-observations.json \
  --output build/math-svg-paint/replayed
```

Missing, reordered, corrupted or incompatible observations fail. The offline
stage verifies the captured source/profile lock against the committed lock and
profile files; the capture stage verifies all pinned upstream module bytes.

`cases.json` contains 18 TeX inputs in each of the actual inline, display and
standalone fragment C ABI modes. Coverage includes solid and dashed array
rules, frames, nested arrays, color, script sizing, the unchanged undefined
`\hlined` command, invisible struts, fraction and radical glyphs, enclosure
lines/curves/arrows, explicit CSS borders, links, error paint and glyph variants.
Four inline inputs naturally produce several SVG pieces. Every piece is checked
in order, and the full original host output is also retained. This is a paint
comparison of those pieces, not a browser line-wrapping test.

## Add actual direct MathML fixture outputs

The existing C++ Source regression tests can optionally export their actual
native output alongside the **unchanged original Source SVG fixtures**. The
export is disabled unless `FOLIA_MATH_SVG_PAINT_CAPTURE_DIR` is set. Run the
existing math test executable containing `common_table_native` and
`native_table_profile_source` with that variable, for example:

```sh
FOLIA_MATH_SVG_PAINT_CAPTURE_DIR="$PWD/build/math-svg-paint/mathml-fixtures" \
  build/math-svg-paint/FoliaMathSvgBridgeTests
python3 tools/math-port/svg-paint/check.py \
  --reference build/math-svg-paint/source/report.json \
  --native build/math-svg-paint/native/report.json \
  --mathml build/math-svg-paint/mathml-fixtures \
  --mathml-binary build/math-svg-paint/FoliaMathSvgBridgeTests \
  --output build/math-svg-paint/comparison
```

These are eight existing table fixtures plus fourteen existing table-profile
fixtures. They exercise real dotted rows, columns and frames, labeled table
viewports, nested percentage widths, stretches and both local and uncached font
resources. No Source fixture is rewritten. The native path constructs the
recorded compiled MathML nodes and uses Common `LayoutJob` and the production
SVG writer; it does not claim a native MathML parser or additional TeX package
coverage. Existing recorded measurement observations stay part of those strict
C++ tests.

The tool requires both complete fixture families, identifies each record as
`direct-mathml`, and records the fixture data and test executable hashes.
Percentage SVG widths use the fixture's actual CSS-pixel container width. Keep
the executable used for capture unchanged until comparison finishes.

## What is checked

The Source expectation is obtained independently from the locked upstream
modules' real `styleSheet()` and style registry. Its table rules have width 70,
no fill, dashed spacing 140, and round dotted caps with spacing `0,140`; glyph
`path[data-c]` and `use[data-c]` have the profile's default blacker width 3.
The registry also supplies error colors, link colors and viewport overflow.
`source-paint.mjs` captures the pinned table and glyph declarations; the Python
checker validates those same declarations before applying the projection.

For each SVG piece the tool saves these views:

- Original official output, with all original geometry and metadata.
- Official output in an SVG host with the **actual upstream stylesheet**.
- The same official output with no stylesheet, exposing its CSS dependencies.
- Official output with just the independently selected SVG paint declarations
  materialized into attributes, and no stylesheet.
- The actual native output in the same host, with no stylesheet.

The raster host replaces only the `mjx-container` selector tag with an outer
`svg[data-mathjax-host]` element carrying `jax="SVG"`. The formula remains its
direct SVG child. All CSS declarations and child/descendant relationships stay
intact, so host-dependent selectors for links and glyphs really match. Injecting
the stylesheet into an otherwise bare SVG would not test those rules.

Every case checks actual-CSS Source against the explicit Source projection with
**zero RGBA tolerance**. It separately compares that actual-CSS Source with the
CSS-free native output. The same sharp/librsvg build renders both sides at 2×
on a transparent canvas; there is no image registration, filtering, smoothing
adjustment or fuzzy threshold. Each original root viewport and every glyph,
transform and primitive remain unchanged. Only the outer test embedding uses
matched CSS-pixel dimensions and a small common margin.

The checker also verifies dimensions, numeric `viewBox` values and standalone
width/height/baseline (within `1e-9` CSS pixels). It examines paint on the actual
line, frame, glyph and table-viewport nodes, rather than accepting a matching
attribute on an ancestor. Removing each table rule and frame individually must
change visible official pixels, with the same affected pixel count in the
native image. Representative enclosure, link, error, glyph and table-viewport
content is likewise removed and rerendered. Strut-only controls must remain
transparent. Full-image exact comparison checks color and coverage as well as
those per-primitive visibility checks.

The C++ expected-side helper `tests/math/support/source_svg_paint.hpp` serves a
different purpose: it materializes the same independently pinned CSS values
while preserving all other Source bytes for strict serialization comparisons.
It preserves authored inline styles and foreign HTML, including raw text and
void elements. Neither helper derives expected paint from native output.

The upstream nonstandard `stroke-thickness` attribute does **not** make CSS use
a different line width. The strict Source helper rejects such unreviewed table
fixtures. Separate typed-Scene writer tests check resolved nondefault widths;
this oracle does not call those cases pixel-equivalent to upstream CSS.

## Reports and limits

`report.json` records all cases and parts, binary/corpus/fixture/Source hashes,
the verified profile and module lock, exact pixel differences, per-primitive
visibility results and actual sharp/librsvg/Cairo versions. Original native SVG
files from the C ABI capture are retained, along with the comparison SVGs and
PNGs. A failed case is never silently dropped.

This checks SVG paint and the existing native layout outputs using a real SVG
rasterizer. It does not certify Windows UI rendering, interactive HTML
tooltips, browser layout, application theme inheritance or browser-specific
text fallback. The ordinary Mermaid formula oracle reuses the same independent
Source paint projection; its ordered geometry/tree and exact pixel checks remain
separate from this broader paint matrix.
