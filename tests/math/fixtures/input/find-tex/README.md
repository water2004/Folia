# FindTeX reference inputs and outputs

The `source` tables hold all 22 independent MathJax inputs and 59 ProtoItem
outputs; `find-tex-source.cjs --write-fixture` regenerates this pair. The
`retained` tables preserve all 56 earlier scan inputs and their 60 outputs
without generating new expected results. Both corpora run through the same
native finder assertions, including the repeated-call cursor checks.

Case records retain the exact UTF-16 string lists, inline/display delimiter
pairs, three option flags and expected output count. Explicit counts plus
`|` separators preserve empty lists and empty strings. Literal pipes are
encoded as `\u007c`, and other escaped UTF-16 units use `\uHHHH`. Output
records retain case/row indices, all three strings, all original offsets and
explicit display mode (`escaped`, `inline_math` or `display_math`).

The reader rejects changed row order, missing records and invalid modes; it
does not interpret or normalize expected math text.
