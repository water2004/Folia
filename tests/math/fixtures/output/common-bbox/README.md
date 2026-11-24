# common-bbox reference values

The 36 ordered records preserve all fields from the original compiled
fixture. Their source is `common-bbox-source.cjs --write-fixture`. Ordinary
native tests load the table directly and retain their original assertions.

Columns are declared in the header. UTF-16 and byte fields use reversible
`\uHHHH` and `\xHH` escapes. Each double is encoded as its exact 16-digit
IEEE-754 binary64 pattern, preserving signed zero, infinity and NaN. Ordered
double lists use commas with explicit counts; booleans use `0`/`1`.
