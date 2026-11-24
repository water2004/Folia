# Entity source fixtures

These tables preserve all 2,205 observations from the pinned MathJax Entity
source. `entities-source.cjs --write-fixture` captures the independent source
and regenerates them; the ordinary tests read the checked-in tables directly.

| Table | Records | Contents |
| --- | ---: | --- |
| rows | 2,128 | Base and shard names and exact UTF-16 values |
| shards | 29 | Shard names and resulting table sizes |
| overrides | 3 | Override, reload and removal observations |
| retries | 3 | Retry triggers, inputs, output and ordered loaded files |
| numeric-retries | 2 | Numeric retry inputs and ordered loaded files |
| numbers | 21 | Numeric values and RangeError outcomes |
| translations | 19 | Module state, inputs, outputs, errors and ordered loads |

Columns are declared in each header. UTF-16 fields encode individual units as
`\uHHHH` when needed, preserving lone surrogates and NUL. Ordered string lists
use a separate element count and `|` separator; literal pipes are encoded as
`\u007c`, and empty elements remain significant. Boolean fields are `0`/`1`.
The typed reader checks column and row counts before exposing the records.
