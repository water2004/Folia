# Text and Unicode reference data

The tables contain the original observations as data. `text_fixtures.hpp` decodes
UTF-16 units without normalizing strings, NULs, or isolated surrogates. Empty
fields are intentional. Integers are decimal; booleans/bit flags retain their
original numeric values. Tables preserve source order and reject wrong row or
column counts when loaded.

| Tables | Records | Coverage |
| --- | ---: | --- |
| `tex-error-focused.tsv` | 14 | Original focused format examples and their descriptive names |
| `tex-error-focused-arguments.tsv` | 147 | Per-case argument order, indexed by case and argument |
| `tex-error-exhaustive.tsv` | 1,981 | Original pinned format/message corpus |
| `tex-error-exhaustive-arguments.tsv` | 10 | The original normalized arguments for the exhaustive corpus |
| `lowercase-mappings.tsv` | 1,460 | All non-identity lower-case mappings from the frozen Node Unicode 16.0 corpus |
| `lowercase-contexts.tsv` | 1,162 | Complete Unicode-domain ranges and sigma-context flags |
| `lowercase-examples.tsv` | 474 | Original string, context, and isolated-surrogate observations |

The exhaustive error source evidence path and SHA-256 remain in the table header.
The Unicode case observations were already frozen reference data; this migration
read their original C++ values and did not query the host Unicode implementation.
The tests continue to visit the full 0–0x10ffff domain, including surrogate units.
All 5,091 case records and 157 argument rows were compared with the compiler-decoded
original values during migration.

## UTF input and RegExp folding

`input-utf16.tsv` retains all 112 input/text-encoder records, including optional
code points, iteration sequences and unsigned byte vectors. `input-decode.tsv`
retains 68 valid byte inputs with exact decoded text/byte offsets;
`input-invalid.tsv` retains 60 invalid inputs and reported byte offsets.
Comma-separated integer vectors carry explicit counts; `null` distinguishes
missing code points from code point zero.

`regexp-case-groups.tsv` retains all 1,141 original BMP equivalence groups and
all four slots of each member/index/cursor array, including unused padding.
The tests still check all 2,307 active members and scan the complete BMP.
