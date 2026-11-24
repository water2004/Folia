# SVG evidence comparison

The acceptance runner is Python. Node.js is only needed to regenerate official
Mermaid captures; it is not needed to compare saved captures or run comparator
tests. `observe-svg.js` is a browser capture function: it reads actual glyph
bounds, computed paints and accessibility text. It contains no acceptance
decisions and is never used to estimate text dimensions.

Install the optional comparison dependencies with:

```sh
python -m pip install -r tools/mermaid-cpp/regression/requirements.txt
python tools/mermaid-cpp/regression/compare-svg.py --self-test
```

All 74 checks from the original JavaScript comparator are registered individually
by `../tests/test_comparison.py`. Their exact SVG, glyph, viewport and raster
inputs are in `../tests/fixtures/`. Missing or extra strokes, text, paints,
opacity, visibility and unsupported text effects continue to fail. Pixel
comparison still uses the 1.5 px geometry neighborhood, 2x raster scale and
2-channel rounding budget. The local ink checks run on both white and black
backgrounds; there is no global image error allowance.

The implementation is split by responsibility:

| Module | Responsibility |
| --- | --- |
| `corpus.py` | Complete corpus, provenance and case identifiers |
| `reports.py` | Report coverage and exact source/SVG hashes |
| `geometry.py` | Viewports and one-to-one visible grapheme matching |
| `pixels.py` | RGBA and local ink comparisons |
| `styles.py` | CSS cascade and inherited SVG presentation |
| `svg.py` | Complete measurement-free Packet and Info checks |
| `browser.py` | Real browser capture and comparison orchestration |

The browser path requires Playwright and an installed browser selected with
`--browser` or `FOLIA_BROWSER`. On Linux, Info and Packet use fontconfig to
verify the actual installed font file and index without pretending to measure
text through a synthetic DOM. Every other family still requires official
browser evidence and actual browser raster capture. Missing evidence is a
failure, never a skip.

## Historical family captures

`../../compare-family.py FAMILY original.json native-svg-directory` compares
the existing per-family capture formats. Shared checks live in `family_checks.py`;
`family_simple.py` covers geometric drawings; `family_graphs.py` covers State,
ER, Requirement, Sequence and Class; `family_labels.py` covers rich and portable
labels; `family_structures.py` covers compound and force layouts; and
`family_flowchart.py` covers shapes, markers and sampled routes. Each family keeps its original paint, text,
route, marker and coordinate rules, including stricter Class geometry limits.
RoughJS random handles are accepted only when they still describe the required
straight segments. Sequence now rejects a missing route-point array instead of
letting an empty loop silently pass.
Agentflow shape samples and Flowchart route samples likewise fail when missing.
Both original Mindmap modes run: strict identical-box layouts and the native
measurement path with its original shape, paint, connected-route and viewport checks.

The capture functions in `../../oracle/observations/` only read browser state.
Official generators and Python's Playwright driver share these exact observers;
comparison decisions and process orchestration belong to Python. Run the
externalized acceptance and rejection cases with:

```sh
python -m unittest discover -s tools/mermaid-cpp/regression/tests -q
```

## Exact Canvas evidence

ZenUML retains the pinned `@napi-rs/canvas` 1.1.1 Skia font and raster backend.
`../../oracle/capture-skia-samples.mjs` only generates actual font measurements
and PNG/raw-RGBA samples. It does not launch the native renderer or decide whether
a native result passes. `canvas_capture.py` owns the native JSON protocol and
saves every request and sample with source, corpus, binary and byte hashes.

The stages can run separately:

```sh
python tools/mermaid-cpp/regression/capture-native-canvas.py --probe=build/path/FoliaMermaidRegressionProbe --official=build/official --output=build/native
python tools/mermaid-cpp/regression/compare-zenuml-canvas.py --official=build/official --observations=build/native/native-observations.json --output=build/native
```

The first stage needs Node only to generate the real reference-backend samples;
the second stage is entirely Python and checks all cases, text, viewports and
every transparent RGBA channel at zero pixel tolerance. The original `--probe`
option remains as a convenience that runs capture before the offline comparison.
Process timeouts, malformed responses, abnormal exits, missing glyph samples,
changed hashes and missing coverage are failures. The committed tiny Canvas
fixture contains real captured font metrics and raster bytes for negative tests.
