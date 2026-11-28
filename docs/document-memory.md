# Large-document memory model

Folia keeps the unified block tree, each editable block's complete Markdown
source and lossless inline CST, and exact scroll geometry. Memory work reduces
duplicated indexes, per-record overhead, and off-screen visual payloads; it does
not discard authoritative source or approximate text width.

## Ownership and lifetime

- The block tree and block-local source/CST remain authoritative. Rendered text,
  source projections, mappings, and layout objects are derived data.
- A prepared layout record retains measured geometry and the identifiers needed
  to find its source. DirectWrite layouts, line data, paint plans, and other
  visual payloads can be released off screen and rebuilt when needed.
- Source offset maps may use compact run representations when that is smaller;
  lookup and iteration must preserve exact offsets, duplicate positions, and
  affinity behavior.
- Ordinary edits update affected source owners and projections. Initial parsing,
  explicit save/search, and genuine document-wide style or width changes may
  still require work proportional to document size.
- Measurements that affect layout use the platform text measurer. Core-only
  memory probes do not establish Windows working-set, DirectWrite, or GPU costs.

The phased memory probe, inputs, and raw observations are maintained with its
owner in [`tools/document-scalability`](../tools/document-scalability/README.md).
Regenerate summaries from the raw records there; do not copy benchmark snapshots
into this architecture note.
