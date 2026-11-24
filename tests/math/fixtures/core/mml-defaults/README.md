# MML default layers from the pinned Source snapshot

These frozen reference tables replace `mml_defaults_source_snapshot.hpp`.
`nodes.tsv` preserves all 43 names, attribute-store flags and original entry
ranges in order; `node-entries.tsv` retains 318 node defaults, and
`global-entries.tsv` retains 28 global defaults. Together they retain all 475
elements of the five original arrays as 389 records.

Names and string values preserve exact UTF-16 units with `\uHHHH` escapes.
Values retain explicit `null`, `boolean`, `number` or `string` tags; numbers use
16-digit IEEE-754 binary64 bits. Empty strings and explicit null stay distinct.
The typed reader owns every referenced string and exposes the same native
`AttributeDefaultValue` variant used by the existing assertions. No native
result is used to regenerate these expected values.
