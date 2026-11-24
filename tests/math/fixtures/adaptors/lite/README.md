# Lite adaptor reference data

`tools/math-port/lite-source.cjs` captures the pinned MathJax Lite implementation.
Its source/runtime hash gates and observations are unchanged; `--write-fixture`
now writes these tables, while ordinary native tests read them directly.

The 41 cases retain every input and complete observation JSON. Separate tables
retain 9 selectors, 14 ordered mutation observations, 4 ordered entity loads,
the element metric, the text-receiver error and the original two count values.
All 71 records are preserved. Each table declares its ordered columns.

UTF-16 fields encode individual units with `\uHHHH`, including NUL and lone
surrogates; `*_binary64` fields contain the exact 16-digit IEEE-754 bit pattern.
Kinds use readable stable names, booleans use `0`/`1`, and indices/counts are
decimal. The reader validates column and record counts. Native test serializers
still produce the same complete JSON for comparison with the Source results.
