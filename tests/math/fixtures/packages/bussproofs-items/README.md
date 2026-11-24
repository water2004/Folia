# bussproofs-items Source observations

`tools/math-port/tex-bussproofs-items-source.cjs --write-fixture` captures the pinned
MathJax implementation and regenerates these external records. Native tests
read the committed data without running JavaScript. Every original scalar,
name, input, snapshot, error and ordered record is retained.

| Table | Records |
| --- | ---: |
| errors | 6 |
| trees | 11 |

Columns are declared in each header. UTF-16 text uses reversible `\uHHHH`
escapes for individual units, including NUL and lone surrogates. Byte fields
use `\xHH`; group indices are decimal integers. Empty fields are significant.
The typed reader checks the exact record and column counts.
