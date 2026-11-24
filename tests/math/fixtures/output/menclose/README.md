# Menclose layout reference data

All 56 original layout rows (28 notations for each of two boxes) retain their
case and notation names, width/height/depth and top/right/bottom/left extents.
The seven numeric columns preserve the 392 original binary64 bit patterns as
16-digit hexadecimal strings. The reader exposes the original ordered rows.

`common-menclose-geometry-source.cjs --write-fixture` captures the pinned
MathJax implementation and writes this table. The existing
`common-source/menclose-geometry-source-expected.json` remains the complete
Source geometry/SVG witness. Its established JSON persistence boundary and
the former C++ serializer normalize negative zero to zero; the TSV generator
preserves that boundary without recalculating expected geometry.
