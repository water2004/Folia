# Operator and pattern reference data

These tables retain the original frozen Source observations. Loading them does
not derive expected values from Folia's operator dictionary or pattern matcher.

| Table | Rows | Values |
| --- | ---: | --- |
| `entries.tsv` | 1,252 | Form, text, spacing, TeX class, property slice |
| `properties.tsv` | 944 | Every original property row, including original ordering |
| `named.tsv` | 27 | Named operator definitions and property slices |
| `named-properties.tsv` | 28 | Named-operator property observations |
| `ranges.tsv` | 50 | Unicode bounds, class, kind, optional variant |
| `spacing.tsv` | 8 | Both spacing components |
| `queries.tsv` | 1,480 | Frozen lookups and range boundaries |
| `masks.tsv` | 30 | Complete 65,536-unit masks for the original six shapes and five patterns |
| `retained-values.tsv` | 5 | Regex source, flags, length bounds, ordered ranges/members, mixed products |
| `retained-samples.tsv` | 255 | Additional frozen pattern observations |
| `independent-truths.tsv` | 40 | Original independent truth observations |

UTF-16 fields preserve exact units; bytes use reversible `\xHH` escapes.
Spacing fields store the exact 16-hex-digit IEEE-754 binary64 representation.
TeX classes and patterns use their semantic names. An optional maximum is
`null` or a decimal integer. The retained ranges use comma-separated `first:last`
pairs, members use comma-separated decimal integers, and both carry explicit
counts. No range expansion or expected-mask calculation occurs in the loader.

All 4,119 records were compared field-for-field with the compiler-decoded
original fixtures. The native tests retain their full-domain iteration, ordered
property comparisons, and every original assertion.
