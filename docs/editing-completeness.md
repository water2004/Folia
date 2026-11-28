# Editing command completeness

This document records command-level acceptance rules shared by rendered and
source views. The block tree is authoritative; editable leaves own exact local
Markdown source and a lossless inline CST; one reversible history spans both
views.

## Command outcomes

A command that can edit selected content must either apply the requested source
or tree change, or move the selection to a meaningful next editable position.
True document edges, already-satisfied formatting, and impossible indentation
may be no-ops and must not create a revision or history entry. Rejected
operations leave document, selection, and history unchanged.

Each edit is one transaction containing its local source and tree changes,
boundary creation, and resulting selection. Undo/redo restores exact source,
structure, and selection. Normal local commands do not parse or serialize the
whole document or diff the whole tree.

## Interaction boundaries

- Navigation and deletion use shared owner order and Unicode grapheme
  boundaries. Raw-block movement maps through visible body text, excluding
  hidden fence syntax.
- Tab behavior follows the active editing surface: tables navigate cells,
  list items indent/outdent, and source/raw selections indent physical lines.
  Structural table commands require an unambiguous cell target.
- Atomic objects expose before/after positions. Insertions and replacements
  obtain a valid editable destination while preserving surrounding structure.
- Inline formatting, links, images, and math edit block-local source and
  reparse that owner's CST. Block formatting and container commands edit the
  unified tree.
- Host input adapters provide native gestures, clipboard/IME, focus, and
  measured geometry; portable core policy does not prove native event delivery.

## Verification

Test accepted and rejected outcomes, exact save/reload, CST flattening, valid
selection offsets, one-step undo/redo, and unchanged neighboring identities.
Include forward and reversed selections, nested containers, raw/atomic/table
boundaries, LF/CRLF/CR, non-BMP and combining text, source/rendered mode changes,
and randomized edit/history sequences. Run the relevant portable and Windows
gates described in [`tests/README.md`](../tests/README.md); report native UI
validation separately from portable results.
