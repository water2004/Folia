# Editing patches and shared history

The Editor owns one history timeline for rendered and source views. Source view
is a projection over the same document and does not own a second application
history. Switching views projects the selection; it does not recreate the Editor
or add a history entry.

## Patch responsibilities

- Transactions record reversible block-local text edits, tree edits, and
  document-property changes, together with selections before and after.
- Text patches validate the removed source. Replay restores exact source,
  structure, payloads, and selection; edited CST identity must not be replaced
  arbitrarily during undo.
- Source-view edits map to local source/tree operations. Reparse the affected
  region, expanding only when Markdown context changes beyond it. Preserve
  untouched block identities and exact source.
- Normal local edits and replay do not parse or serialize the whole document or
  compute a whole-tree transaction diff. Operations with genuine document-wide
  grammar dependencies may affect a wider region.
- The WinUI session routes rendered, source, search, and input commands through
  the shared core APIs.

## Acceptance

Cover edits in both views and across view switches, redo invalidation, empty and
invalid patches, history capacity, incomplete syntax, marker variants,
escapes/entities, links/images/math, all supported line endings, Unicode,
nested containers, tables, structural edits, search replacement, and randomized
edit/undo/redo sequences. Assert exact source and tree restoration, CST content,
selection, and rendered output. Keep source-view offsets at the presentation
boundary; rendered positions remain container ID plus block-local source offset
and affinity.

Use the focused core, platform, and application test instructions in
[`tests/README.md`](../tests/README.md). Do not treat portable core passes as
native WinUI/IME validation.
