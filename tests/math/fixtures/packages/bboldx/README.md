# bboldx reference observations

These 180 ordered mapping records were exported from the existing pinned
MathJax 4.1.3 fixture; no Folia output was used to derive expected values.
Four font outcomes and all three argument slots (including unused empty values) are preserved for every row.

TSV column names are in each file. UTF-16 uses `\uXXXX` for code units and
doubles use the exact 16 hexadecimal IEEE-754 binary64 bits. The native adapter
rejects missing rows, unknown callback names and invalid argument counts.
Regenerate only with the pinned `tools/math-port/tex-bboldx-source.cjs` oracle.
