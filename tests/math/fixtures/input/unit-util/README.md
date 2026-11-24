# unit-util reference values

These frozen tables preserve the original compiled Source corpus; no native
result is used to generate expectations. All ordered fields, original optional
values and exact floating-point patterns remain available to the same tests.

| Table | Records |
| --- | ---: |
| defaults | 9 |
| formats | 23 |
| matches | 29 |
| trims | 37 |

Each table declares its columns. Optional values use explicit `0`/`1`
presence fields so absence remains distinct from zero, NaN or an empty string.
UTF-16 and byte fields use reversible `\uHHHH` and `\xHH` escapes; doubles use
exact 16-digit IEEE-754 binary64 bit patterns, including signed zero, infinity
and NaN. Native value structures are constructed only after validation.
