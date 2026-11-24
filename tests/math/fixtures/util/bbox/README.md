# primitive-bbox reference values

These frozen tables preserve the original compiled Source corpus; no native
result is used to generate expectations. All ordered fields, original optional
values and exact floating-point patterns remain available to the same tests.

| Table | Records |
| --- | ---: |
| definitions | 7 |
| expected | 46 |
| full-width | 1 |
| sides | 4 |
| steps | 32 |
| zero-setup | 1 |

Each table declares its columns. Optional values use explicit `0`/`1`
presence fields so absence remains distinct from zero, NaN or an empty string.
UTF-16 and byte fields use reversible `\uHHHH` and `\xHH` escapes; doubles use
exact 16-digit IEEE-754 binary64 bit patterns, including signed zero, infinity
and NaN. Native value structures are constructed only after validation.
