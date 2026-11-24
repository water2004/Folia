# NewCM CSS metadata

`css-metadata.tsv` retains all 33 expected variant/family/italic/bold records from
`tests/math/fonts/font_css_metadata.cpp` at the test-refactor baseline. This is
reference expectation data, not a snapshot generated from Folia's font provider.
All fields and row order are preserved; the original native assertions remain
unchanged. Names are byte fields (`\xHH` escapes); flags are exactly `0` or `1`.
The reader requires all 33 rows and rejects malformed flags or columns.
