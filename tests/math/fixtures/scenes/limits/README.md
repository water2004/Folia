# limits MathML observations

`observations.tsv` replaces the original compiled fixture without deriving any
expected value from Folia. Its three columns are `field`, `type`, and `value`.
Named field paths preserve member/sequence order; explicit `.count` and `.present`
fields preserve empty lists and absent optionals. Byte strings use `\xHH`, UTF-16
uses `\uHHHH`, and floating values use exact binary64 bits. `max` represents the
platform-independent maximum index sentinel. Enum ordinals follow the adapter's
declared enum; unknown values fail.

The reader owns backing strings and sequences for every returned view. Root
record counts, every path/type, and full consumption are checked. The migration
was verified by compiling the original aggregate observations, reading this
file, and comparing every field bit-for-bit, including complete SVG bytes.
Original source metadata is retained in `provenance.json`.

Regenerate with `tools/math-port/common-limits-source-witness.cjs` using the existing pinned
source locks. `math/support/limits_observations.hpp` contains the typed reader and test-facing schema; ordinary
native test execution does not invoke JavaScript.
