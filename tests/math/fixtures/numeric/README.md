# Numeric reference data

The native tests read these tables at runtime. No Node.js runtime or fixture
generation is involved in a normal test run. The tables retain the complete
observations formerly compiled from `numeric_source_values.inc` and
`input_host_numeric_values.inc`; their values, row order, signed zero, NaNs,
UTF-16 units and byte strings are unchanged.

| File | Records | Observation |
| --- | ---: | --- |
| `input-number.tsv` | 1,013 | Numeric conversion of exact UTF-16 input |
| `input-parse-int.tsv` | 944 | Integer parsing with the specified radix |
| `number-format.tsv` | 6,412 | Shortest and fixed decimal output |
| `text-length.tsv` | 520 | Text lengths with explicit metric context |
| `number-length.tsv` | 88 | Numeric lengths with explicit metric context |
| `formatted-length.tsv` | 512 | Percent and em formatting |
| `pixel.tsv` | 1,024 | Pixel formatting, limits and scale |
| `unit.tsv` | 30 | Physical and relative units |
| `reduction.tsv` | 145 | Dense sum/max reductions, including NaNs |

Each file starts with a column header comment. Binary64 fields are exactly
16 hexadecimal digits, preserving the IEEE-754 representation. Boolean fields
are `0` or `1`. Input sequences for reductions are comma-separated binary64
fields; an empty field represents an empty sequence.

UTF-16 columns use printable ASCII and `\uHHHH` code-unit escapes. This keeps
isolated surrogates and NULs intact. Byte columns use printable ASCII and
`\xHH` escapes, preserving malformed UTF-8 where it is an intentional test
input. Literal backslashes and field/record separators are always escaped.
Fields must be decoded according to their declared type, not with a production
MathJax parser or a lossy Unicode conversion.

The original immutable data was imported at repository revision
`2ba389aa34a5c2f23500dc552b9fab9c434a0cdd`. Migration verification compared every
field in the compiler-decoded original arrays against the runtime readers.
