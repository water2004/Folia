# Math scene reference observations

These tables contain the complete pinned MathJax 4.1.3 observations previously
embedded in the C++ `common_scene.inc` and `canonical_scene_modes_source.hpp`
fixtures. Native tests load them lazily through `support/scene_fixtures.hpp`.
No JavaScript, browser, font regeneration or network access is needed to read
the fixtures during a test run.

| File | Records | Meaning |
| --- | ---: | --- |
| `common.tsv` | 150 | Original 75 ordered inputs and 75 focused scene cases |
| `canonical-cases.tsv` | 75 | Ordered canonical TeX inputs and host metrics |
| `canonical-modes.tsv` | 150 | Two independent observations per canonical input: no cache and local cache |

Input, expected SVG, error text, root attributes and geometry retain their exact
bytes. Byte fields use printable ASCII and `\xHH` escapes. Binary64 fields are
exactly 16 hexadecimal digits; they preserve the complete floating-point
representation without decimal re-rounding. Boolean fields are `0` or `1`.

Canonical mode rows are ordered by zero-based case index and then mode index.
The loader rejects missing, duplicated or reordered modes. Cache-prefix lists
carry an explicit count; their escaped elements are separated by `|`, with a
literal pipe encoded as `\x7c`. The explicit count distinguishes an empty list
from a list containing an empty prefix.

The original source-lock and session-provenance hashes are retained in the
table headers. Regenerate or verify against the pinned official implementation
with `tools/math-port/common-scene-source.cjs` and
`tools/math-port/source-canonical-scene-modes.cjs`, using their existing
`--write-fixture` option only after reviewing a changed official observation.
Those generators emit data directly; they no longer generate C++ literals.
