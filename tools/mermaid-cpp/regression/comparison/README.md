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
`family_simple.py` covers geometric drawings and `family_graphs.py` covers State,
ER, Requirement, Sequence and Class. Each family keeps its original paint, text,
route, marker and coordinate rules, including stricter Class geometry limits.
RoughJS random handles are accepted only when they still describe the required
straight segments. Sequence now rejects a missing route-point array instead of
letting an empty loop silently pass.

The capture functions in `../../oracle/observations/` only read browser state.
Official generators and Python's Playwright driver share these exact observers;
comparison decisions and process orchestration belong to Python. Run the
externalized acceptance and rejection cases with:

```sh
python -m unittest discover -s tools/mermaid-cpp/regression/tests -q
```
