# SVG paint comparison evidence

Three existing display cases (`array-dashed`, `strut-only`, `link`) retain the
real MathJax 4.1.3 / NewCM output and production native C ABI output captured
during the test-system refactor. They cover table lines/frame/dashes, invisible
content, glyphs and link paint. The complete 18-input, three-mode corpus remains
in `tools/math-port/svg-paint/cases.json`; no input was replaced or removed.

`expected.json` contains the unmodified former `check.mjs` results for these
records, including every per-primitive removal count, paint observation and PNG
hash. `reference.json` is a subset of actual independently captured MathJax
output, bounding boxes, font metric and stylesheet, with the exact source lock.
`native.json` contains the corresponding native output. Subsetting changes only
coverage and the subset corpus SHA-256, not any SVG or measurement.

`samples/` holds original Sharp 0.35.5 raw RGBA and PNG bytes for the common
canvas, bare/explicit CSS controls and every removed primitive. Requests and
responses are bound by hashes in `samples.jsonl` and `raster-observations.json`.
The Python tests replay this evidence without Node, inspect the original
checks, and deliberately corrupt inputs, paint, coverage and pixels. Raster
size, backend, exact alpha/RGB values and the zero-pixel tolerance are retained.
