# unicode Source observations

`tools/math-port/unicode-source.cjs --write-fixture` captures the pinned
MathJax implementation and regenerates these external records. Native tests
read the committed data without running JavaScript. Every original scalar,
name, input, snapshot, error and ordered record is retained.

| Table | Records |
| --- | ---: |
| cases | 51 |
| creator-input | 1 |
| creator-requests | 1 |
| recovery | 6 |
| registry | 6 |

Columns are declared in each header. UTF-16 text uses reversible `\uHHHH`
escapes for individual units, including NUL and lone surrogates. Byte fields
use `\xHH`; group indices are decimal integers. Empty fields are significant.
The typed reader checks the exact record and column counts.
